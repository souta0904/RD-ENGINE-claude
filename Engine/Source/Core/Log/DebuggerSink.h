#pragma once
#include "Core/Log/LogSink.h"

namespace rd
{
	// Visual Studio の出力ウィンドウへ書く出力先
	//  Warning 以上は「ファイル(行):」の形にするので、ダブルクリックでその行へ飛べる
	class DebuggerSink final : public LogSink
	{
	public:
		void Write(const LogRecord& record) override;
	};
}
