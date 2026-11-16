#pragma once
#include <cstdint>
#include <source_location>
#include <string>

namespace rd
{
	enum class LogLevel : uint8_t
	{
		Trace,
		Debug,
		Info,
		Warning,
		Error,
		Fatal,
	};
	const char* ToString(LogLevel level);

	// コンパイル時に消すレベル（Game ビルドでは Debug 以下を消す）
#ifndef RD_LOG_COMPILE_LEVEL
#if !RD_EDITOR && !RD_DEBUG
#define RD_LOG_COMPILE_LEVEL 2
#else
#define RD_LOG_COMPILE_LEVEL 0
#endif
#endif
	inline constexpr LogLevel kLogCompileLevel = LogLevel(RD_LOG_COMPILE_LEVEL);

	// ログを出した機能の名前
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
		double time = 0.0;
		uint16_t hour = 0, minute = 0, second = 0, millisecond = 0;
		uint32_t threadId = 0;
		bool mainThread = false;
		std::source_location location;
		std::string message;
	};

	// ログの設定（前回のログは directory/History/ へ日時付きで移し、keepHistory 個だけ残す）
	struct LogSettings
	{
		std::string directory = "Logs";
		std::string fileName = "RdEngine.log";
		uint32_t keepHistory = 10;
		LogLevel minLevel = LogLevel::Debug;
	};
}
