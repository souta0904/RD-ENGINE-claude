#include "RdPch.h"
#include "Core/Log.h"
#include "Core/StringUtil.h"
#include <chrono>
#include <fstream>

namespace rd
{
	namespace
	{
		const auto kStartTime = std::chrono::steady_clock::now();

		const char* LevelPrefix(LogLevel level)
		{
			switch (level)
			{
			case LogLevel::Warning: return "[Warn ] ";
			case LogLevel::Error: return "[Error] ";
			default: return "[Info ] ";
			}
		}
	}

	void Log::Write(LogLevel level, std::string message)
	{
		double time = std::chrono::duration<double>(std::chrono::steady_clock::now() - kStartTime).count();

		// 出力ウィンドウ
		std::string line = LevelPrefix(level) + message + "\n";
		OutputDebugStringW(ToWide(line).c_str());

		std::lock_guard lock(sMutex);
		// ファイルにも残す（起動ごとに作り直す）
		static std::ofstream file = []
			{
				std::filesystem::create_directories("Logs");
				return std::ofstream("Logs/RdEngine.log", std::ios::trunc);
			}();
		if (file)
		{
			file << std::format("{:9.3f} ", time) << line;
			file.flush();
		}
		if (sEntries.size() >= kMaxEntries)
		{
			sEntries.erase(sEntries.begin(), sEntries.begin() + kMaxEntries / 4);
		}
		sEntries.push_back({ level, time, std::move(message) });
		++sVersion;
	}

	void Log::Clear()
	{
		std::lock_guard lock(sMutex);
		sEntries.clear();
		++sVersion;
	}

	void ReportAssertFailure(const char* expr, const char* file, int line, const std::string& message)
	{
		Log::Error("Assertion failed: {} ({}:{}) {}", expr, file, line, message);
	}
}
