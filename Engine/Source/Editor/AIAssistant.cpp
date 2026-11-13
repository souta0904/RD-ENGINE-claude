#include "RdPch.h"
#include "Editor/AIAssistant.h"
#include "Core/StringUtil.h"
#include "Editor/AssistantTools.h"
#include "Editor/Editor.h"
#include "Engine.h"
#include "Platform/HttpClient.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		constexpr const char* kApiUrl = "https://api.anthropic.com/v1/messages";
		constexpr const char* kApiVersion = "2026-10-21";
		constexpr int kMaxToolLoops = 25;

		struct ModelOption
		{
			const char* id;
			const char* label;
			bool fallbacks;// 安全上の理由で断られたとき、サーバー側で別のモデルに切り替える
		};
		constexpr ModelOption kModels[] = {
			{ "claude-opus-5-5", "Claude Opus 5.5", true },
			{ "claude-sonnet-5-5", "Claude Sonnet 5.5", true },
			{ "claude-haiku-5-5", "Claude Haiku 5.5", false },
		};
		constexpr const char* kEfforts[] = { "low", "medium", "high" };

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

		std::string GetEnv(const wchar_t* name)
		{
			wchar_t buffer[4096];
			DWORD n = GetEnvironmentVariableW(name, buffer, DWORD(std::size(buffer)));
			return n > 0 && n < std::size(buffer) ? ToUtf8(std::wstring(buffer, n)) : std::string();
		}

		// API のエラーメッセージを取り出す（{"type":"error","error":{"type":..,"message":..}}）
		std::string ApiErrorMessage(const HttpResponse& r)
		{
			if (r.status == 0) { return r.error; }
			try
			{
				Json j = Json::parse(r.body);
				if (auto e = j.find("error"); e != j.end() && e->is_object())
				{
					return std::format("HTTP {} {}: {}", r.status, e->value("type", ""), e->value("message", ""));
				}
			}
			catch (...)
			{
			}
			return std::format("HTTP {}", r.status);
		}

		// 途中で別のモデルに切り替わった応答を会話に戻すときの決まり:
		//  最後の fallback ブロックより前の thinking / tool_use などは送り返さない（fallback ブロック自体は位置を保つ）
		Json FilterContentForEcho(const Json& content)
		{
			size_t lastFallback = SIZE_MAX;
			for (size_t i = 0; i < content.size(); ++i)
			{
				if (content[i].value("type", "") == "fallback") { lastFallback = i; }
			}
			if (lastFallback == SIZE_MAX) { return content; }
			Json out = Json::array();
			for (size_t i = 0; i < content.size(); ++i)
			{
				std::string type = content[i].value("type", "");
				bool keep = i >= lastFallback || type == "text";
				if (keep) { out.push_back(content[i]); }
			}
			return out;
		}
	}

	// 通信中の 1 リクエスト（ワーカースレッドと共有する）
	struct AIAssistant::PendingRequest
	{
		std::atomic<bool> done = false;
		std::atomic<bool> cancel = false;
		HttpResponse response;
	};

	namespace
	{
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

	AIAssistant::AIAssistant(Editor& editor) : mEditor(editor), mToolHost(std::make_unique<EditorToolHost>(editor))
	{
		mChat.push_back({ ChatEntry::Kind::Info, "シーンの編集を頼めます。例:「赤い箱を 5 個、半径 3m の円に並べて」" });
	}

	AIAssistant::~AIAssistant()
	{
		// 通信中のスレッドは切り離してあるので、結果を捨てるよう知らせるだけでよい
		if (mRequest) { mRequest->cancel = true; }
	}

	void AIAssistant::RollbackToCleanState()
	{
		// 次に依頼を送れる形まで戻す: 最後が「ツールを呼ばずに終わった assistant の返事」になるまで外す
		//  （tool_use に結果が付いていない、user が続く、といった形は API が受け付けない）
		while (!mMessages.empty())
		{
			const Json& last = mMessages.back();
			bool pendingTool = false;
			if (last.value("role", "") == "assistant")
			{
				for (const auto& b : last.value("content", Json::array())) { pendingTool |= b.value("type", "") == "tool_use"; }
				if (!pendingTool) { break; }
			}
			mMessages.erase(mMessages.end() - 1);
		}
	}

	void AIAssistant::NewChat()
	{
		if (mRequest) { mRequest->cancel = true; mRequest.reset(); }
		mMessages = Json::array();
		mChat.clear();
		mChat.push_back({ ChatEntry::Kind::Info, "新しい会話を始めました。" });
		mLoopCount = 0;
	}

	Json AIAssistant::BuildRequestBody() const
	{
		const ModelOption& model = kModels[mModelIndex];
		Json body = {
			{ "model", model.id },
			{ "max_tokens", 16000 },
			{ "system", Json::array({ { { "type", "text" }, { "text", kSystemPrompt } } }) },
			{ "tools", AssistantTools::GetDefinitions() },
			{ "messages", mMessages },
			{ "output_config", { { "effort", kEfforts[mEffortIndex] } } },
			// 会話の先頭部分（システムプロンプト・ツール・これまでの履歴）をキャッシュして、ツールの往復を安く速くする
			{ "cache_control", { { "type", "ephemeral" } } },
		};
		if (model.fallbacks) { body["fallbacks"] = "default"; }
		return body;
	}

	void AIAssistant::Send(const std::string& text)
	{
		if (text.empty() || mRequest) { return; }
		mChat.push_back({ ChatEntry::Kind::User, text });
		// 選択中のアクターを添えておく（「これを赤くして」のような頼み方に答えられるように）
		std::string content = text;
		if (Actor* sel = mEditor.GetSelected())
		{
			content += std::format("\n\n(Currently selected actor: id {} \"{}\")", sel->GetId(), sel->GetName());
		}
		mMessages.push_back({ { "role", "user" }, { "content", content } });
		mLoopCount = 0;
		StartRequest();
	}

	void AIAssistant::StartRequest()
	{
		std::string apiKey = GetEnv(L"ANTHROPIC_API_KEY");
		std::string authToken = apiKey.empty() ? GetEnv(L"ANTHROPIC_AUTH_TOKEN") : std::string();
		if (apiKey.empty() && authToken.empty())
		{
			mChat.push_back({ ChatEntry::Kind::Error,
				"API キーが設定されていません。環境変数 ANTHROPIC_API_KEY を設定してから、エディタを起動し直してください。" });
			mMessages.erase(mMessages.end() - 1);// 送れなかった依頼は履歴から外す
			return;
		}

		std::vector<std::string> betas;
		HttpClient::Headers headers = {
			{ "content-type", "application/json" },
			{ "anthropic-version", kApiVersion },
		};
		if (!apiKey.empty()) { headers.push_back({ "x-api-key", apiKey }); }
		else
		{
			headers.push_back({ "authorization", "Bearer " + authToken });
			betas.push_back("oauth-2025-04-20");
		}
		if (kModels[mModelIndex].fallbacks) { betas.push_back("server-side-fallback-2026-07-01"); }
		if (!betas.empty())
		{
			std::string joined;
			for (const auto& b : betas) { joined += (joined.empty() ? "" : ",") + b; }
			headers.push_back({ "anthropic-beta", joined });
		}

		auto request = std::make_shared<PendingRequest>();
		mRequest = request;
		std::string body = BuildRequestBody().dump();
		Log::Debug(LogCat::Editor, "AI request: {} messages, {} bytes", mMessages.size(), body.size());

		// 返事まで数十秒かかることもあるので専用のスレッドで待つ（JobSystem のワーカーを塞がない）
		std::thread([request, headers = std::move(headers), body = std::move(body)]
			{
				for (int attempt = 0; attempt < 3 && !request->cancel; ++attempt)
				{
					request->response = HttpClient::Post(kApiUrl, headers, body, 600000, &request->cancel);
					int s = request->response.status;
					bool retryable = s == 429 || s == 500 || s == 502 || s == 503 || s == 529 || (s == 0 && request->response.error != "Canceled");
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
		if (!mRequest || !mRequest->done) { return; }
		auto request = std::move(mRequest);
		if (request->cancel) { return; }
		HandleResponse(request->response);
	}

	void AIAssistant::HandleResponse(const HttpResponse& response)
	{
		mScrollToBottom = true;
		if (!response.Ok())
		{
			std::string message = ApiErrorMessage(response);
			Log::Warning(LogCat::Editor, "AI request failed: {}", message);
			mChat.push_back({ ChatEntry::Kind::Error, message });
			RollbackToCleanState();
			return;
		}

		Json message;
		try { message = Json::parse(response.body); }
		catch (const std::exception& e)
		{
			mChat.push_back({ ChatEntry::Kind::Error, std::string("応答を読めませんでした: ") + e.what() });
			RollbackToCleanState();
			return;
		}
		if (auto usage = message.find("usage"); usage != message.end())
		{
			mInputTokens += usage->value("input_tokens", 0ull) + usage->value("cache_creation_input_tokens", 0ull);
			mCacheReadTokens += usage->value("cache_read_input_tokens", 0ull);
			mOutputTokens += usage->value("output_tokens", 0ull);
		}

		std::string stopReason = message.value("stop_reason", "");
		Json content = FilterContentForEcho(message.value("content", Json::array()));
		// 応答はそのまま（thinking ブロックも含めて）履歴に戻す
		mMessages.push_back({ { "role", "assistant" }, { "content", content } });

		for (const auto& block : content)
		{
			std::string type = block.value("type", "");
			if (type == "text" && !block.value("text", "").empty()) { mChat.push_back({ ChatEntry::Kind::Assistant, block["text"] }); }
			else if (type == "fallback")
			{
				mChat.push_back({ ChatEntry::Kind::Info, "（別のモデルに切り替えて続けました）" });
			}
		}

		if (stopReason == "refusal")
		{
			mChat.push_back({ ChatEntry::Kind::Error, "この依頼には答えられませんでした。言い方を変えてもう一度試してください。" });
			return;
		}
		if (stopReason == "max_tokens")
		{
			mChat.push_back({ ChatEntry::Kind::Info, "応答が長すぎて途中で切れました。" });
		}
		if (stopReason != "tool_use") { return; }

		// ツールを実行して、結果をすべて 1 つの user メッセージで返す
		Json results = Json::array();
		for (const auto& block : content)
		{
			if (block.value("type", "") != "tool_use") { continue; }
			std::string name = block.value("name", "");
			Json input = block.value("input", Json::object());
			bool isError = false;
			std::string result;
			try { result = AssistantTools::Run(*mToolHost, name, input, isError); }
			catch (const std::exception& e)
			{
				result = std::string("Tool failed: ") + e.what();
				isError = true;
			}
			std::string summary = name;
			if (input.contains("name")) { summary += " \"" + input.value("name", "") + "\""; }
			if (input.contains("actor_id")) { summary += std::format(" #{}", input.value("actor_id", 0ull)); }
			if (isError) { summary += "  -> " + result; }
			mChat.push_back({ isError ? ChatEntry::Kind::Error : ChatEntry::Kind::Tool, summary });

			Json r = { { "type", "tool_result" }, { "tool_use_id", block.value("id", "") }, { "content", result } };
			if (isError) { r["is_error"] = true; }
			results.push_back(std::move(r));
		}
		mMessages.push_back({ { "role", "user" }, { "content", results } });

		if (++mLoopCount >= kMaxToolLoops)
		{
			mChat.push_back({ ChatEntry::Kind::Error, "ツールの呼び出しが多すぎるので止めました。" });
			RollbackToCleanState();
			return;
		}
		StartRequest();
	}

	void AIAssistant::Draw(bool* open)
	{
		ImGui::SetNextWindowSize(ImVec2(420.0f, 560.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("AI Assistant", open))
		{
			ImGui::End();
			return;
		}

		// 設定
		ImGui::SetNextItemWidth(150.0f);
		if (ImGui::BeginCombo("##model", kModels[mModelIndex].label))
		{
			for (int i = 0; i < int(std::size(kModels)); ++i)
			{
				if (ImGui::Selectable(kModels[i].label, i == mModelIndex)) { mModelIndex = i; }
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::Combo("Effort", &mEffortIndex, kEfforts, int(std::size(kEfforts)));
		ImGui::SameLine();
		if (ImGui::Button("New Chat")) { NewChat(); }
		ImGui::TextDisabled("Tokens: in %llu (cached %llu) / out %llu", (unsigned long long)mInputTokens,
			(unsigned long long)mCacheReadTokens, (unsigned long long)mOutputTokens);

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
				ImGui::TextColored(ImVec4(0.75f, 0.6f, 1.0f, 1.0f), "Claude");
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
