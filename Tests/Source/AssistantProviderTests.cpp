#include "TestFramework.h"

#if RD_EDITOR
#include "Editor/AssistantProviders.h"
#include "Editor/AssistantTools.h"

using namespace rd;

// LLM ごとの変換（会話 → リクエスト、返事 → 会話）を、サーバーなしで確かめる
namespace
{
	std::vector<AssistantTurn> SampleHistory()
	{
		std::vector<AssistantTurn> h;
		AssistantTurn user;
		user.role = AssistantTurn::Role::User;
		user.text = "make a cube";
		h.push_back(user);
		AssistantTurn assistant;
		assistant.role = AssistantTurn::Role::Assistant;
		assistant.text = "Creating it.";
		assistant.toolCalls.push_back({ "call_1", "create_actor", { { "name", "Cube" } } });
		h.push_back(assistant);
		AssistantTurn results;
		results.role = AssistantTurn::Role::ToolResults;
		results.results.push_back({ "call_1", "create_actor", "{\"created_id\":3}", false });
		h.push_back(results);
		return h;
	}

	AssistantSettings LocalSettings(AssistantProviderKind kind)
	{
		AssistantSettings s;
		s.provider = kind;
		s.localModel = "qwen3:8b";
		return s;
	}

	HttpResponse Ok(const Json& body)
	{
		HttpResponse r;
		r.status = 200;
		r.body = body.dump();
		return r;
	}
}

RD_TEST(AssistantProvider, OllamaRequestShape)
{
	auto provider = CreateAssistantProvider(AssistantProviderKind::Ollama);
	AssistantRequest req;
	std::string error;
	RD_REQUIRE(provider->BuildRequest(SampleHistory(), "SYSTEM", AssistantTools::GetDefinitions(), LocalSettings(AssistantProviderKind::Ollama), req, error));
	RD_CHECK_EQ(req.url, std::string("http://localhost:11434/api/chat"));
	Json body = Json::parse(req.body);
	RD_CHECK_EQ(body["model"].get<std::string>(), std::string("qwen3:8b"));
	RD_CHECK(body["stream"] == false);
	RD_CHECK(body["options"]["num_ctx"].get<int>() >= 2048);
	const Json& m = body["messages"];
	RD_REQUIRE(m.size() == 4);
	RD_CHECK(m[0]["role"] == "system" && m[0]["content"] == "SYSTEM");
	RD_CHECK(m[2]["tool_calls"][0]["function"]["arguments"].is_object());// Ollama は引数を JSON のまま
	RD_CHECK(m[3]["role"] == "tool" && m[3]["tool_name"] == "create_actor");
	// ツール定義は OpenAI の function の形
	RD_CHECK(body["tools"][0]["type"] == "function");
	RD_CHECK(body["tools"][0]["function"]["parameters"]["type"] == "object");
}

RD_TEST(AssistantProvider, OpenAICompatibleRequestShape)
{
	auto provider = CreateAssistantProvider(AssistantProviderKind::OpenAICompatible);
	AssistantRequest req;
	std::string error;
	AssistantSettings s = LocalSettings(AssistantProviderKind::OpenAICompatible);
	s.openAIUrl = "http://localhost:1234/v1/";// 末尾の / は取り除く
	RD_REQUIRE(provider->BuildRequest(SampleHistory(), "SYSTEM", AssistantTools::GetDefinitions(), s, req, error));
	RD_CHECK_EQ(req.url, std::string("http://localhost:1234/v1/chat/completions"));
	Json body = Json::parse(req.body);
	const Json& call = body["messages"][2]["tool_calls"][0];
	RD_CHECK(call["id"] == "call_1" && call["type"] == "function");
	RD_CHECK(call["function"]["arguments"].is_string());// OpenAI 互換は文字列にした JSON
	RD_CHECK(body["messages"][3]["tool_call_id"] == "call_1");
	RD_CHECK(!body.contains("options"));

	// モデルを選んでいなければ送らない
	s.localModel.clear();
	RD_CHECK(!provider->BuildRequest(SampleHistory(), "SYSTEM", AssistantTools::GetDefinitions(), s, req, error));
	RD_CHECK(!error.empty());
}

RD_TEST(AssistantProvider, ParseOllamaToolCalls)
{
	auto provider = CreateAssistantProvider(AssistantProviderKind::Ollama);
	AssistantReply reply = provider->ParseResponse(Ok({
		{ "message", { { "role", "assistant" }, { "content", "<think>planning...</think>\nOK" },
			{ "tool_calls", Json::array({ { { "function", { { "name", "get_scene" }, { "arguments", Json::object() } } } } }) } } },
		{ "done_reason", "stop" }, { "prompt_eval_count", 120 }, { "eval_count", 30 } }));
	RD_CHECK(reply.error.empty());
	RD_CHECK(reply.stop == AssistantReply::Stop::ToolUse);// done_reason が stop でもツールを呼んでいれば ToolUse
	RD_CHECK_EQ(reply.turn.text, std::string("OK"));// <think> は取り除く
	RD_REQUIRE(reply.turn.toolCalls.size() == 1);
	RD_CHECK_EQ(reply.turn.toolCalls[0].name, std::string("get_scene"));
	RD_CHECK(!reply.turn.toolCalls[0].id.empty());// id がなければ振る
	RD_CHECK_EQ(reply.inputTokens, uint64_t(120));
}

RD_TEST(AssistantProvider, ParseOpenAIResponse)
{
	auto provider = CreateAssistantProvider(AssistantProviderKind::OpenAICompatible);
	AssistantReply reply = provider->ParseResponse(Ok({
		{ "choices", Json::array({ { { "finish_reason", "tool_calls" }, { "message", {
			{ "role", "assistant" }, { "content", nullptr },
			{ "tool_calls", Json::array({
				{ { "id", "abc" }, { "type", "function" }, { "function", { { "name", "delete_actor" }, { "arguments", "{\"actor_id\": 5}" } } } },
				{ { "id", "bad" }, { "type", "function" }, { "function", { { "name", "get_actor" }, { "arguments", "{not json" } } } },
			}) } } } } }) },
		{ "usage", { { "prompt_tokens", 10 }, { "completion_tokens", 2 } } } }));
	RD_REQUIRE(reply.turn.toolCalls.size() == 2);
	RD_CHECK_EQ(reply.turn.toolCalls[0].input.value("actor_id", 0), 5);
	RD_CHECK(reply.turn.toolCalls[1].input.contains("__invalid_arguments"));// 壊れた引数は印を付けて返す

	AssistantReply text = provider->ParseResponse(Ok({ { "choices", Json::array({ { { "finish_reason", "length" },
		{ "message", { { "role", "assistant" }, { "content", "partial" } } } } }) } }));
	RD_CHECK(text.stop == AssistantReply::Stop::MaxTokens);

	HttpResponse err;
	err.status = 404;
	err.body = R"({"error":{"message":"model not found"}})";
	RD_CHECK(provider->ParseResponse(err).error.find("model not found") != std::string::npos);
	HttpResponse down;
	down.error = "HTTP request failed (error 12029)";
	RD_CHECK(!provider->ParseResponse(down).error.empty());
}

RD_TEST(AssistantProvider, ClaudeKeepsRawContent)
{
	// Claude は返した形（thinking を含む）をそのまま送り返す
	auto provider = CreateAssistantProvider(AssistantProviderKind::Claude);
	Json content = Json::array({
		{ { "type", "thinking" }, { "thinking", "" }, { "signature", "sig" } },
		{ { "type", "tool_use" }, { "id", "toolu_1" }, { "name", "get_scene" }, { "input", Json::object() } } });
	AssistantReply reply = provider->ParseResponse(Ok({ { "content", content }, { "stop_reason", "tool_use" },
		{ "usage", { { "input_tokens", 5 }, { "cache_read_input_tokens", 100 }, { "output_tokens", 7 } } } }));
	RD_CHECK(reply.stop == AssistantReply::Stop::ToolUse);
	RD_CHECK(reply.turn.raw == content);
	RD_CHECK_EQ(reply.cacheReadTokens, uint64_t(100));

	AssistantReply refused = provider->ParseResponse(Ok({ { "content", Json::array() }, { "stop_reason", "refusal" } }));
	RD_CHECK(refused.stop == AssistantReply::Stop::Refusal);
}

RD_TEST(AssistantProvider, SettingsRoundTrip)
{
	AssistantSettings a;
	a.provider = AssistantProviderKind::OpenAICompatible;
	a.localModel = "llama3.1";
	a.contextSize = 32768;
	AssistantSettings b;
	b.FromJson(a.ToJson());
	RD_CHECK(b.provider == AssistantProviderKind::OpenAICompatible);
	RD_CHECK_EQ(b.localModel, std::string("llama3.1"));
	RD_CHECK_EQ(b.contextSize, 32768);
	RD_CHECK(a.ToJson().dump().find("key") == std::string::npos);// API キーは保存しない
}
#endif
