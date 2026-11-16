#include "RdPch.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	std::wstring ToWide(std::string_view utf8)
	{
		if (utf8.empty()) { return {}; }
		int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
		std::wstring result(size_t(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), result.data(), size);
		return result;
	}

	std::string ToUtf8(std::wstring_view wide)
	{
		if (wide.empty()) { return {}; }
		int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), nullptr, 0, nullptr, nullptr);
		std::string result(size_t(size), '\0');
		WideCharToMultiByte(CP_UTF8, 0, wide.data(), int(wide.size()), result.data(), size, nullptr, nullptr);
		return result;
	}

	std::string NormalizePath(std::string_view path)
	{
		std::string result(path);
		std::replace(result.begin(), result.end(), '\\', '/');
		return result;
	}

	std::string GetFileName(std::string_view path)
	{
		auto pos = path.find_last_of("/\\");
		return std::string(pos == std::string_view::npos ? path : path.substr(pos + 1));
	}

	std::string GetStem(std::string_view path)
	{
		std::string name = GetFileName(path);
		auto pos = name.find_last_of('.');
		return pos == std::string::npos ? name : name.substr(0, pos);
	}

	std::string GetExtension(std::string_view path)
	{
		std::string name = GetFileName(path);
		auto pos = name.find_last_of('.');
		return pos == std::string::npos ? std::string() : ToLower(name.substr(pos));
	}

	std::string GetDirectory(std::string_view path)
	{
		auto pos = path.find_last_of("/\\");
		return pos == std::string_view::npos ? std::string() : NormalizePath(path.substr(0, pos + 1));
	}

	std::string ToLower(std::string_view str)
	{
		std::string result(str);
		for (char& c : result) { c = char(std::tolower(static_cast<unsigned char>(c))); }
		return result;
	}

	bool EndsWith(std::string_view str, std::string_view suffix)
	{
		return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
	}

	bool ContainsNoCase(std::string_view text, std::string_view pattern)
	{
		return ToLower(text).find(ToLower(pattern)) != std::string::npos;
	}

	std::vector<std::string> Split(std::string_view str, char delimiter)
	{
		std::vector<std::string> result;
		size_t start = 0;
		while (true)
		{
			size_t pos = str.find(delimiter, start);
			result.emplace_back(str.substr(start, pos - start));
			if (pos == std::string_view::npos) { break; }
			start = pos + 1;
		}
		return result;
	}
}
