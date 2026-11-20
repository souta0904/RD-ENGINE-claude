#pragma once
#include "Core/Log/LogSink.h"
#include <concepts>
#include <format>
#include <functional>
#include <memory>
#include <string_view>
#include <type_traits>

namespace rd
{
	// フォーマット文字列と呼び出し位置をまとめて受け取る（Log::Info("x={}", x) の "x={}" の部分）
	//
	//  なぜこんな型が要るのか
	//    ログに「どのファイルの何行目で出したか」を載せたい。source_location::current() を既定引数に書くと、
	//    呼び出した側の位置が入る。ただし可変長の引数（Args&&... args）の後ろには既定引数を置けない。
	//    そこで、先頭のフォーマット文字列を受け取る型のコンストラクタに既定引数として持たせる。
	//    "x={}" から LogFormat へ暗黙に変換されるときに、呼び出した側の位置も一緒に記録される
	//
	//  consteval … コンパイル時にしか呼べない関数。std::format_string もコンパイル時に作る必要があるので、
	//  {} の数と引数の数が合わないと、実行する前にコンパイルエラーで分かる
	//  requires … 文字列として扱える型（文字列リテラル、std::string_view など）だけを受け付ける条件
	template <class... Args>
	struct LogFormat
	{
		template <class S> requires std::convertible_to<const S&, std::string_view>
		consteval LogFormat(const S& str, std::source_location loc = std::source_location::current())
			: fmt(str), location(loc) {}
		std::format_string<Args...> fmt;
		std::source_location location;
	};
	// type_identity_t … 「この引数から Args を推論しない」ための印。
	// Args は後ろの実際の値（args）からだけ決め、フォーマット文字列はその Args に合わせて変換させる
	template <class... Args>
	using LogFormatOf = LogFormat<std::type_identity_t<Args>...>;

	// スレッドセーフなロガー（どのスレッドから呼んでもよい）
	//  Log::Info(LogCat::Render, "Loaded {}", path);  カテゴリを省略すると General
	//  書き出しはログ専用スレッドで行うので、呼んだ側はほとんど待たない（Error 以上は書き出しまで待つ）
	//
	//  なぜ専用スレッドで書くのか
	//    ファイルへの書き込みは遅い（数十マイクロ秒～）。ゲームのメインスレッドで毎回待つと、
	//    ログが多いフレームでカクつく。キューに入れるだけにして、書くのは別のスレッドに任せる
	class RD_API Log
	{
	public:
		// ファイルへの出力を始める（それまでのログは履歴に残っているので、ファイルの先頭にまとめて書かれる）
		static void Initialize(const LogSettings& settings = {});
		static void Shutdown();
		// キューに残っているログを書き終えるまで待つ
		static void Flush();

		// 実行中にレベルを変える（-loglevel debug などの起動の引数や、エディタの設定から）
		static void SetLevel(LogLevel level);
		static LogLevel GetLevel();
		static bool IsEnabled(LogLevel level);

		static void AddSink(std::shared_ptr<LogSink> sink);
		static void RemoveSink(const std::shared_ptr<LogSink>& sink);
		// 今書いているログファイルのパス（エディタの「ログを開く」などで使う）
		static const std::string& GetFilePath();

		// 書式を済ませた文字列を 1 件記録する（普段は下の Info などを使い、直接は呼ばない）
		static void Write(LogLevel level, LogCategory category, const std::source_location& location, std::string message);

		// Trace / Debug / Info / Warning / Error の 5 つは中身が同じなので、マクロで 5 回書き出す
		//  1 つ目: カテゴリありの Name(category, fmt, args...)
		//  2 つ目: カテゴリなしの Name(fmt, args...)。General を付けて 1 つ目を呼ぶ
		//  if constexpr … コンパイル時に条件を決める if。kLogCompileLevel より軽いレベルは中身ごと消える
		//  IsEnabled を先に調べるのは、出さないログのために std::format（文字列を作る重い処理）をしないため
		//  std::forward … 受け取った引数を、そのままの種類（一時オブジェクトかどうか）で次へ渡す決まった書き方
#define RD_LOG_LEVEL_FUNC(Name, Level)                                                                     \
		template <class... Args>                                                                           \
		static void Name(LogCategory category, LogFormatOf<Args...> fmt, Args&&... args)                   \
		{                                                                                                  \
			if constexpr (Level >= kLogCompileLevel)                                                       \
			{                                                                                              \
				if (IsEnabled(Level))                                                                      \
				{                                                                                          \
					Write(Level, category, fmt.location, std::format(fmt.fmt, std::forward<Args>(args)...)); \
				}                                                                                          \
			}                                                                                              \
		}                                                                                                  \
		template <class... Args>                                                                           \
		static void Name(LogFormatOf<Args...> fmt, Args&&... args)                                         \
		{                                                                                                  \
			Name<Args...>(LogCat::General, fmt, std::forward<Args>(args)...);                              \
		}
		RD_LOG_LEVEL_FUNC(Trace, LogLevel::Trace)
		RD_LOG_LEVEL_FUNC(Debug, LogLevel::Debug)
		RD_LOG_LEVEL_FUNC(Info, LogLevel::Info)
		RD_LOG_LEVEL_FUNC(Warning, LogLevel::Warning)
		RD_LOG_LEVEL_FUNC(Error, LogLevel::Error)
		// 使い終わったマクロは消す（他のファイルで同じ名前とぶつからないように）
#undef RD_LOG_LEVEL_FUNC

		// 続けられないときのログ。書いた後にプログラムを終了する（クラッシュハンドラがレポートを残す）
		// [[noreturn]] … この関数から戻らないことをコンパイラに教える（後ろのコードの「値を返していない」警告が消える）
		template <class... Args>
		[[noreturn]] static void Fatal(LogCategory category, LogFormatOf<Args...> fmt, Args&&... args)
		{
			Write(LogLevel::Fatal, category, fmt.location, std::format(fmt.fmt, std::forward<Args>(args)...));
			Abort();
		}

		// エディタのコンソール用の履歴（直近 2000 件。コールバック中はロックされる）
		static void ForEach(const std::function<void(const LogRecord&)>& func);
		static void Clear();
		// 履歴が変わるたびに増える番号。コンソールは前回と同じなら作り直さない
		static uint64_t GetVersion();

	private:
		[[noreturn]] static void Abort();
	};
}
