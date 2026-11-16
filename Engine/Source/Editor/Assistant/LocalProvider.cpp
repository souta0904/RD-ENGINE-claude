#include "RdPch.h"
#include "Editor/Assistant/LocalProvider.h"
#include "Editor/Assistant/ProviderUtil.h"

namespace rd
{
	namespace
	{
		// ローカルの小さなモデルは扱える長さが短いので、大きすぎるツールの結果は切り詰める
		constexpr size_t kMaxLocalToolResult = 12000;
	}

	uint64_t LocalProvider::NextId()
	{
		static std::atomic<uint64_t> next = 1;
		return next++;
	}

	Json LocalProvider::BuildMessages(const std::vector<AssistantTurn>& history, const std::string& system) const
	{
		Json messages = Json::array({ { { "role", "system" }, { "content", system } } });
		for (const AssistantTurn& t : history)
		{
			switch (t.role)
			{
			case AssistantTurn::Role::User:
				messages.push_back({ { "role", "user" }, { "content", t.text } });
				break;
			case AssistantTurn::Role::Assistant:
			{
				Json m = { { "role", "assistant" }, { "content", t.text } };
				if (!t.toolCalls.empty())
				{
					Json calls = Json::array();
					for (const auto& c : t.toolCalls)
					{
						if (mOllama) { calls.push_back({ { "function", { { "name", c.name }, { "arguments", c.input } } } }); }
						else { calls.push_back({ { "id", c.id }, { "type", "function" }, { "function", { { "name", c.name }, { "arguments", c.input.dump() } } } }); }
					}
					m["tool_calls"] = calls;
				}
				messages.push_back(std::move(m));
				break;
			}
			case AssistantTurn::Role::ToolResults:
				for (const auto& r : t.results)
				{
					std::string content = (r.isError ? "Error: " : "") + r.content;
					if (content.size() > kMaxLocalToolResult) { content = content.substr(0, kMaxLocalToolResult) + "...(truncated)"; }
					Json m = { { "role", "tool" }, { "content", content } };
					if (mOllama) { m["tool_name"] = r.name; }
					else { m["tool_call_id"] = r.id; }
					messages.push_back(std::move(m));
				}
				break;
			}
		}
		return messages;
	}

	bool LocalProvider::BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
		const AssistantSettings& settings, AssistantRequest& out, std::string& error) const
	{
		if (settings.localModel.empty())
		{
			error = "モデルが選ばれていません。ローカルのサーバーを起動して、モデルの一覧から選んでください。";
			return false;
		}

		// Claude の形のツールの定義を OpenAI の形へ
		Json functions = Json::array();
		for (const auto& t : tools)
		{
			functions.push_back({ { "type", "function" }, { "function", {
				{ "name", t.value("name", "") }, { "description", t.value("description", "") }, { "parameters", t.value("input_schema", Json::object()) } } } });
		}

		Json body = { { "model", settings.localModel }, { "messages", BuildMessages(history, system) }, { "tools", functions }, { "stream", false } };
		if (mOllama)
		{
			// 既定の文脈の長さでは、ツールの説明とシーンが入りきらない
			body["options"] = { { "num_ctx", std::max(2048, settings.contextSize) } };
			out.url = assistant::TrimSlash(settings.ollamaUrl) + "/api/chat";
		}
		else
		{
			out.url = assistant::TrimSlash(settings.openAIUrl) + "/chat/completions";
		}
		out.headers = { { "content-type", "application/json" } };
		out.body = body.dump();
		return true;
	}

	// 引数は JSON のオブジェクトか、JSON を入れた文字列で届く。読めなければ __invalid_arguments に入れて返す
	AssistantToolCall LocalProvider::ParseToolCall(const Json& call) const
	{
		const Json& fn = call.value("function", Json::object());
		AssistantToolCall tc;
		tc.name = fn.value("name", "");
		tc.id = call.value("id", "");
		if (tc.id.empty()) { tc.id = std::format("call_{}", NextId()); }
		if (auto args = fn.find("arguments"); args != fn.end())
		{
			if (args->is_object()) { tc.input = *args; }
			else if (args->is_string())
			{
				try { tc.input = Json::parse(args->get<std::string>()); }
				catch (...) { tc.input = { { "__invalid_arguments", args->get<std::string>() } }; }
			}
		}
		return tc;
	}

	// サーバーによっては、ツールを呼んでも終わりの理由が "stop" のことがあるので、ツールの有無で決める
	AssistantReply LocalProvider::ParseResponse(const HttpResponse& response) const
	{
		AssistantReply reply;
		reply.turn.role = AssistantTurn::Role::Assistant;
		if (response.status == 0 && response.error != "Canceled")
		{
			reply.error = response.error + "\nローカル LLM のサーバーが起動しているか確かめてください（Settings の説明を参照）。";
			return reply;
		}
		if (!response.Ok()) { reply.error = assistant::HttpErrorMessage(response); return reply; }
		Json j = Json::parse(response.body);
		Json message;
		std::string finish;
		if (mOllama)
		{
			message = j.value("message", Json::object());
			finish = j.value("done_reason", "");
			reply.inputTokens = j.value("prompt_eval_count", 0ull);
			reply.outputTokens = j.value("eval_count", 0ull);
		}
		else
		{
			const Json& choices = j.at("choices");
			if (choices.empty()) { reply.error = "empty response"; return reply; }
			message = choices[0].value("message", Json::object());
			finish = choices[0].value("finish_reason", "");
			if (auto usage = j.find("usage"); usage != j.end() && usage->is_object())
			{
				reply.inputTokens = usage->value("prompt_tokens", 0ull);
				reply.outputTokens = usage->value("completion_tokens", 0ull);
			}
		}

		if (auto c = message.find("content"); c != message.end() && c->is_string()) { reply.turn.text = assistant::StripThinking(c->get<std::string>()); }
		if (auto calls = message.find("tool_calls"); calls != message.end() && calls->is_array())
		{
			for (const auto& call : *calls) { reply.turn.toolCalls.push_back(ParseToolCall(call)); }
		}
		if (!reply.turn.toolCalls.empty()) { reply.stop = AssistantReply::Stop::ToolUse; }
		else if (finish == "length") { reply.stop = AssistantReply::Stop::MaxTokens; }
		else { reply.stop = AssistantReply::Stop::EndTurn; }
		return reply;
	}
}
