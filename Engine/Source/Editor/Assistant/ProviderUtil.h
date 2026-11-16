#pragma once
#include "Platform/Network/HttpResponse.h"
#include <string>

// LLM の変換で使う小さな関数
namespace rd::assistant
{
	std::string GetEnv(const wchar_t* name);
	std::string TrimSlash(std::string url);
	// 推論モデル（qwen3・deepseek-r1 など）が本文に混ぜる <think>...</think> を取り除く
	std::string StripThinking(std::string text);
	// Claude / OpenAI 互換の { "error": { "message": ... } } と、Ollama の { "error": "..." } を読む
	std::string HttpErrorMessage(const HttpResponse& response);
}
