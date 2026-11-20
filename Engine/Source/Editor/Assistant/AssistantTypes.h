#pragma once
#include "Core/Serialization/Json.h"
#include "Platform/Network/HttpClient.h"
#include <string>
#include <vector>

// AI アシスタントの会話の形（どの LLM でも同じ形で持つ）
namespace rd
{
	struct AssistantToolCall
	{
		std::string id;
		std::string name;
		Json input = Json::object();
	};

	struct AssistantToolResult
	{
		std::string id;
		std::string name;
		std::string content;
		bool isError = false;
	};

	// text は User / Assistant の本文、toolCalls は Assistant が呼んだツール、results は ToolResults
	// raw はその LLM が返したそのままの形（同じ LLM へはこれを送り返す。Claude の thinking を保つため）
	struct RD_API AssistantTurn
	{
		enum class Role { User, Assistant, ToolResults } role = Role::User;
		std::string text;
		std::vector<AssistantToolCall> toolCalls;
		std::vector<AssistantToolResult> results;
		Json raw;
	};

	// LLM の返事 1 回分（usedFallback は Claude が別のモデルに切り替えて答えたとき）
	struct RD_API AssistantReply
	{
		enum class Stop { EndTurn, ToolUse, MaxTokens, Refusal, Error } stop = Stop::Error;
		AssistantTurn turn;
		std::string error;
		bool usedFallback = false;
		uint64_t inputTokens = 0;
		uint64_t outputTokens = 0;
		uint64_t cacheReadTokens = 0;
	};

	// 送る HTTP のリクエスト
	struct AssistantRequest
	{
		std::string url;
		HttpClient::Headers headers;
		std::string body;
	};
}
