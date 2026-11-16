#include "RdPch.h"
#include "Editor/Assistant/AssistantProvider.h"
#include "Editor/Assistant/ClaudeProvider.h"
#include "Editor/Assistant/LocalProvider.h"
#include "Editor/Assistant/ProviderUtil.h"

namespace rd
{
	std::unique_ptr<AssistantProvider> CreateAssistantProvider(AssistantProviderKind kind)
	{
		switch (kind)
		{
		case AssistantProviderKind::Ollama: return std::make_unique<LocalProvider>(true);
		case AssistantProviderKind::OpenAICompatible: return std::make_unique<LocalProvider>(false);
		case AssistantProviderKind::Claude: return std::make_unique<ClaudeProvider>();
		}
		return nullptr;
	}

	// Ollama: { "models": [ { "name": ... } ] }、OpenAI 互換: { "data": [ { "id": ... } ] }
	std::vector<std::string> ListLocalModels(const AssistantSettings& settings, std::string& error)
	{
		std::vector<std::string> names;
		bool ollama = settings.provider == AssistantProviderKind::Ollama;
		std::string url = ollama ? assistant::TrimSlash(settings.ollamaUrl) + "/api/tags" : assistant::TrimSlash(settings.openAIUrl) + "/models";
		HttpResponse r = HttpClient::Get(url, {}, 3000);
		if (!r.Ok())
		{
			error = r.status == 0 ? "サーバーに接続できません（" + url + "）" : assistant::HttpErrorMessage(r);
			return names;
		}
		try
		{
			Json j = Json::parse(r.body);
			for (const auto& m : j.value(ollama ? "models" : "data", Json::array()))
			{
				std::string name = m.value(ollama ? "name" : "id", "");
				if (!name.empty()) { names.push_back(name); }
			}
		}
		catch (const std::exception& e)
		{
			error = std::string("モデルの一覧を読めませんでした: ") + e.what();
		}
		return names;
	}
}
