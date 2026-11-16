#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// ファイル全体の読み書き（パスは UTF-8）
namespace rd
{
	bool ReadTextFile(const std::string& path, std::string& out);
	bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out);
	// 親のフォルダがなければ作る
	bool WriteTextFile(const std::string& path, std::string_view text);
	bool FileExists(const std::string& path);
}
