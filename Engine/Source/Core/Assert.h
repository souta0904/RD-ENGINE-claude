#pragma once
#include <format>
#include <string>

namespace rd
{
	// アサート失敗時の処理（ログ出力してブレーク）
	void ReportAssertFailure(const char* expr, const char* file, int line, const std::string& message);
}

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
#define RD_ASSERT(cond, ...) do { (void)sizeof(cond); } while (false)
#endif

// リリースでも評価される致命的チェック
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

namespace rd::detail
{
	inline std::string AssertMessage() { return {}; }
	template <class... Args>
	std::string AssertMessage(std::format_string<Args...> fmt, Args&&... args)
	{
		return std::format(fmt, std::forward<Args>(args)...);
	}
}
