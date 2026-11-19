#include "RdPch.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include <fstream>
#include <sstream>

namespace rd
{
	// std::filesystem::path に std::string をそのまま渡すと、Windows では「今の言語の文字コード（日本語なら Shift_JIS）」
	// として読まれ、UTF-8 の日本語が化ける。ToWide で UTF-16 にしてから渡すと正しく開ける
	//
	// どのファイルも std::ios::binary で開く。テキストモードだと読むときに "\r\n" が "\n" に変わり、
	// 書いた内容と読んだ内容がずれるため（ファイルのバイトをそのまま扱う）

	// rdbuf() はファイルの読み込み口。stringstream へ流し込むと、ファイルの最後まで一度に読める
	bool ReadTextFile(const std::string& path, std::string& out)
	{
		std::ifstream file(std::filesystem::path(ToWide(path)), std::ios::binary);
		if (!file) { return false; }
		std::stringstream ss;
		ss << file.rdbuf();
		out = ss.str();
		return true;
	}

	// std::ios::ate は「開いたら最後の位置へ移る」。そこで tellg（今の位置）を聞くとファイルの大きさになる。
	// 大きさの分だけ入れ物を用意してから、先頭（seekg(0)）へ戻って一度に読む
	// 注意: 位置を取れないと tellg は -1 を返すので、size_t にすると巨大な数になる
	bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out)
	{
		std::ifstream file(std::filesystem::path(ToWide(path)), std::ios::binary | std::ios::ate);
		if (!file) { return false; }
		auto size = file.tellg();
		out.resize(size_t(size));
		file.seekg(0);
		file.read(reinterpret_cast<char*>(out.data()), size);
		return bool(file);
	}

	// create_directories は途中のフォルダもまとめて作る（すでにあれば何もしない）
	// error_code を渡す版は、失敗しても例外を投げない（その後の open の失敗で false になる）
	bool WriteTextFile(const std::string& path, std::string_view text)
	{
		std::filesystem::path fsPath(ToWide(path));
		if (fsPath.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::create_directories(fsPath.parent_path(), ec);
		}
		std::ofstream file(fsPath, std::ios::binary);
		if (!file) { return false; }
		file.write(text.data(), std::streamsize(text.size()));
		return bool(file);
	}

	// error_code を渡すので、アクセスできない場所などでも例外にならず false が返る
	bool FileExists(const std::string& path)
	{
		std::error_code ec;
		return std::filesystem::exists(std::filesystem::path(ToWide(path)), ec);
	}
}
