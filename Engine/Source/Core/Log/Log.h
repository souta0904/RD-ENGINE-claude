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
	//  既定引数の source_location::current() が呼び出した側の位置になる
	template <class... Args>
	struct LogFormat
	{
		template <class S> requires std::convertible_to<const S&, std::string_view>
		consteval LogFormat(const S& str, std::source_location loc = std::source_location::current())
			: fmt(str), location(loc) {}
		std::format_string<Args...> fmt;
		std::source_location location;
	};
	template <class... Args>
	using LogFormatOf = LogFormat<std::type_identity_t<Args>...>;

	// スレッドセーフなロガー
	//  Log::Info(LogCat::Render, "Loaded {}", path);  カテゴリを省略すると General
	//  書き出しはログ専用スレッドで行うので、呼んだ側はほとんど待たない（Error 以上は書き出しまで待つ）
	class Log
	{
	public:
		static void Initialize(const LogSettings& settings = {});
		static void Shutdown();
		static void Flush();

		static void SetLevel(LogLevel level);
		static LogLevel GetLevel();
		static bool IsEnabled(LogLevel level);

		static void AddSink(std::shared_ptr<LogSink> sink);
		static void RemoveSink(const std::shared_ptr<LogSink>& sink);
		static const std::string& GetFilePath();

		static void Write(LogLevel level, LogCategory category, const std::source_location& location, std::string message);

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
#undef RD_LOG_LEVEL_FUNC

		template <class... Args>
		[[noreturn]] static void Fatal(LogCategory category, LogFormatOf<Args...> fmt, Args&&... args)
		{
			Write(LogLevel::Fatal, category, fmt.location, std::format(fmt.fmt, std::forward<Args>(args)...));
			Abort();
		}

		// エディタのコンソール用の履歴（コールバック中はロックされる）
		static void ForEach(const std::function<void(const LogRecord&)>& func);
		static void Clear();
		static uint64_t GetVersion();

	private:
		[[noreturn]] static void Abort();
	};
}
