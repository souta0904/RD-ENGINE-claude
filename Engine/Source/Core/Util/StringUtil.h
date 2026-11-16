#pragma once
#include <string>
#include <string_view>
#include <vector>

// 文字列とパスの操作（パスの区切りは '/' にそろえて扱う）
namespace rd
{
	std::wstring ToWide(std::string_view utf8);
	std::string ToUtf8(std::wstring_view wide);

	std::string NormalizePath(std::string_view path);
	// "a/b/c.png" -> "c.png"
	std::string GetFileName(std::string_view path);
	// "a/b/c.png" -> "c"
	std::string GetStem(std::string_view path);
	// "a/b/c.PNG" -> ".png"（小文字）
	std::string GetExtension(std::string_view path);
	// "a/b/c.png" -> "a/b/"
	std::string GetDirectory(std::string_view path);

	std::string ToLower(std::string_view str);
	bool EndsWith(std::string_view str, std::string_view suffix);
	bool ContainsNoCase(std::string_view text, std::string_view pattern);
	std::vector<std::string> Split(std::string_view str, char delimiter);
}
