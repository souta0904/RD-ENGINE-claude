#pragma once
#include "Core/Log/LogSink.h"
#include <fstream>

namespace rd
{
	// ファイルへ書く出力先（起動ごとに作り直し、前回分は History/ へ日時付きで移す）
	class FileSink final : public LogSink
	{
	public:
		bool Open(const LogSettings& settings, std::string& outPath);
		void Write(const LogRecord& record) override;
		void Flush() override;

	private:
		std::ofstream mFile;
	};
}
