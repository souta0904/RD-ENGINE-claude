#pragma once
#include "Core/JsonArchive.h"
#include <atomic>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class AssistantToolHost;
	class Editor;
	struct HttpResponse;

	// エディタの AI アシスタント（Claude）
	//  「赤い箱を 5 個円形に並べて」のように頼むと、Claude がツールを使ってシーンを読み書きする。
	//  - API キーは環境変数 ANTHROPIC_API_KEY（または ANTHROPIC_AUTH_TOKEN）から読む。ファイルには保存しない
	//  - 通信はワーカースレッド、ツールの実行（シーンの変更）はメインスレッドで行う
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

		void Send(const std::string& text);
		void StartRequest();
		void HandleResponse(const HttpResponse& response);
		void NewChat();
		void RollbackToCleanState();
		Json BuildRequestBody() const;

		Editor& mEditor;
		std::unique_ptr<AssistantToolHost> mToolHost;// ツールからエディタを触る窓口（Editor/AssistantTools.h）
		Json mMessages = Json::array();
		std::vector<ChatEntry> mChat;
		std::shared_ptr<PendingRequest> mRequest;
		int mLoopCount = 0;// 1 回の依頼でのツール往復の数（暴走防止）

		// 設定と表示
		int mModelIndex = 0;
		int mEffortIndex = 1;
		std::string mInput;
		bool mScrollToBottom = false;
		uint64_t mInputTokens = 0;
		uint64_t mOutputTokens = 0;
		uint64_t mCacheReadTokens = 0;
	};
}
