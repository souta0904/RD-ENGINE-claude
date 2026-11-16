#include "RdPch.h"
#include "Core/Log/DebuggerSink.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	void DebuggerSink::Write(const LogRecord& r)
	{
		std::string line;
		if (r.level >= LogLevel::Warning)
		{
			line = std::format("{}({}): {}: [{}] {}\n", r.location.file_name(), r.location.line(),
				r.level == LogLevel::Warning ? "warning" : "error", r.category, r.message);
		}
		else
		{
			line = std::format("[{:<5}] [{}] {}\n", ToString(r.level), r.category, r.message);
		}
		OutputDebugStringW(ToWide(line).c_str());
	}
}
