#include "RdPch.h"
#include "Core/Log/FileSink.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	namespace
	{
		// フルパスからファイル名だけを取り出す（ログの行を短くするため）
		// source_location::file_name() は const char* なので、std::string を作らずにポインタを進めて探す
		const char* FileName(const char* path)
		{
			const char* name = path;
			for (const char* p = path; *p; ++p)
			{
				if (*p == '\\' || *p == '/') { name = p + 1; }
			}
			return name;
		}

		// {:04} … 4 桁で、足りなければ前を 0 で埋める（2026、09 のように）
		std::string Timestamp(const SYSTEMTIME& t)
		{
			return std::format("{:04}{:02}{:02}_{:02}{:02}{:02}", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
		}

		// 前回のログを更新日時の付いた名前で History/ へ移し、古いものから消して数を絞る
		//  例: Logs/RdEngine.log → Logs/History/RdEngine_20261119_153012.log
		//  付ける日時は「今」ではなく、前回のファイルを最後に書いた時刻（= 前回の終了時刻）にする
		//  FILETIME は UTC なので、日本時間にしてから年月日に分ける
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

	// std::ios::trunc … 開くときに中身を空にする（前回分は History へ移した後なので消してよい）
	// 先頭に起動した日時とプロセスの番号、列の見出しを書いておく
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

	// 1 件を 1 行で書く。列の幅をそろえて、テキストエディタで縦に読みやすくする
	//  {:9.3f} … 9 文字の幅で小数点以下 3 桁、{:<8} … 8 文字の幅で左寄せ、{:>6} … 6 文字の幅で右寄せ
	void FileSink::Write(const LogRecord& r)
	{
		if (!mFile) { return; }
		std::string thread = r.mainThread ? std::string("main") : std::to_string(r.threadId);
		mFile << std::format("{:02}:{:02}:{:02}.{:03} {:9.3f} {:<5} {:<8} {:>6} | {}  ({}:{})\n",
			r.hour, r.minute, r.second, r.millisecond, r.time, ToString(r.level), r.category, thread,
			r.message, FileName(r.location.file_name()), r.location.line());
	}

	// ofstream は書いた内容をメモリに溜めてから、まとめてファイルへ書く。
	// flush で溜めている分を今すぐ書き出す（落ちたときに最後のログが消えないように）
	void FileSink::Flush()
	{
		if (mFile) { mFile.flush(); }
	}
}
