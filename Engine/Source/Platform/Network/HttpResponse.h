#pragma once
#include <string>

namespace rd
{
	// HTTP の応答（status が 0 なら通信できなかった。理由は error、retryAfter は 429 / 503 のときのヘッダ）
	struct HttpResponse
	{
		int status = 0;
		std::string body;
		std::string error;
		std::string retryAfter;

		bool Ok() const { return status >= 200 && status < 300; }
	};
}
