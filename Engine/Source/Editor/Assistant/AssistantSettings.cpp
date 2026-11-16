#include "RdPch.h"
#include "Editor/Assistant/AssistantSettings.h"

namespace rd
{
	Json AssistantSettings::ToJson() const
	{
		return {
			{ "provider", int(provider) },
			{ "ollamaUrl", ollamaUrl },
			{ "openAIUrl", openAIUrl },
			{ "localModel", localModel },
			{ "contextSize", contextSize },
			{ "claudeModel", claudeModel },
			{ "claudeEffort", claudeEffort },
		};
	}

	void AssistantSettings::FromJson(const Json& j)
	{
		provider = AssistantProviderKind(std::clamp(j.value("provider", int(provider)), 0, 2));
		ollamaUrl = j.value("ollamaUrl", ollamaUrl);
		openAIUrl = j.value("openAIUrl", openAIUrl);
		localModel = j.value("localModel", localModel);
		contextSize = j.value("contextSize", contextSize);
		claudeModel = j.value("claudeModel", claudeModel);
		claudeEffort = j.value("claudeEffort", claudeEffort);
	}
}
