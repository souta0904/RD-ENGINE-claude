#pragma once
#include "Editor/Assistant/AssistantProvider.h"
#include "Editor/Panels/EditorPanel.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class AssistantToolHost;

	// エディタの AI アシスタント（パネル）
	//  「赤い箱を 5 個円形に並べて」のように頼むと、LLM がツールを使ってシーンを読み書きする。
	//  - 既定はローカル LLM（Ollama）。API キーも利用料もいらないので誰でも使える
	//    LM Studio / llama.cpp server などの OpenAI 互換のサーバーと、Claude API（有料）にも切り替えられる
	//  - 通信は専用のスレッド、ツールの実行（シーンの変更）はメインスレッドで行う
	//  - シーンの変更は普段の編集と同じコマンドとして積むので、Ctrl+Z で 1 つずつ戻せる
	//  - 1 回の依頼でのツールの往復の数に上限を付けて、止まらなくなるのを防ぐ
	//  実装は AIAssistant.cpp（会話と通信）と AIAssistantView.cpp（画面）
	class AIAssistant final : public EditorPanel
	{
	public:
		explicit AIAssistant(Editor& editor);
		~AIAssistant();
		const char* GetShortcut() const override { return "F8"; }

		// 閉じていても、届いた応答の処理とツールの実行を進める
		void Update(float dt) override;
		bool IsBusy() const { return mRequest != nullptr; }

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(420.0f, 600.0f); }

	private:
		struct ChatEntry
		{
			enum class Kind { User, Assistant, Tool, Error, Info } kind;
			std::string text;
		};
		struct PendingRequest;
		struct PendingModelList;

		void Send(const std::string& text);
		void StartRequest();
		void HandleReply(const AssistantReply& reply);
		void RunTools(const AssistantTurn& turn);
		void NewChat();
		void Cancel();
		void RollbackToCleanState();
		void RefreshModels();
		void SaveSettings() const;

		void DrawSettings();
		void DrawLocalSettings(bool& changed);
		void DrawClaudeSettings(bool& changed);
		void DrawChat(float inputHeight);
		void DrawInput(float inputHeight);

		std::unique_ptr<AssistantToolHost> mToolHost;
		AssistantSettings mSettings;
		std::vector<AssistantTurn> mHistory;
		std::vector<ChatEntry> mChat;
		std::shared_ptr<PendingRequest> mRequest;
		int mLoopCount = 0;

		std::shared_ptr<PendingModelList> mModelList;
		std::vector<std::string> mModels;
		std::string mModelListError;
		bool mModelsRequested = false;

		std::string mInput;
		bool mScrollToBottom = false;
		uint64_t mInputTokens = 0;
		uint64_t mOutputTokens = 0;
		uint64_t mCacheReadTokens = 0;
	};
}
