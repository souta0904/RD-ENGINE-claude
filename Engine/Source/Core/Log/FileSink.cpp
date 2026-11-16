#include "RdPch.h"
#include "Core/Log/FileSink.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	namespace
	{
		const char* FileName(const char* path)
		{
			const char* name = path;
			for (const char* p = path; *p; ++p)
			{
				if (*p == '\\' || *p == '/') { name = p + 1; }
			}
			return name;
		}

		std::string Timestamp(const SYSTEMTIME& t)
		{
			return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
		}

		// 前回のログを更新日時の付いた名前で History/ へ移し、古いものから消して数を絞る
		void MoveToHistory(const std::filesystem::path& path, uint32_t keep)
		{
			namespace fs = std::filesystem;
			std::error_code ec;
			fs::path history = path.parent_path() / L"History";
			fs::create_directories(history, ec);

			WIN32_FILE_ATTRIBUTE_DATA attr = {};
			SYSTEMTIME st = {};
			if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr))
			{
				FILETIME local;
				FileTimeToLocalFileTime(&attr.ftLastWriteTime, &local);
				FileTimeToSystemTime(&local, &st);
			}
			fs::rename(path, history / (path.stem().wstring() + L"_" + ToWide(Timestamp(st)) + path.extension().wstring()), ec);

			std::vector<fs::path> files;
			for (auto& e : fs::directory_iterator(history, ec))
			{
				if (e.is_regular_file() && e.path().extension() == path.extension()) { files.push_back(e.path()); }
			}
			// 名前に日時が入っているので、名前順がそのまま古い順になる
			std::sort(files.begin(), files.end());
			while (files.size() > keep)
			{
				fs::remove(files.front(), ec);
				files.erase(files.begin());
			}
		}
	}

	bool FileSink::Open(const LogSettings& settings, std::string& outPath)
	{
		namespace fs = std::filesystem;
		std::error_code ec;
		fs::path dir = ToWide(settings.directory);
		fs::create_directories(dir, ec);
		fs::path path = dir / ToWide(settings.fileName);
		if (fs::exists(path, ec)) { MoveToHistory(path, settings.keepHistory); }

		mFile.open(path, std::ios::trunc);
		outPath = ToUtf8(path.wstring());
		if (!mFile) { return false; }

		SYSTEMTIME now;
		GetLocalTime(&now);
		mFile << std::format("RD ENGINE log  {:04}-{:02}-{:02} {:02}:{:02}:{:02}  pid {}\n",
			now.wYear, now.wMonth, now.wDay, now.wHour, now.wMinute, now.wSecond, GetCurrentProcessId());
		mFile << "time         elapsed   level category thread | message  (source)\n";
		return true;
	}

	void FileSink::Write(const LogRecord& r)
	{
		if (!mFile) { return; }
		std::string thread = r.mainThread ? std::string("main") : std::to_string(r.threadId);
		mFile << std::format("{:02}:{:02}:{:02}.{:03} {:9.3f} {:<5} {:<8} {:>6} | {}  ({}:{})\n",
			r.hour, r.minute, r.second, r.millisecond, r.time, ToString(r.level), r.category, thread,
			r.message, FileName(r.location.file_name()), r.location.line());
	}

	void FileSink::Flush()
	{
		if (mFile) { mFile.flush(); }
	}
}
