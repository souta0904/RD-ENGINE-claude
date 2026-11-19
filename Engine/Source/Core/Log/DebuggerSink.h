#pragma once
#include "Core/Log/LogSink.h"

namespace rd
{
	// Visual Studio の出力ウィンドウへ書く出力先
	//  Warning 以上は「ファイル(行):」の形にするので、ダブルクリックでその行へ飛べる
	//  Log の中で最初から登録されているので、Log::Initialize の前に出したログも見える
	//  final … これ以上継承させない印（コンパイラが仮想関数の呼び出しを速くできることもある）
	class DebuggerSink final : public LogSink
	{
	public:
		void Write(const LogRecord& record) override;
	};
}
