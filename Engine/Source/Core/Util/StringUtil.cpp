#include "RdPch.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	// MultiByteToWideChar は 2 回呼ぶのが決まった使い方
	//   1 回目: 書き込み先に nullptr を渡すと、変換後の文字数だけを返す
	//   2 回目: その文字数の入れ物を用意して、実際に変換する
	// 長さを int(utf8.size()) で渡しているので、末尾の '\0' は数に入らない（std::wstring が自分で持つ）
	std::wstring ToWide(std::string_view utf8)
	{
		if (utf8.empty()) { return {}; }
		int size = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), nullptr, 0);
		std::wstring result(size_t(size), L'\0');
		MultiByteToWideChar(CP_UTF8, 0, utf8.data(), int(utf8.size()), result.data(), size);
		return result;
	}

	// ToWide の逆向き。WideCharToMultiByte も同じく「数を数える → 変換する」の 2 回
	// 後ろの 2 つの nullptr は、UTF-8 では使わない引数（変換できない文字の代わりの文字の指定）
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

	// find_last_of("/\\") は「'/' か '\' のどちらか」が最後に出てくる位置を探す
	// （NormalizePath していないパスが来てもよいように、両方の区切りを見る）
	std::string GetFileName(std::string_view path)
	{
		auto pos = path.find_last_of("/\\");
		return std::string(pos == std::string_view::npos ? path : path.substr(pos + 1));
	}

	// 先にファイル名だけにしてから '.' を探す
	// パスのまま探すと "a.b/c" のようにフォルダ名の '.' を拡張子と取り違えるため
	std::string GetStem(std::string_view path)
	{
		std::string name = GetFileName(path);
		auto pos = name.find_last_of('.');
		return pos == std::string::npos ? name : name.substr(0, pos);
	}

	// 最後の '.' から後ろ（"c.tar.gz" なら ".gz"）
	std::string GetExtension(std::string_view path)
	{
		std::string name = GetFileName(path);
		auto pos = name.find_last_of('.');
		return pos == std::string::npos ? std::string() : ToLower(name.substr(pos));
	}

	// 区切りを含めて切り出すので pos + 1 文字（"a/b/c.png" なら "a/b/"）
	std::string GetDirectory(std::string_view path)
	{
		auto pos = path.find_last_of("/\\");
		return pos == std::string_view::npos ? std::string() : NormalizePath(path.substr(0, pos + 1));
	}

	// std::tolower に char をそのまま渡すと、日本語の UTF-8 のバイト（0x80 以上）が負の数になり、
	// 未定義の動作になる。そのため unsigned char にしてから渡す
	std::string ToLower(std::string_view str)
	{
		std::string result(str);
		for (char& c : result) { c = char(std::tolower(static_cast<unsigned char>(c))); }
		return result;
	}

	// 末尾の suffix.size() 文字を切り出して比べる（先に長さを確かめないと substr が範囲外になる）
	bool EndsWith(std::string_view str, std::string_view suffix)
	{
		return str.size() >= suffix.size() && str.substr(str.size() - suffix.size()) == suffix;
	}

	// 両方を小文字にしてから探す。呼ぶたびに 2 つのコピーを作るので、大量の文字列を毎フレーム探す用途には向かない
	bool ContainsNoCase(std::string_view text, std::string_view pattern)
	{
		return ToLower(text).find(ToLower(pattern)) != std::string::npos;
	}

	// start から次の区切りまでを 1 つずつ取り出す
	// 区切りが見つからない（npos）ときは、substr が残りの全部を返すので、それを最後の要素にして終わる
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
