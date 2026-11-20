#pragma once
#include "Platform/Network/HttpResponse.h"
#include <atomic>
#include <utility>
#include <vector>

namespace rd
{
	// HTTP / HTTPS の通信（WinHTTP）。呼んだスレッドで終わるまで待つので、ワーカースレッドから使う
	class RD_API HttpClient
	{
	public:
		using Headers = std::vector<std::pair<std::string, std::string>>;

		// url は "https://host/path" の形。cancel が true になったら途中でやめる
		static HttpResponse Post(const std::string& url, const Headers& headers, const std::string& body,
			int timeoutMs = 600000, const std::atomic<bool>* cancel = nullptr);
		static HttpResponse Get(const std::string& url, const Headers& headers = {}, int timeoutMs = 5000);

	private:
		static HttpResponse Send(const wchar_t* method, const std::string& url, const Headers& headers, const std::string& body,
			int timeoutMs, const std::atomic<bool>* cancel);
	};
}
