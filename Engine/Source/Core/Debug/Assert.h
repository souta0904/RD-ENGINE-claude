#pragma once
#include <format>
#include <string>

// アサート（「ここでは必ずこうなっているはず」という前提を確かめる仕組み）
//
//   RD_ASSERT(index < size, "index {} is out of range ({})", index, size);
//
// 条件が false なら、ログに式・ファイル・行・メッセージを出して、デバッガをその行で止める。
// メッセージは省略してもよく、書くときは std::format と同じ {} の書式を使える。
//
//   RD_ASSERT … Debug 構成だけで調べる。Release / Game では消える（条件の式も実行されない）。
//               プログラムの間違いを見つけるためのもの。重い確認を書いても製品版の速さに影響しない
//   RD_VERIFY … どの構成でも調べ、失敗したらプログラムを終了する。
//               続けると壊れたデータで動いてしまう、致命的な状態の確認に使う
//
// どちらも、ファイルが見つからないなど起こりうる失敗には使わない（それはログを出して処理を続ける）
namespace rd
{
	// 失敗をログに出す（中身は Core/Log/Logger.cpp にある。Assert.h から Logger.h を読み込まないようにするため）
	RD_API void ReportAssertFailure(const char* expr, const char* file, int line, const std::string& message);
}

// マクロの書き方について
//   #cond      … 条件の式を文字列にする（ログに "index < size" のように式そのものが出る）
//   __FILE__ / __LINE__ … マクロを書いた場所のファイル名と行番号
//   __VA_ARGS__ … マクロの 2 つ目以降の引数（メッセージとその値）をそのまま渡す
//   do { ... } while (false) … 複数の文を 1 つの文として扱うための決まった書き方。
//                  if (x) RD_ASSERT(...); else ... と書いても、else の対応が崩れない
//   各行末の \ … マクロを複数行に分けて書くための印
#if RD_DEBUG
#define RD_ASSERT(cond, ...)                                                                   \
	do                                                                                         \
	{                                                                                          \
		if (!(cond))                                                                           \
		{                                                                                      \
			::rd::ReportAssertFailure(#cond, __FILE__, __LINE__, ::rd::detail::AssertMessage(__VA_ARGS__)); \
			__debugbreak();                                                                    \
		}                                                                                      \
	} while (false)
#else
// sizeof の中の式は実行されない。条件だけで使っている変数が「使われていない」と警告されるのを防ぎつつ、
// 式が正しく書けているかのコンパイルの確認だけは残す
#define RD_ASSERT(cond, ...) do { (void)sizeof(cond); } while (false)
#endif

// リリースでも評価され、失敗したら終了する
// デバッガがつながっていれば先に止まる（つながっていないときに __debugbreak するとそこで落ちるため確かめる）。
// std::abort はクラッシュハンドラが受け取り、落ちた時のレポートとダンプを残す
#define RD_VERIFY(cond, ...)                                                                   \
	do                                                                                         \
	{                                                                                          \
		if (!(cond))                                                                           \
		{                                                                                      \
			::rd::ReportAssertFailure(#cond, __FILE__, __LINE__, ::rd::detail::AssertMessage(__VA_ARGS__)); \
			if (IsDebuggerPresent()) { __debugbreak(); }                                       \
			std::abort();                                                                      \
		}                                                                                      \
	} while (false)

// マクロの中だけで使う補助（detail は「中の実装なので直接使わない」という意味の名前空間）
namespace rd::detail
{
	// メッセージを省略したとき（RD_ASSERT(cond) と書いたとき）はこちらが選ばれる
	inline std::string AssertMessage() { return {}; }

	// メッセージがあるときは std::format で文字列にする
	// format_string を使うと、{} の数と引数の数が合わないときにコンパイルエラーで気づける
	template <class... Args>
	std::string AssertMessage(std::format_string<Args...> fmt, Args&&... args)
	{
		return std::format(fmt, std::forward<Args>(args)...);
	}
}
