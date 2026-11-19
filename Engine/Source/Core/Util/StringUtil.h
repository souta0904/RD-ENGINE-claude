#pragma once
#include <string>
#include <string_view>
#include <vector>

// 文字列とパスの操作
//
//  文字コードについて
//    エンジンの中の文字列は、すべて UTF-8 の std::string で持つ（日本語のファイル名も、そのまま入れられる）。
//    ただし Windows の API（CreateFileW、MessageBoxW など末尾が W のもの）は UTF-16 の wchar_t の文字列を受け取るので、
//    API へ渡す直前に ToWide で変換し、API から受け取った文字列は ToUtf8 で戻す。
//
//  パスについて
//    区切りは '/' にそろえて扱う（Windows は '\' でも '/' でも開けるので '/' で困らない）。
//    同じファイルを指すパスが同じ文字列になるので、アセットのキャッシュのキーにそのまま使える
//
//  引数の std::string_view は「文字列を見るだけの軽い参照」。std::string・文字列リテラル・部分文字列の
//  どれを渡してもコピーが起きないので、読むだけの引数に使う
namespace rd
{
	// UTF-8 → UTF-16（Windows の W 版の API に渡すとき）
	std::wstring ToWide(std::string_view utf8);
	// UTF-16 → UTF-8（Windows の API から受け取った文字列をエンジンで使うとき）
	std::string ToUtf8(std::wstring_view wide);

	// '\' を '/' に置き換える。"a/./b" や ".." はまとめず、大文字と小文字もそのまま
	// （大文字と小文字だけが違うパスは、別のキーとして扱われるので注意）
	std::string NormalizePath(std::string_view path);
	// "a/b/c.png" -> "c.png"
	std::string GetFileName(std::string_view path);
	// "a/b/c.png" -> "c"（拡張子を除いたファイル名。stem は「幹」の意味）
	std::string GetStem(std::string_view path);
	// "a/b/c.PNG" -> ".png"（比べやすいよう小文字にし、'.' も付ける）
	// ".gitignore" のように '.' で始まる名前は、全体が拡張子として扱われる
	std::string GetExtension(std::string_view path);
	// "a/b/c.png" -> "a/b/"（最後の '/' を残す。区切りがなければ空）
	std::string GetDirectory(std::string_view path);

	// 英字（ASCII）だけを小文字にする。日本語などはそのまま
	std::string ToLower(std::string_view str);
	// str が suffix で終わるか（例: EndsWith(path, ".png")）
	bool EndsWith(std::string_view str, std::string_view suffix);
	// 大文字と小文字を区別せずに、text の中に pattern があるか（エディタの検索欄で使う）
	bool ContainsNoCase(std::string_view text, std::string_view pattern);
	// delimiter で区切って配列にする。"a,b,,c" -> ["a", "b", "", "c"]
	// 区切りが続くと空の要素も入る。空の文字列を渡すと、空の要素が 1 つだけ入る
	std::vector<std::string> Split(std::string_view str, char delimiter);
}
