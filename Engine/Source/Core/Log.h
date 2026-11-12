#pragma once
#include <cstdint>
#include <format>
#include <mutex>
#include <string>
#include <vector>

namespace rd
{
	enum class LogLevel : uint8_t
	{
		Info,
		Warning,
		Error
	};

	struct LogEntry
	{
		LogLevel level;
		double time;// 起動からの秒数
		std::string message;
	};

	// スレッドセーフなロガー
	// 出力ウィンドウへ書き出しつつ、エディタのコンソール用に履歴を保持する
	class Log
	{
	public:
		static void Write(LogLevel level, std::string message);

		template <class... Args>
		static void Info(std::format_string<Args...> fmt, Args&&... args)
		{
			Write(LogLevel::Info, std::format(fmt, std::forward<Args>(args)...));
		}
		template <class... Args>
		static void Warning(std::format_string<Args...> fmt, Args&&... args)
		{
			Write(LogLevel::Warning, std::format(fmt, std::forward<Args>(args)...));
		}
		template <class... Args>
		static void Error(std::format_string<Args...> fmt, Args&&... args)
		{
			Write(LogLevel::Error, std::format(fmt, std::forward<Args>(args)...));
		}

		// 履歴へのアクセス（コールバック中はロックされる）
		template <class Func>
		static void ForEach(Func&& func)
		{
			std::lock_guard lock(sMutex);
			for (const auto& e : sEntries) { func(e); }
		}
		static void Clear();
		static uint64_t GetVersion() { return sVersion; }

	private:
		static constexpr size_t kMaxEntries = 2000;
		static inline std::mutex sMutex;
		static inline std::vector<LogEntry> sEntries;
		static inline uint64_t sVersion = 0;
	};
}
