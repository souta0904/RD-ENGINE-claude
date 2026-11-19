#pragma once
#include "Core/Log/LogTypes.h"

namespace rd
{
	// ログの出力先の基底
	//  ログ 1 件を「どこへ書くか」を決めるのが Sink（流し込む先、の意味）。
	//  ファイル（FileSink）・VS の出力ウィンドウ（DebuggerSink）・エディタのコンソールなど、
	//  出力先ごとにこれを継承したクラスを作り、Log::AddSink で足す。
	//  Write はログ専用スレッドから 1 件ずつ順に呼ばれるので、出力先ごとのロックはいらない
	class LogSink
	{
	public:
		// 基底クラスのポインタ（shared_ptr<LogSink>）で消しても、派生クラスのデストラクタが呼ばれるように virtual にする
		virtual ~LogSink() = default;
		virtual void Write(const LogRecord& record) = 0;
		// まとめて書いたあとに呼ばれる。バッファに溜めている出力先は、ここで実際に書き出す
		virtual void Flush() {}
	};
}
