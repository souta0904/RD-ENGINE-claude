#pragma once
#include "Editor/AssistantProviders.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class AssistantToolHost;
	class Editor;

	// エディタの AI アシスタント
	//  「赤い箱を 5 個円形に並べて」のように頼むと、LLM がツールを使ってシーンを読み書きする。
	//  - 既定はローカル LLM（Ollama）。API キーも利用料もいらないので誰でも使える
	//    LM Studio / llama.cpp server などの OpenAI 互換サーバーと、Claude API（有料）にも切り替えられる
	//  - 通信は専用スレッド、ツールの実行（シーンの変更）はメインスレッドで行う
	//  - シーンの変更は通常の編集と同じコマンドとして積むので、Ctrl+Z で 1 つずつ戻せる
	class AIAssistant
	{
	public:
		explicit AIAssistant(Editor& editor);
		~AIAssistant();

		// 毎フレーム呼ぶ（届いた応答の処理とツールの実行。パネルを閉じていても進める）
		void Update();
		void Draw(bool* open);

		bool IsBusy() const { return mRequest != nullptr; }

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
		void NewChat();
		void RollbackToCleanState();
		void DrawSettings();
		void RefreshModels();
		void SaveSettings() const;

		Editor& mEditor;
		std::unique_ptr<AssistantToolHost> mToolHost;// ツールからエディタを触る窓口（Editor/AssistantTools.h）
		AssistantSettings mSettings;
		std::vector<AssistantTurn> mHistory;
		std::vector<ChatEntry> mChat;
		std::shared_ptr<PendingRequest> mRequest;
		int mLoopCount = 0;// 1 回の依頼でのツール往復の数（暴走防止）

		// ローカルのモデル一覧
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
