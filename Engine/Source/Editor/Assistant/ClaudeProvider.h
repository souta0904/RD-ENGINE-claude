#pragma once
#include "Editor/Assistant/AssistantProvider.h"

namespace rd
{
	// Claude（Messages API）への変換（環境変数 ANTHROPIC_API_KEY か ANTHROPIC_AUTH_TOKEN を使う）
	//  Claude が返した形をそのまま送り返して thinking を保ち、会話の先頭をキャッシュしてツールの往復を安く速くする
	class RD_API ClaudeProvider final : public AssistantProvider
	{
	public:
		bool BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
			const AssistantSettings& settings, AssistantRequest& out, std::string& error) const override;
		AssistantReply ParseResponse(const HttpResponse& response) const override;

	private:
		static Json BuildMessages(const std::vector<AssistantTurn>& history);
		static Json FilterContentForEcho(const Json& content);
	};
}
