#pragma once
#include "Editor/Assistant/AssistantSettings.h"
#include "Editor/Assistant/AssistantTypes.h"
#include <memory>

namespace rd
{
	// LLM ごとの変換の基底（会話 → HTTP のリクエスト、HTTP の返事 → 会話）
	//  通信はしないので、テストからそのまま呼べる。tools は Claude の形（name / description / input_schema）で渡す
	//  送れないとき（API キーがないなど）は BuildRequest が false と理由を返す
	class AssistantProvider
	{
	public:
		virtual ~AssistantProvider() = default;
		virtual bool BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
			const AssistantSettings& settings, AssistantRequest& out, std::string& error) const = 0;
		virtual AssistantReply ParseResponse(const HttpResponse& response) const = 0;
	};

	std::unique_ptr<AssistantProvider> CreateAssistantProvider(AssistantProviderKind kind);

	// ローカルのサーバーにあるモデルの名前（つながらなければ空で、error に理由）
	std::vector<std::string> ListLocalModels(const AssistantSettings& settings, std::string& error);
}
