#pragma once
#include <atomic>
#include <string>
#include <utility>
#include <vector>

namespace rd
{
	struct HttpResponse
	{
		int status = 0;// 0 なら通信できなかった（error に理由）
		std::string body;
		std::string error;
		std::string retryAfter;// 429 / 503 のときの retry-after ヘッダ

		bool Ok() const { return status >= 200 && status < 300; }
	};

	// HTTP / HTTPS の通信（WinHTTP）。呼んだスレッドで完了まで待つので、ワーカースレッドから使う
	class HttpClient
	{
	public:
		using Headers = std::vector<std::pair<std::string, std::string>>;

		// url は "https://host/path" や "http://localhost:11434/api/chat" の形。cancel が true になったら途中で打ち切る
		static HttpResponse Post(const std::string& url, const Headers& headers, const std::string& body,
			int timeoutMs = 600000, const std::atomic<bool>* cancel = nullptr);
		static HttpResponse Get(const std::string& url, const Headers& headers = {}, int timeoutMs = 5000);

	private:
		static HttpResponse Send(const wchar_t* method, const std::string& url, const Headers& headers, const std::string& body,
			int timeoutMs, const std::atomic<bool>* cancel);
	};
}
