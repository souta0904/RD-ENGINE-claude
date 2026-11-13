#include "RdPch.h"
#include "Editor/AssistantProviders.h"
#include "Core/StringUtil.h"

namespace rd
{
	namespace
	{
		// ローカルの小さなモデルは扱える長さが短いので、大きすぎるツールの結果は切り詰める
		constexpr size_t kMaxLocalToolResult = 12000;

		std::string GetEnv(const wchar_t* name)
		{
			wchar_t buffer[4096];
			DWORD n = GetEnvironmentVariableW(name, buffer, DWORD(std::size(buffer)));
			return n > 0 && n < std::size(buffer) ? ToUtf8(std::wstring(buffer, n)) : std::string();
		}

		std::string TrimSlash(std::string url)
		{
			while (!url.empty() && url.back() == '/') { url.pop_back(); }
			return url;
		}

		// 推論モデル（qwen3・deepseek-r1 など）が本文に混ぜる <think>...</think> を取り除く
		std::string StripThinking(std::string text)
		{
			while (true)
			{
				size_t begin = text.find("<think>");
				if (begin == std::string::npos) { break; }
				size_t end = text.find("</think>", begin);
				text.erase(begin, end == std::string::npos ? std::string::npos : end + 8 - begin);
			}
			size_t first = text.find_first_not_of(" \r\n\t");
			return first == std::string::npos ? std::string() : text.substr(first);
		}

		std::string HttpErrorMessage(const HttpResponse& r)
		{
			if (r.status == 0) { return r.error; }
			try
			{
				Json j = Json::parse(r.body);
				if (auto e = j.find("error"); e != j.end())
				{
					// Claude / OpenAI 互換: { "error": { "message": ... } }、Ollama: { "error": "..." }
					if (e->is_string()) { return std::format("HTTP {}: {}", r.status, e->get<std::string>()); }
					if (e->is_object()) { return std::format("HTTP {} {}: {}", r.status, e->value("type", ""), e->value("message", "")); }
				}
			}
			catch (...)
			{
			}
			return std::format("HTTP {}", r.status);
		}

		// ==================================================
		// Claude（Messages API）
		// ==================================================
		constexpr const char* kClaudeUrl = "https://api.anthropic.com/v1/messages";
		constexpr const char* kClaudeVersion = "2026-10-21";

		// 途中で別のモデルに切り替わった応答を会話に戻すときの決まり:
		//  最後の fallback ブロックより前の thinking / tool_use などは送り返さない（fallback ブロック自体は位置を保つ）
		Json FilterContentForEcho(const Json& content)
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

		class ClaudeProvider final : public AssistantProvider
		{
		public:
			bool BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
				const AssistantSettings& settings, AssistantRequest& out, std::string& error) const override
			{
				std::string apiKey = GetEnv(L"ANTHROPIC_API_KEY");
				std::string authToken = apiKey.empty() ? GetEnv(L"ANTHROPIC_AUTH_TOKEN") : std::string();
				if (apiKey.empty() && authToken.empty())
				{
					error = "Claude を使うには環境変数 ANTHROPIC_API_KEY を設定してから、エディタを起動し直してください（有料）。";
					return false;
				}
				const ClaudeModelOption& model = kClaudeModels[std::clamp(settings.claudeModel, 0, int(std::size(kClaudeModels)) - 1)];

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
						// Claude が返した形をそのまま戻す（thinking ブロックを含めて変えない）
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

				Json body = {
					{ "model", model.id },
					{ "max_tokens", 16000 },
					{ "system", Json::array({ { { "type", "text" }, { "text", system } } }) },
					{ "tools", tools },
					{ "messages", messages },
					{ "output_config", { { "effort", kClaudeEfforts[std::clamp(settings.claudeEffort, 0, 2)] } } },
					// 会話の先頭部分（システムプロンプト・ツール・これまでの履歴）をキャッシュして、ツールの往復を安く速くする
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

			AssistantReply ParseResponse(const HttpResponse& response) const override
			{
				AssistantReply reply;
				reply.turn.role = AssistantTurn::Role::Assistant;
				if (!response.Ok()) { reply.error = HttpErrorMessage(response); return reply; }
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
				reply.stop = stop == "tool_use" ? AssistantReply::Stop::ToolUse
					: stop == "refusal" ? AssistantReply::Stop::Refusal
					: stop == "max_tokens" ? AssistantReply::Stop::MaxTokens
					: AssistantReply::Stop::EndTurn;
				return reply;
			}
		};

		// ==================================================
		// ローカル LLM（Ollama の /api/chat と、OpenAI 互換の /chat/completions）
		//  どちらも OpenAI の「function calling」と同じ形のツールを使う
		// ==================================================
		class LocalProvider final : public AssistantProvider
		{
		public:
			explicit LocalProvider(bool ollama) : mOllama(ollama) {}

			bool BuildRequest(const std::vector<AssistantTurn>& history, const std::string& system, const Json& tools,
				const AssistantSettings& settings, AssistantRequest& out, std::string& error) const override
			{
				if (settings.localModel.empty())
				{
					error = "モデルが選ばれていません。ローカルのサーバーを起動して、モデルの一覧から選んでください。";
					return false;
				}
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
								// Ollama は引数を JSON のまま、OpenAI 互換は文字列にした JSON で受け取る
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

				// Claude の形のツール定義を OpenAI の形へ
				Json functions = Json::array();
				for (const auto& t : tools)
				{
					functions.push_back({ { "type", "function" }, { "function", {
						{ "name", t.value("name", "") }, { "description", t.value("description", "") }, { "parameters", t.value("input_schema", Json::object()) } } } });
				}

				Json body = { { "model", settings.localModel }, { "messages", messages }, { "tools", functions }, { "stream", false } };
				if (mOllama)
				{
					// 既定の文脈の長さでは、ツールの説明とシーンが入りきらない
					body["options"] = { { "num_ctx", std::max(2048, settings.contextSize) } };
					out.url = TrimSlash(settings.ollamaUrl) + "/api/chat";
				}
				else
				{
					out.url = TrimSlash(settings.openAIUrl) + "/chat/completions";
				}
				out.headers = { { "content-type", "application/json" } };
				out.body = body.dump();
				return true;
			}

			AssistantReply ParseResponse(const HttpResponse& response) const override
			{
				AssistantReply reply;
				reply.turn.role = AssistantTurn::Role::Assistant;
				if (response.status == 0 && response.error != "Canceled")
				{
					reply.error = response.error + "\nローカル LLM のサーバーが起動しているか確かめてください（Settings の説明を参照）。";
					return reply;
				}
				if (!response.Ok()) { reply.error = HttpErrorMessage(response); return reply; }
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

				if (auto c = message.find("content"); c != message.end() && c->is_string()) { reply.turn.text = StripThinking(c->get<std::string>()); }
				if (auto calls = message.find("tool_calls"); calls != message.end() && calls->is_array())
				{
					for (const auto& call : *calls)
					{
						const Json& fn = call.value("function", Json::object());
						AssistantToolCall tc;
						tc.name = fn.value("name", "");
						tc.id = call.value("id", "");
						if (tc.id.empty()) { tc.id = std::format("call_{}", NextId()); }
						// 引数は JSON のオブジェクトか、JSON を入れた文字列で届く
						if (auto args = fn.find("arguments"); args != fn.end())
						{
							if (args->is_object()) { tc.input = *args; }
							else if (args->is_string())
							{
								try { tc.input = Json::parse(args->get<std::string>()); }
								catch (...) { tc.input = { { "__invalid_arguments", args->get<std::string>() } }; }
							}
						}
						reply.turn.toolCalls.push_back(std::move(tc));
					}
				}
				// サーバーによっては、ツールを呼んでも finish_reason が "stop" のことがある
				reply.stop = !reply.turn.toolCalls.empty() ? AssistantReply::Stop::ToolUse
					: finish == "length" ? AssistantReply::Stop::MaxTokens
					: AssistantReply::Stop::EndTurn;
				return reply;
			}

		private:
			static uint64_t NextId()
			{
				static std::atomic<uint64_t> next = 1;
				return next++;
			}
			bool mOllama;
		};
	}

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

	std::vector<std::string> ListLocalModels(const AssistantSettings& settings, std::string& error)
	{
		std::vector<std::string> names;
		bool ollama = settings.provider == AssistantProviderKind::Ollama;
		std::string url = ollama ? TrimSlash(settings.ollamaUrl) + "/api/tags" : TrimSlash(settings.openAIUrl) + "/models";
		HttpResponse r = HttpClient::Get(url, {}, 3000);
		if (!r.Ok())
		{
			error = r.status == 0 ? "サーバーに接続できません（" + url + "）" : HttpErrorMessage(r);
			return names;
		}
		try
		{
			Json j = Json::parse(r.body);
			// Ollama: { "models": [ { "name": ... } ] }、OpenAI 互換: { "data": [ { "id": ... } ] }
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
