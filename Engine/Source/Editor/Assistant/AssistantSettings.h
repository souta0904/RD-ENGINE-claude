#pragma once
#include "Core/Serialization/Json.h"
#include <string>

namespace rd
{
	// Ollama: ローカル（http://localhost:11434）。既定
	// OpenAICompatible: ローカル（LM Studio / llama.cpp server など OpenAI 互換の /chat/completions）
	// Claude: Anthropic の Claude API（環境変数 ANTHROPIC_API_KEY が要る・有料）
	enum class AssistantProviderKind
	{
		Ollama,
		OpenAICompatible,
		Claude,
	};

	// AI アシスタントの設定（UserSettings/AIAssistant.json に保存する。API キーは保存しない）
	//  localModel が空ならサーバーにある最初のモデル、contextSize は Ollama の num_ctx（ツールの説明とシーンが入る大きさ）
	//  claudeModel は kClaudeModels の番号、claudeEffort は low / medium / high
	struct RD_API AssistantSettings
	{
		AssistantProviderKind provider = AssistantProviderKind::Ollama;
		std::string ollamaUrl = "http://localhost:11434";
		std::string openAIUrl = "http://localhost:1234/v1";
		std::string localModel;
		int contextSize = 16384;
		int claudeModel = 0;
		int claudeEffort = 1;

		Json ToJson() const;
		void FromJson(const Json& j);
		bool IsLocal() const { return provider != AssistantProviderKind::Claude; }
	};

	// fallbacks: 安全上の理由で断られたとき、サーバーの側で別のモデルに切り替える
	struct ClaudeModelOption
	{
		const char* id;
		const char* label;
		bool fallbacks;
	};
	inline constexpr ClaudeModelOption kClaudeModels[] = {
		{ "claude-opus-5-5", "Claude Opus 5.5", true },
		{ "claude-sonnet-5-5", "Claude Sonnet 5.5", true },
		{ "claude-haiku-5-5", "Claude Haiku 5.5", false },
	};
	inline constexpr const char* kClaudeEfforts[] = { "low", "medium", "high" };
}
