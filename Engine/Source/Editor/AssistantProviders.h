#pragma once
#include "Core/JsonArchive.h"
#include "Platform/HttpClient.h"
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	// ==================================================
	// AI アシスタントの会話（どの LLM でも同じ形で持つ）
	// ==================================================
	struct AssistantToolCall
	{
		std::string id;
		std::string name;
		Json input = Json::object();
	};

	struct AssistantToolResult
	{
		std::string id;
		std::string name;
		std::string content;
		bool isError = false;
	};

	struct AssistantTurn
	{
		enum class Role { User, Assistant, ToolResults } role = Role::User;
		std::string text;// User / Assistant の本文
		std::vector<AssistantToolCall> toolCalls;// Assistant が呼んだツール
		std::vector<AssistantToolResult> results;// ToolResults
		Json raw;// その LLM が返したそのままの形（同じ LLM へはこれを送り返す。Claude の thinking を保つため）
	};

	// LLM の返事 1 回分
	struct AssistantReply
	{
		enum class Stop { EndTurn, ToolUse, MaxTokens, Refusal, Error } stop = Stop::Error;
		AssistantTurn turn;// role = Assistant
		std::string error;
		bool usedFallback = false;// Claude が別のモデルに切り替えて答えた
		uint64_t inputTokens = 0;
		uint64_t outputTokens = 0;
		uint64_t cacheReadTokens = 0;
	};

	// 送る HTTP リクエスト
	struct AssistantRequest
	{
		std::string url;
		HttpClient::Headers headers;
		std::string body;
	};

	// ==================================================
	// 設定（UserSettings/AIAssistant.json に保存。API キーは保存しない）
	// ==================================================
	enum class AssistantProviderKind
	{
		Ollama,// ローカル: Ollama（http://localhost:11434）。既定
		OpenAICompatible,// ローカル: LM Studio / llama.cpp server など OpenAI 互換の /chat/completions
		Claude,// Anthropic の Claude API（環境変数 ANTHROPIC_API_KEY が必要・有料）
	};

	struct AssistantSettings
	{
		AssistantProviderKind provider = AssistantProviderKind::Ollama;
		std::string ollamaUrl = "http://localhost:11434";
		std::string openAIUrl = "http://localhost:1234/v1";
		std::string localModel;// 空なら、サーバーにある最初のモデル
		int contextSize = 16384;// Ollama の num_ctx（ツールの説明とシーンが入る大きさ）
		int claudeModel = 0;// kClaudeModels の番号
		int claudeEffort = 1;// low / medium / high

		Json ToJson() const;
		void FromJson(const Json& j);
		bool IsLocal() const { return provider != AssistantProviderKind::Claude; }
	};

	// ==================================================
	// LLM ごとの変換（会話 → HTTP リクエスト、HTTP の返事 → 会話）
	//  通信はしないので、テストからそのまま呼べる
	// ==================================================
	class AssistantProvider
	{
	public:
		virtual ~AssistantProvider() = default;
		// 送れないとき（API キーがないなど）は false と理由
		virtual bool BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
			const AssistantSettings& settings, AssistantRequest& out, std::string& error) const = 0;
		virtual AssistantReply ParseResponse(const HttpResponse& response) const = 0;
		// tools は Claude の形（name / description / input_schema）で渡す
	};

	std::unique_ptr<AssistantProvider> CreateAssistantProvider(AssistantProviderKind kind);

	// ローカルのサーバーにあるモデルの名前（つながらなければ空で、error に理由）
	std::vector<std::string> ListLocalModels(const AssistantSettings& settings, std::string& error);

	struct ClaudeModelOption
	{
		const char* id;
		const char* label;
		bool fallbacks;// 安全上の理由で断られたとき、サーバー側で別のモデルに切り替える
	};
	inline constexpr ClaudeModelOption kClaudeModels[] = {
		{ "claude-opus-5-5", "Claude Opus 5.5", true },
		{ "claude-sonnet-5-5", "Claude Sonnet 5.5", true },
		{ "claude-haiku-5-5", "Claude Haiku 5.5", false },
	};
	inline constexpr const char* kClaudeEfforts[] = { "low", "medium", "high" };
}
