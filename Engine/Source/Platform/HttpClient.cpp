#include "RdPch.h"
#include "Platform/HttpClient.h"
#include "Core/StringUtil.h"
#include <winhttp.h>

#pragma comment(lib, "winhttp.lib")

namespace rd
{
	namespace
	{
		struct Handle
		{
			HINTERNET h = nullptr;
			Handle() = default;
			explicit Handle(HINTERNET handle) : h(handle) {}
			~Handle() { if (h) { WinHttpCloseHandle(h); } }
			Handle(const Handle&) = delete;
			Handle& operator=(const Handle&) = delete;
			explicit operator bool() const { return h != nullptr; }
		};

		std::string LastError(const char* what)
		{
			return std::format("{} failed (error {})", what, GetLastError());
		}
	}

	HttpResponse HttpClient::Post(const std::string& url, const Headers& headers, const std::string& body,
		int timeoutMs, const std::atomic<bool>* cancel)
	{
		return Send(L"POST", url, headers, body, timeoutMs, cancel);
	}

	HttpResponse HttpClient::Get(const std::string& url, const Headers& headers, int timeoutMs)
	{
		return Send(L"GET", url, headers, std::string(), timeoutMs, nullptr);
	}

	HttpResponse HttpClient::Send(const wchar_t* method, const std::string& url, const Headers& headers, const std::string& body,
		int timeoutMs, const std::atomic<bool>* cancel)
	{
		HttpResponse res;
		std::wstring wurl = ToWide(url);
		URL_COMPONENTS parts = { sizeof(parts) };
		wchar_t host[256] = {};
		wchar_t path[2048] = {};
		parts.lpszHostName = host;
		parts.dwHostNameLength = DWORD(std::size(host));
		parts.lpszUrlPath = path;
		parts.dwUrlPathLength = DWORD(std::size(path));
		if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts))
		{
			res.error = "Invalid URL: " + url;
			return res;
		}

		Handle session(WinHttpOpen(L"RdEngine/2.0", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
		if (!session) { res.error = LastError("WinHttpOpen"); return res; }
		WinHttpSetTimeouts(session.h, timeoutMs, timeoutMs, timeoutMs, timeoutMs);

		Handle connection(WinHttpConnect(session.h, host, parts.nPort, 0));
		if (!connection) { res.error = LastError("WinHttpConnect"); return res; }

		DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
		Handle request(WinHttpOpenRequest(connection.h, method, path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
		if (!request) { res.error = LastError("WinHttpOpenRequest"); return res; }

		std::wstring headerText;
		for (const auto& [name, value] : headers) { headerText += ToWide(name + ": " + value + "\r\n"); }
		if (!WinHttpSendRequest(request.h, headerText.empty() ? WINHTTP_NO_ADDITIONAL_HEADERS : headerText.c_str(),
				headerText.empty() ? 0 : DWORD(-1L), body.empty() ? WINHTTP_NO_REQUEST_DATA : const_cast<char*>(body.data()), DWORD(body.size()), DWORD(body.size()), 0) ||
			!WinHttpReceiveResponse(request.h, nullptr))
		{
			res.error = LastError("HTTP request");
			return res;
		}

		DWORD status = 0, size = sizeof(status);
		WinHttpQueryHeaders(request.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
		res.status = int(status);

		wchar_t retry[64] = {};
		DWORD retrySize = sizeof(retry);
		if (WinHttpQueryHeaders(request.h, WINHTTP_QUERY_CUSTOM, L"retry-after", retry, &retrySize, WINHTTP_NO_HEADER_INDEX))
		{
			res.retryAfter = ToUtf8(retry);
		}

		while (true)
		{
			if (cancel && cancel->load())
			{
				res.status = 0;
				res.error = "Canceled";
				return res;
			}
			DWORD available = 0;
			if (!WinHttpQueryDataAvailable(request.h, &available)) { res.error = LastError("Read"); break; }
			if (available == 0) { break; }
			size_t offset = res.body.size();
			res.body.resize(offset + available);
			DWORD read = 0;
			if (!WinHttpReadData(request.h, res.body.data() + offset, available, &read)) { res.error = LastError("Read"); break; }
			res.body.resize(offset + read);
		}
		return res;
	}
}
