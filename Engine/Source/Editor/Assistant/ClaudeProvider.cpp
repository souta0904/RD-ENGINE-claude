#include "RdPch.h"
#include "Editor/Assistant/ClaudeProvider.h"
#include "Editor/Assistant/ProviderUtil.h"

namespace rd
{
	namespace
	{
		constexpr const char* kClaudeUrl = "https://api.anthropic.com/v1/messages";
		constexpr const char* kClaudeVersion = "2026-10-21";
	}

	// 途中で別のモデルに切り替わった応答を会話に戻すときの決まり:
	//  最後の fallback ブロックより前の thinking / tool_use などは送り返さない（fallback ブロック自体は位置を保つ）
	Json ClaudeProvider::FilterContentForEcho(const Json& content)
	{
		size_t lastFallback = SIZE_MAX;
		for (size_t i = 0; i < content.size(); ++i)
		{
			if (content[i].value("type", "") == "fallback") { lastFallback = i; }
		}
		if (lastFallback == SIZE_MAX) { return content; }
		Json out = Json::array();
		for (size_t i = 0; i < content.size(); ++i)
		{
			if (i >= lastFallback || content[i].value("type", "") == "text") { out.push_back(content[i]); }
		}
		return out;
	}

	// Assistant の発言は Claude が返した形（thinking ブロックを含む）をそのまま戻す
	Json ClaudeProvider::BuildMessages(const std::vector<AssistantTurn>& history)
	{
		Json messages = Json::array();
		for (const AssistantTurn& t : history)
		{
			switch (t.role)
			{
			case AssistantTurn::Role::User:
				messages.push_back({ { "role", "user" }, { "content", t.text } });
				break;
			case AssistantTurn::Role::Assistant:
			{
				Json content = t.raw.is_array() ? t.raw : Json::array();
				if (!t.raw.is_array())
				{
					if (!t.text.empty()) { content.push_back({ { "type", "text" }, { "text", t.text } }); }
					for (const auto& c : t.toolCalls) { content.push_back({ { "type", "tool_use" }, { "id", c.id }, { "name", c.name }, { "input", c.input } }); }
				}
				messages.push_back({ { "role", "assistant" }, { "content", content } });
				break;
			}
			case AssistantTurn::Role::ToolResults:
			{
				Json content = Json::array();
				for (const auto& r : t.results)
				{
					Json block = { { "type", "tool_result" }, { "tool_use_id", r.id }, { "content", r.content } };
					if (r.isError) { block["is_error"] = true; }
					content.push_back(std::move(block));
				}
				messages.push_back({ { "role", "user" }, { "content", content } });
				break;
			}
			}
		}
		return messages;
	}

	bool ClaudeProvider::BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
		const AssistantSettings& settings, AssistantRequest& out, std::string& error) const
	{
		std::string apiKey = assistant::GetEnv(L"ANTHROPIC_API_KEY");
		std::string authToken = apiKey.empty() ? assistant::GetEnv(L"ANTHROPIC_AUTH_TOKEN") : std::string();
		if (apiKey.empty() && authToken.empty())
		{
			error = "Claude を使うには環境変数 ANTHROPIC_API_KEY を設定してから、エディタを起動し直してください（有料）。";
			return false;
		}
		const ClaudeModelOption& model = kClaudeModels[std::clamp(settings.claudeModel, 0, int(std::size(kClaudeModels)) - 1)];

		Json body = {
			{ "model", model.id },
			{ "max_tokens", 16000 },
			{ "system", Json::array({ { { "type", "text" }, { "text", system } } }) },
			{ "tools", tools },
			{ "messages", BuildMessages(history) },
			{ "output_config", { { "effort", kClaudeEfforts[std::clamp(settings.claudeEffort, 0, 2)] } } },
			{ "cache_control", { { "type", "ephemeral" } } },
		};
		if (model.fallbacks) { body["fallbacks"] = "default"; }

		std::string betas = model.fallbacks ? "server-side-fallback-2026-07-01" : "";
		out.url = kClaudeUrl;
		out.headers = { { "content-type", "application/json" }, { "anthropic-version", kClaudeVersion } };
		if (!apiKey.empty()) { out.headers.push_back({ "x-api-key", apiKey }); }
		else
		{
			out.headers.push_back({ "authorization", "Bearer " + authToken });
			betas += std::string(betas.empty() ? "" : ",") + "oauth-2025-04-20";
		}
		if (!betas.empty()) { out.headers.push_back({ "anthropic-beta", betas }); }
		out.body = body.dump();
		return true;
	}

	AssistantReply ClaudeProvider::ParseResponse(const HttpResponse& response) const
	{
		AssistantReply reply;
		reply.turn.role = AssistantTurn::Role::Assistant;
		if (!response.Ok()) { reply.error = assistant::HttpErrorMessage(response); return reply; }
		Json message = Json::parse(response.body);
		if (auto usage = message.find("usage"); usage != message.end())
		{
			reply.inputTokens = usage->value("input_tokens", 0ull) + usage->value("cache_creation_input_tokens", 0ull);
			reply.cacheReadTokens = usage->value("cache_read_input_tokens", 0ull);
			reply.outputTokens = usage->value("output_tokens", 0ull);
		}
		Json content = FilterContentForEcho(message.value("content", Json::array()));
		reply.turn.raw = content;
		for (const auto& block : content)
		{
			std::string type = block.value("type", "");
			if (type == "text") { reply.turn.text += block.value("text", ""); }
			else if (type == "tool_use")
			{
				reply.turn.toolCalls.push_back({ block.value("id", ""), block.value("name", ""), block.value("input", Json::object()) });
			}
			else if (type == "fallback") { reply.usedFallback = true; }
		}
		std::string stop = message.value("stop_reason", "");
		if (stop == "tool_use") { reply.stop = AssistantReply::Stop::ToolUse; }
		else if (stop == "refusal") { reply.stop = AssistantReply::Stop::Refusal; }
		else if (stop == "max_tokens") { reply.stop = AssistantReply::Stop::MaxTokens; }
		else { reply.stop = AssistantReply::Stop::EndTurn; }
		return reply;
	}
}
