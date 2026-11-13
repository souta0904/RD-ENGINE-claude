#pragma once
#include <concepts>
#include <cstdint>
#include <format>
#include <functional>
#include <memory>
#include <source_location>
#include <string>
#include <string_view>
#include <type_traits>

namespace rd
{
	// ログの重要度（下ほど重要）
	enum class LogLevel : uint8_t
	{
		Trace,// 毎フレーム出るような細かい追跡
		Debug,// 開発中だけ見たい情報
		Info,// 起動・読み込みなどの節目
		Warning,// 続行できるが直すべきこと
		Error,// 機能が動かなかった（ファイルへ即座に書き出す）
		Fatal,// 続行できない（書き出して終了する）
	};
	const char* ToString(LogLevel level);

	// コンパイル時に除くレベル（Game ビルドでは Debug 以下を消す）
#ifndef RD_LOG_COMPILE_LEVEL
#if !RD_EDITOR && !RD_DEBUG
#define RD_LOG_COMPILE_LEVEL 2
#else
#define RD_LOG_COMPILE_LEVEL 0
#endif
#endif
	inline constexpr LogLevel kLogCompileLevel = LogLevel(RD_LOG_COMPILE_LEVEL);

	// ログのカテゴリ（どの機能から出たか）
	struct LogCategory
	{
		const char* name;
	};
	namespace LogCat
	{
		inline constexpr LogCategory General{ "General" };
		inline constexpr LogCategory Core{ "Core" };
		inline constexpr LogCategory Platform{ "Platform" };
		inline constexpr LogCategory RHI{ "RHI" };
		inline constexpr LogCategory Render{ "Render" };
		inline constexpr LogCategory Asset{ "Asset" };
		inline constexpr LogCategory Audio{ "Audio" };
		inline constexpr LogCategory Physics{ "Physics" };
		inline constexpr LogCategory Scene{ "Scene" };
		inline constexpr LogCategory Editor{ "Editor" };
		inline constexpr LogCategory Game{ "Game" };
	}

	// 1 件のログ
	struct LogRecord
	{
		LogLevel level = LogLevel::Info;
		const char* category = "General";
		double time = 0.0;// 起動からの秒数
		uint16_t hour = 0, minute = 0, second = 0, millisecond = 0;// 発生した時刻（ローカル時間）
		uint32_t threadId = 0;
		bool mainThread = false;
		std::source_location location;
		std::string message;
	};

	// ログの出力先（デバッガ・ファイル・エディタのコンソールなど）
	//  Write はログ専用スレッドから呼ばれるので、出力先ごとにロックはいらない
	class LogSink
	{
	public:
		virtual ~LogSink() = default;
		virtual void Write(const LogRecord& record) = 0;
		virtual void Flush() {}
	};

	struct LogSettings
	{
		std::string directory = "Logs";
		std::string fileName = "RdEngine.log";// 今回のログ（前回分は History/ へ日時付きで移す）
		uint32_t keepHistory = 10;// History/ に残す数
		LogLevel minLevel = LogLevel::Debug;// 実行時に出すレベル
	};

	// フォーマット文字列と呼び出し位置をまとめて受け取る（Log::Info("x={}", x) の "x={}" の部分）
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
		// たまっているログをすべて出力先へ書き出すまで待つ
		static void Flush();

		static void SetLevel(LogLevel level);
		static LogLevel GetLevel();
		static bool IsEnabled(LogLevel level);

		static void AddSink(std::shared_ptr<LogSink> sink);
		static void RemoveSink(const std::shared_ptr<LogSink>& sink);
		// 今回のログファイルのパス
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

		// 致命的なエラー: 書き出してから終了する
		template <class... Args>
		[[noreturn]] static void Fatal(LogCategory category, LogFormatOf<Args...> fmt, Args&&... args)
		{
			Write(LogLevel::Fatal, category, fmt.location, std::format(fmt.fmt, std::forward<Args>(args)...));
			Abort();
		}

		// エディタのコンソール用の履歴（コールバック中はロックされる）
		static void ForEach(const std::function<void(const LogRecord&)>& func);
		static void Clear();
		static uint64_t GetVersion();// 履歴が変わるたびに増える

	private:
		[[noreturn]] static void Abort();
	};
}
