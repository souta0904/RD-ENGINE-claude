#include "RdPch.h"
#include "Editor/AIAssistant.h"
#include "Core/StringUtil.h"
#include "Editor/AssistantTools.h"
#include "Editor/Editor.h"
#include "Engine.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		constexpr int kMaxToolLoops = 25;
		constexpr const char* kSettingsPath = "UserSettings/AIAssistant.json";
		constexpr const char* kProviderLabels[] = { "Local (Ollama)", "Local (OpenAI compatible)", "Claude API" };

		constexpr const char* kSystemPrompt = R"(You are the built-in assistant of RD ENGINE, a C++/DirectX 12 game engine with a Unity-like editor.
You help the user edit the currently open scene by calling the provided tools, and you answer questions about the engine.

Scene model:
- A scene is a hierarchy of actors. Each actor has a transform and components (MeshRenderer, lights, colliders, Rigidbody, game scripts, ...).
- Coordinates: left-handed, +Y is up, +Z is forward, units are meters. Rotations in tool inputs and outputs are Euler angles in degrees, applied X then Y then Z.
- Built-in models for MeshRenderer: "Engine/Cube", "Engine/Sphere", "Engine/Plane", "Engine/Capsule" (unit size). Other assets live under "Assets/".
- Component properties are the same names the inspector shows (for example MeshRenderer has "Model" and a "Material" group). Call list_component_types to see every component type with its default properties before you set properties you are unsure about.
- References to other actors are written as {"$ref": actor_id}.

How to work:
- Read before you write: use get_scene (and get_actor for details) to learn ids and current values instead of guessing.
- Make the smallest set of changes that does what the user asked. Every change is recorded in the editor's undo history.
- When several independent changes are needed, call the tools for them in the same turn.
- After editing, reply with a short summary of what you changed. Reply in the user's language.)";

		// ツールからエディタを触る窓口
		class EditorToolHost final : public AssistantToolHost
		{
		public:
			explicit EditorToolHost(Editor& editor) : mEditor(editor) {}
			Scene& GetScene() override { return mEditor.GetScene(); }
			void PushCommand(std::unique_ptr<EditorCommand> command) override { mEditor.PushCommand(std::move(command)); }
			Actor* GetSelected() override { return mEditor.GetSelected(); }
			void Select(Actor* actor) override { mEditor.Select(actor); }
			bool IsPlaying() override { return mEditor.GetEngine().IsPlaying(); }

		private:
			Editor& mEditor;
		};
	}

	// 通信中の 1 リクエスト（通信スレッドと共有する）
	struct AIAssistant::PendingRequest
	{
		std::atomic<bool> done = false;
		std::atomic<bool> cancel = false;
		std::unique_ptr<AssistantProvider> provider;
		HttpResponse response;
	};

	struct AIAssistant::PendingModelList
	{
		std::atomic<bool> done = false;
		std::vector<std::string> models;
		std::string error;
	};

	AIAssistant::AIAssistant(Editor& editor) : mEditor(editor), mToolHost(std::make_unique<EditorToolHost>(editor))
	{
		Json saved;
		if (std::filesystem::exists(kSettingsPath) && LoadJsonFile(kSettingsPath, saved)) { mSettings.FromJson(saved); }
		mChat.push_back({ ChatEntry::Kind::Info, "シーンの編集を頼めます。例:「赤い箱を 5 個、半径 3m の円に並べて」" });
	}

	AIAssistant::~AIAssistant()
	{
		// 通信中のスレッドは切り離してあるので、結果を捨てるよう知らせるだけでよい
		if (mRequest) { mRequest->cancel = true; }
	}

	void AIAssistant::SaveSettings() const
	{
		std::error_code ec;
		std::filesystem::create_directories("UserSettings", ec);
		SaveJsonFile(kSettingsPath, mSettings.ToJson());
	}

	void AIAssistant::NewChat()
	{
		if (mRequest) { mRequest->cancel = true; mRequest.reset(); }
		mHistory.clear();
		mChat.clear();
		mChat.push_back({ ChatEntry::Kind::Info, "新しい会話を始めました。" });
		mLoopCount = 0;
	}

	void AIAssistant::RollbackToCleanState()
	{
		// 次に依頼を送れる形まで戻す: 最後が「ツールを呼ばずに終わった返事」になるまで外す
		//  （結果の付いていないツール呼び出しや、user が続く形は受け付けられない）
		while (!mHistory.empty())
		{
			const AssistantTurn& last = mHistory.back();
			if (last.role == AssistantTurn::Role::Assistant && last.toolCalls.empty()) { break; }
			mHistory.pop_back();
		}
	}

	// ==================================================
	// 会話
	// ==================================================
	void AIAssistant::Send(const std::string& text)
	{
		if (text.empty() || mRequest) { return; }
		mChat.push_back({ ChatEntry::Kind::User, text });
		// 選択中のアクターを添えておく（「これを赤くして」のような頼み方に答えられるように）
		AssistantTurn turn;
		turn.role = AssistantTurn::Role::User;
		turn.text = text;
		if (Actor* sel = mEditor.GetSelected())
		{
			turn.text += std::format("\n\n(Currently selected actor: id {} \"{}\")", sel->GetId(), sel->GetName());
		}
		mHistory.push_back(std::move(turn));
		mLoopCount = 0;
		StartRequest();
	}

	void AIAssistant::StartRequest()
	{
		auto request = std::make_shared<PendingRequest>();
		request->provider = CreateAssistantProvider(mSettings.provider);
		AssistantRequest http;
		std::string error;
		if (!request->provider->BuildRequest(mHistory, kSystemPrompt, AssistantTools::GetDefinitions(), mSettings, http, error))
		{
			mChat.push_back({ ChatEntry::Kind::Error, error });
			RollbackToCleanState();
			return;
		}
		mRequest = request;
		Log::Debug(LogCat::Editor, "AI request ({}): {} turns, {} bytes", kProviderLabels[int(mSettings.provider)], mHistory.size(), http.body.size());

		// 返事まで数十秒かかることもあるので専用のスレッドで待つ（JobSystem のワーカーを塞がない）
		bool local = mSettings.IsLocal();
		std::thread([request, http = std::move(http), local]
			{
				// ローカルは一度だけ。Claude は混雑（429 / 5xx）なら少し待って最大 2 回やり直す
				int attempts = local ? 1 : 3;
				for (int attempt = 0; attempt < attempts && !request->cancel; ++attempt)
				{
					request->response = HttpClient::Post(http.url, http.headers, http.body, 600000, &request->cancel);
					int s = request->response.status;
					bool retryable = s == 429 || s == 500 || s == 502 || s == 503 || s == 529;
					if (!retryable) { break; }
					int waitSec = 2 << attempt;
					if (!request->response.retryAfter.empty()) { waitSec = std::clamp(std::atoi(request->response.retryAfter.c_str()), 1, 60); }
					for (int i = 0; i < waitSec * 10 && !request->cancel; ++i) { Sleep(100); }
				}
				request->done = true;
			}).detach();
	}

	void AIAssistant::Update()
	{
		if (mModelList && mModelList->done)
		{
			mModels = std::move(mModelList->models);
			mModelListError = std::move(mModelList->error);
			mModelList.reset();
			// まだ選んでいなければ、最初のモデルを使う
			if (mSettings.localModel.empty() && !mModels.empty())
			{
				mSettings.localModel = mModels.front();
				SaveSettings();
			}
		}

		if (!mRequest || !mRequest->done) { return; }
		auto request = std::move(mRequest);
		if (request->cancel) { return; }
		AssistantReply reply;
		try { reply = request->provider->ParseResponse(request->response); }
		catch (const std::exception& e)
		{
			reply.error = std::string("応答を読めませんでした: ") + e.what();
		}
		HandleReply(reply);
	}

	void AIAssistant::HandleReply(const AssistantReply& reply)
	{
		mScrollToBottom = true;
		if (!reply.error.empty())
		{
			Log::Warning(LogCat::Editor, "AI request failed: {}", reply.error);
			mChat.push_back({ ChatEntry::Kind::Error, reply.error });
			RollbackToCleanState();
			return;
		}
		mInputTokens += reply.inputTokens;
		mOutputTokens += reply.outputTokens;
		mCacheReadTokens += reply.cacheReadTokens;

		mHistory.push_back(reply.turn);
		if (reply.usedFallback) { mChat.push_back({ ChatEntry::Kind::Info, "（別のモデルに切り替えて続けました）" }); }
		if (!reply.turn.text.empty()) { mChat.push_back({ ChatEntry::Kind::Assistant, reply.turn.text }); }

		switch (reply.stop)
		{
		case AssistantReply::Stop::Refusal:
			mChat.push_back({ ChatEntry::Kind::Error, "この依頼には答えられませんでした。言い方を変えてもう一度試してください。" });
			return;
		case AssistantReply::Stop::MaxTokens:
			mChat.push_back({ ChatEntry::Kind::Info, "応答が長すぎて途中で切れました。" });
			break;
		default:
			break;
		}
		if (reply.stop != AssistantReply::Stop::ToolUse || reply.turn.toolCalls.empty()) { return; }

		// ツールを実行して、結果をすべてまとめて返す
		AssistantTurn results;
		results.role = AssistantTurn::Role::ToolResults;
		for (const AssistantToolCall& call : reply.turn.toolCalls)
		{
			bool isError = false;
			std::string result;
			if (call.input.contains("__invalid_arguments"))
			{
				isError = true;
				result = "The arguments were not valid JSON. Call the tool again with a JSON object.";
			}
			else
			{
				result = AssistantTools::Run(*mToolHost, call.name, call.input, isError);
			}
			std::string summary = call.name;
			if (call.input.contains("name") && call.input["name"].is_string()) { summary += " \"" + call.input["name"].get<std::string>() + "\""; }
			if (call.input.contains("actor_id") && call.input["actor_id"].is_number()) { summary += std::format(" #{}", call.input["actor_id"].get<uint64_t>()); }
			if (isError) { summary += "  -> " + result; }
			mChat.push_back({ isError ? ChatEntry::Kind::Error : ChatEntry::Kind::Tool, summary });
			results.results.push_back({ call.id, call.name, result, isError });
		}
		mHistory.push_back(std::move(results));

		if (++mLoopCount >= kMaxToolLoops)
		{
			mChat.push_back({ ChatEntry::Kind::Error, "ツールの呼び出しが多すぎるので止めました。" });
			RollbackToCleanState();
			return;
		}
		StartRequest();
	}

	// ==================================================
	// 画面
	// ==================================================
	void AIAssistant::RefreshModels()
	{
		if (mModelList) { return; }
		auto list = std::make_shared<PendingModelList>();
		mModelList = list;
		AssistantSettings settings = mSettings;
		std::thread([list, settings]
			{
				list->models = ListLocalModels(settings, list->error);
				list->done = true;
			}).detach();
	}

	void AIAssistant::DrawSettings()
	{
		bool changed = false;
		int provider = int(mSettings.provider);
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::Combo("##provider", &provider, kProviderLabels, int(std::size(kProviderLabels))))
		{
			mSettings.provider = AssistantProviderKind(provider);
			mModels.clear();
			mModelListError.clear();
			mModelsRequested = false;
			changed = true;
		}

		if (mSettings.IsLocal())
		{
			// 開いたら一度だけ、サーバーにあるモデルを聞きに行く
			if (!mModelsRequested) { mModelsRequested = true; RefreshModels(); }

			std::string& url = mSettings.provider == AssistantProviderKind::Ollama ? mSettings.ollamaUrl : mSettings.openAIUrl;
			// 名前の欄の幅（いちばん長い "Context" に合わせる）
			const float labelWidth = ImGui::CalcTextSize("Context").x + ImGui::GetStyle().ItemInnerSpacing.x * 2.0f;
			ImGui::SetNextItemWidth(-labelWidth);
			if (ImGui::InputText("URL", &url, ImGuiInputTextFlags_EnterReturnsTrue)) { changed = true; RefreshModels(); }
			if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; }

			const float refreshWidth = ImGui::CalcTextSize("R").x + ImGui::GetStyle().FramePadding.x * 2.0f + ImGui::GetStyle().ItemSpacing.x;
			ImGui::SetNextItemWidth(-(labelWidth + refreshWidth));
			const char* preview = mSettings.localModel.empty() ? "(none)" : mSettings.localModel.c_str();
			if (ImGui::BeginCombo("##model", preview))
			{
				for (const auto& m : mModels)
				{
					if (ImGui::Selectable(m.c_str(), m == mSettings.localModel)) { mSettings.localModel = m; changed = true; }
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(mModelList != nullptr);
			if (ImGui::Button("R")) { RefreshModels(); }
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Refresh the model list"); }
			ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
			ImGui::TextUnformatted("Model");
			if (mSettings.provider == AssistantProviderKind::Ollama)
			{
				ImGui::SetNextItemWidth(-labelWidth);
				if (ImGui::InputInt("Context", &mSettings.contextSize, 1024, 4096)) { mSettings.contextSize = std::clamp(mSettings.contextSize, 2048, 262144); }
				if (ImGui::IsItemDeactivatedAfterEdit()) { changed = true; }
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("num_ctx: how many tokens the model can see. Larger uses more memory"); }
			}

			if (mModelList) { ImGui::TextDisabled("Connecting..."); }
			else if (!mModelListError.empty() || mModels.empty())
			{
				// つながらない・モデルがないときは、始め方を出す
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.75f, 0.4f, 1.0f));
				if (!mModelListError.empty()) { ImGui::TextWrapped("%s", mModelListError.c_str()); }
				else { ImGui::TextWrapped("サーバーにモデルがありません。"); }
				ImGui::PopStyleColor();
				if (mSettings.provider == AssistantProviderKind::Ollama)
				{
					ImGui::TextWrapped("使い方: Ollama（ollama.com、無料）をインストールし、コマンドプロンプトで次を実行してから R を押します。ツールを使えるモデルが必要です。");
					static std::string kCommand = "ollama pull qwen3:8b";
					ImGui::SetNextItemWidth(-1.0f);
					ImGui::InputText("##cmd", &kCommand, ImGuiInputTextFlags_ReadOnly);
				}
				else
				{
					ImGui::TextWrapped("LM Studio なら Developer タブでサーバーを開始し、ツール呼び出しに対応したモデルを読み込んでから R を押します。llama.cpp なら llama-server --jinja で起動します。");
				}
			}
		}
		else
		{
			ImGui::SetNextItemWidth(160.0f);
			if (ImGui::BeginCombo("##claude", kClaudeModels[mSettings.claudeModel].label))
			{
				for (int i = 0; i < int(std::size(kClaudeModels)); ++i)
				{
					if (ImGui::Selectable(kClaudeModels[i].label, i == mSettings.claudeModel)) { mSettings.claudeModel = i; changed = true; }
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80.0f);
			changed |= ImGui::Combo("Effort", &mSettings.claudeEffort, kClaudeEfforts, int(std::size(kClaudeEfforts)));
			ImGui::TextDisabled("Needs ANTHROPIC_API_KEY (paid)");
		}
		if (changed) { SaveSettings(); }
	}

	void AIAssistant::Draw(bool* open)
	{
		ImGui::SetNextWindowSize(ImVec2(420.0f, 600.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("AI Assistant", open))
		{
			ImGui::End();
			return;
		}

		if (ImGui::CollapsingHeader("Settings", ImGuiTreeNodeFlags_DefaultOpen)) { DrawSettings(); }
		if (ImGui::Button("New Chat")) { NewChat(); }
		ImGui::SameLine();
		if (mSettings.IsLocal())
		{
			ImGui::TextDisabled("Tokens: in %llu / out %llu", (unsigned long long)mInputTokens, (unsigned long long)mOutputTokens);
		}
		else
		{
			ImGui::TextDisabled("Tokens: in %llu (cached %llu) / out %llu", (unsigned long long)mInputTokens,
				(unsigned long long)mCacheReadTokens, (unsigned long long)mOutputTokens);
		}

		// 会話
		float inputHeight = ImGui::GetTextLineHeightWithSpacing() * 4.0f + 10.0f;
		ImGui::BeginChild("##chat", ImVec2(0, -inputHeight), ImGuiChildFlags_Border);
		for (const auto& e : mChat)
		{
			switch (e.kind)
			{
			case ChatEntry::Kind::User:
				ImGui::TextColored(ImVec4(0.55f, 0.75f, 1.0f, 1.0f), "You");
				break;
			case ChatEntry::Kind::Assistant:
				ImGui::TextColored(ImVec4(0.75f, 0.6f, 1.0f, 1.0f), "Assistant");
				break;
			case ChatEntry::Kind::Tool:
				ImGui::TextColored(ImVec4(0.5f, 0.85f, 0.55f, 1.0f), "  > %s", e.text.c_str());
				continue;
			case ChatEntry::Kind::Error:
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
				ImGui::TextWrapped("%s", e.text.c_str());
				ImGui::PopStyleColor();
				continue;
			case ChatEntry::Kind::Info:
				ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
				ImGui::TextWrapped("%s", e.text.c_str());
				ImGui::PopStyleColor();
				continue;
			}
			ImGui::TextWrapped("%s", e.text.c_str());
			ImGui::Spacing();
		}
		if (mRequest)
		{
			static const char* kSpin = "|/-\\";
			ImGui::TextDisabled("考え中 %c", kSpin[int(ImGui::GetTime() * 8.0) % 4]);
		}
		if (mScrollToBottom) { ImGui::SetScrollHereY(1.0f); mScrollToBottom = false; }
		ImGui::EndChild();

		// 入力（Ctrl+Enter で送信）
		bool send = false;
		ImGui::BeginDisabled(mRequest != nullptr);
		ImGui::InputTextMultiline("##input", &mInput, ImVec2(-80.0f, inputHeight - 8.0f));
		if (ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) { send = true; }
		ImGui::EndDisabled();
		ImGui::SameLine();
		if (mRequest)
		{
			if (ImGui::Button("Stop", ImVec2(-1, 0)))
			{
				mRequest->cancel = true;
				mRequest.reset();
				mChat.push_back({ ChatEntry::Kind::Info, "止めました。" });
				RollbackToCleanState();
			}
		}
		else if (ImGui::Button("Send", ImVec2(-1, 0)))
		{
			send = true;
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Ctrl+Enter"); }
		if (send && !mInput.empty() && !mRequest)
		{
			Send(mInput);
			mInput.clear();
			mScrollToBottom = true;
		}
		ImGui::End();
	}
}
