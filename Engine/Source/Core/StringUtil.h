#pragma once
#include <string>
#include <string_view>
#include <vector>

namespace rd
{
	// UTF-8 <-> UTF-16
	std::wstring ToWide(std::string_view utf8);
	std::string ToUtf8(std::wstring_view wide);

	// パス操作（区切り文字は '/' に正規化して扱う）
	std::string NormalizePath(std::string_view path);
	std::string GetFileName(std::string_view path);// "a/b/c.png" -> "c.png"
	std::string GetStem(std::string_view path);// "a/b/c.png" -> "c"
	std::string GetExtension(std::string_view path);// "a/b/c.PNG" -> ".png"（小文字）
	std::string GetDirectory(std::string_view path);// "a/b/c.png" -> "a/b/"

	std::string ToLower(std::string_view str);
	bool EndsWith(std::string_view str, std::string_view suffix);
	std::vector<std::string> Split(std::string_view str, char delimiter);

	// ファイル全体を読み込む
	bool ReadTextFile(const std::string& path, std::string& out);
	bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out);
	bool WriteTextFile(const std::string& path, std::string_view text);
}
