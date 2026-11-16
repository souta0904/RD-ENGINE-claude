#include "RdPch.h"
#include "Editor/Assistant/ProviderUtil.h"
#include "Core/Serialization/Json.h"
#include "Core/Util/StringUtil.h"

namespace rd::assistant
{
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
				if (e->is_string()) { return std::format("HTTP {}: {}", r.status, e->get<std::string>()); }
				if (e->is_object()) { return std::format("HTTP {} {}: {}", r.status, e->value("type", ""), e->value("message", "")); }
			}
		}
		catch (...)
		{
		}
		return std::format("HTTP {}", r.status);
	}
}
