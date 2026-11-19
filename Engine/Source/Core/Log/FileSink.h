#pragma once
#include "Core/Log/LogSink.h"
#include <fstream>

namespace rd
{
	// ファイルへ書く出力先（起動ごとに作り直し、前回分は History/ へ日時付きで移す）
	//  前回のログを残しておくと、「さっき落ちたときのログ」を後から見られる
	class FileSink final : public LogSink
	{
	public:
		// ファイルを開く（outPath に実際に開いたパスが入る）。開けなければ false
		bool Open(const LogSettings& settings, std::string& outPath);
		void Write(const LogRecord& record) override;
		void Flush() override;

	private:
		std::ofstream mFile;
	};
}
