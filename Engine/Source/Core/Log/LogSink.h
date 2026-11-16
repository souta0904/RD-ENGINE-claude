#pragma once
#include "Core/Log/LogTypes.h"

namespace rd
{
	// ログの出力先の基底（Write はログ専用スレッドから呼ばれるので、出力先ごとのロックはいらない）
	class LogSink
	{
	public:
		virtual ~LogSink() = default;
		virtual void Write(const LogRecord& record) = 0;
		virtual void Flush() {}
	};
}
