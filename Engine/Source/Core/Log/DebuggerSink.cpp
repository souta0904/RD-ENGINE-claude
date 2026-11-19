#include "RdPch.h"
#include "Core/Log/DebuggerSink.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	// Warning 以上は "D:\...\Scene.cpp(120): warning: [Scene] ..." の形にする
	// これはコンパイラのエラーと同じ形なので、VS の出力ウィンドウでダブルクリックするとその行が開き、
	// エラー一覧にも出る
	// OutputDebugStringW … デバッガ（VS）の出力ウィンドウへ文字列を送る Windows の API。
	// A 版は日本語が化けるので、UTF-16 にして W 版で送る
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
			// {:<5} … 5 文字の幅で左寄せ（レベルの名前の長さが違っても後ろの列がそろう）
			line = std::format("[{:<5}] [{}] {}\n", ToString(r.level), r.category, r.message);
		}
		OutputDebugStringW(ToWide(line).c_str());
	}
}
