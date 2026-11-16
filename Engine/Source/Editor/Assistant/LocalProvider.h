#pragma once
#include "Editor/Assistant/AssistantProvider.h"

namespace rd
{
	// ローカルの LLM への変換（Ollama の /api/chat と、OpenAI 互換の /chat/completions）
	//  どちらも OpenAI の function calling と同じ形のツールを使う。違いはツールの引数の形（Ollama は JSON のまま、
	//  OpenAI 互換は文字列にした JSON）と、ツールの結果の結び付け方（Ollama は tool_name、OpenAI 互換は tool_call_id）
	class LocalProvider final : public AssistantProvider
	{
	public:
		explicit LocalProvider(bool ollama) : mOllama(ollama) {}

		bool BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
			const AssistantSettings& settings, AssistantRequest& out, std::string& error) const override;
		AssistantReply ParseResponse(const HttpResponse& response) const override;

	private:
		Json BuildMessages(const std::vector<AssistantTurn>& history, const std::string& system) const;
		AssistantToolCall ParseToolCall(const Json& call) const;
		static uint64_t NextId();

		bool mOllama;
	};
}
