#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// ファイル全体の読み書き
//  パスは UTF-8 で渡す（中で ToWide して開くので、日本語のフォルダ名やファイル名でも開ける）。
//  失敗（ファイルがない、開けない）は false で返す。起こりうる失敗なので、アサートやログは呼んだ側に任せる
namespace rd
{
	// ファイルの中身をそのまま文字列にする（改行の変換や BOM の除去はしない）
	bool ReadTextFile(const std::string& path, std::string& out);
	// 画像や音声などのバイト列を読む
	bool ReadBinaryFile(const std::string& path, std::vector<uint8_t>& out);
	// 上書きで書く。親のフォルダがなければ作る
	bool WriteTextFile(const std::string& path, std::string_view text);
	// ファイルかフォルダがあるか
	bool FileExists(const std::string& path);
}
