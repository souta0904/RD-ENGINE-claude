#include "RdPch.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Core/Util/FileUtil.h"
#include "Editor/Assistant/AssistantTools.h"
#include "Editor/Assistant/EditorToolHost.h"
#include "Editor/Editor.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		constexpr int kMaxToolLoops = 25;
		constexpr const char* kSettingsPath = "UserSettings/AIAssistant.json";

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

		std::string ToolSummary(const AssistantToolCall& call)
		{
			std::string summary = call.name;
			if (call.input.contains("name") && call.input["name"].is_string()) { summary += " \"" + call.input["name"].get<std::string>() + "\""; }
			if (call.input.contains("actor_id") && call.input["actor_id"].is_number()) { summary += std::format(" #{}", call.input["actor_id"].get<uint64_t>()); }
			return summary;
		}

		bool IsRetryable(int status)
		{
			return status == 429 || status == 500 || status == 502 || status == 503 || status == 529;
		}
	}

	// 通信中の 1 つのリクエスト（通信のスレッドと共有する）
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

	AIAssistant::AIAssistant(Editor& editor)
		: EditorPanel(editor, "AI Assistant", false)
		, mToolHost(std::make_unique<EditorToolHost>(editor))
	{
		Json saved;
		if (FileExists(kSettingsPath) && LoadJsonFile(kSettingsPath, saved)) { mSettings.FromJson(saved); }
		mChat.push_back({ ChatEntry::Kind::Info, "シーンの編集を頼めます。例:「赤い箱を 5 個、半径 3m の円に並べて」" });
	}

	// 通信中のスレッドは切り離してあるので、結果を捨てるよう知らせるだけでよい
	AIAssistant::~AIAssistant()
	{
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

	void AIAssistant::Cancel()
	{
		if (!mRequest) { return; }
		mRequest->cancel = true;
		mRequest.reset();
		mChat.push_back({ ChatEntry::Kind::Info, "止めました。" });
		RollbackToCleanState();
	}

	// 次の依頼を送れる形まで戻す: 最後が「ツールを呼ばずに終わった返事」になるまで外す
	//  （結果の付いていないツールの呼び出しや、user が続く形は受け付けられない）
	void AIAssistant::RollbackToCleanState()
	{
		while (!mHistory.empty())
		{
			const AssistantTurn& last = mHistory.back();
			if (last.role == AssistantTurn::Role::Assistant && last.toolCalls.empty()) { break; }
			mHistory.pop_back();
		}
	}

	// 選んでいるアクターを添えておく（「これを赤くして」のような頼み方に答えられるように）
	void AIAssistant::Send(const std::string& text)
	{
		if (text.empty() || mRequest) { return; }
		mChat.push_back({ ChatEntry::Kind::User, text });
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

	// 返事まで数十秒かかることもあるので、専用のスレッドで待つ（JobSystem のワーカーを塞がない）
	//  ローカルは 1 回だけ。Claude は混んでいる（429 / 5xx）なら少し待って 2 回までやり直す
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
		Log::Debug(LogCat::Editor, "AI request: {} turns, {} bytes", mHistory.size(), http.body.size());

		bool local = mSettings.IsLocal();
		std::thread([request, http = std::move(http), local]
			{
				int attempts = local ? 1 : 3;
				for (int attempt = 0; attempt < attempts && !request->cancel; ++attempt)
				{
					request->response = HttpClient::Post(http.url, http.headers, http.body, 600000, &request->cancel);
					if (!IsRetryable(request->response.status)) { break; }
					int waitSec = 2 << attempt;
					if (!request->response.retryAfter.empty()) { waitSec = std::clamp(std::atoi(request->response.retryAfter.c_str()), 1, 60); }
					for (int i = 0; i < waitSec * 10 && !request->cancel; ++i) { Sleep(100); }
				}
				request->done = true;
			}).detach();
	}

	void AIAssistant::Update(float)
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

		if (reply.stop == AssistantReply::Stop::Refusal)
		{
			mChat.push_back({ ChatEntry::Kind::Error, "この依頼には答えられませんでした。言い方を変えてもう一度試してください。" });
			return;
		}
		if (reply.stop == AssistantReply::Stop::MaxTokens) { mChat.push_back({ ChatEntry::Kind::Info, "応答が長すぎて途中で切れました。" }); }
		if (reply.stop != AssistantReply::Stop::ToolUse || reply.turn.toolCalls.empty()) { return; }

		RunTools(reply.turn);
		if (++mLoopCount >= kMaxToolLoops)
		{
			mChat.push_back({ ChatEntry::Kind::Error, "ツールの呼び出しが多すぎるので止めました。" });
			RollbackToCleanState();
			return;
		}
		StartRequest();
	}

	// ツールを実行して、結果をすべてまとめて 1 つの発言として返す
	void AIAssistant::RunTools(const AssistantTurn& turn)
	{
		AssistantTurn results;
		results.role = AssistantTurn::Role::ToolResults;
		for (const AssistantToolCall& call : turn.toolCalls)
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
			std::string summary = ToolSummary(call);
			if (isError) { summary += "  -> " + result; }
			mChat.push_back({ isError ? ChatEntry::Kind::Error : ChatEntry::Kind::Tool, summary });
			results.results.push_back({ call.id, call.name, result, isError });
		}
		mHistory.push_back(std::move(results));
	}

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
}
