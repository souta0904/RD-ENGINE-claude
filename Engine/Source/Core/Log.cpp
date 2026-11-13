#include "RdPch.h"
#include "Core/Log.h"
#include "Core/StringUtil.h"
#include <DbgHelp.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <deque>
#include <fstream>
#include <thread>

#pragma comment(lib, "dbghelp.lib")

namespace rd
{
	const char* ToString(LogLevel level)
	{
		switch (level)
		{
		case LogLevel::Trace: return "Trace";
		case LogLevel::Debug: return "Debug";
		case LogLevel::Info: return "Info";
		case LogLevel::Warning: return "Warn";
		case LogLevel::Error: return "Error";
		case LogLevel::Fatal: return "Fatal";
		}
		return "?";
	}

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

		// --------------------------------------------------
		// 出力先: Visual Studio の出力ウィンドウ
		//  Warning 以上は「ファイル(行):」形式にするので、ダブルクリックで該当行へ飛べる
		// --------------------------------------------------
		class DebuggerSink final : public LogSink
		{
		public:
			void Write(const LogRecord& r) override
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
		};

		// --------------------------------------------------
		// 出力先: ファイル
		//  起動ごとに作り直し、前回分は History/ へ日時付きで移して数を絞る
		// --------------------------------------------------
		class FileSink final : public LogSink
		{
		public:
			bool Open(const LogSettings& settings, std::string& outPath)
			{
				namespace fs = std::filesystem;
				std::error_code ec;
				fs::path dir = ToWide(settings.directory);
				fs::create_directories(dir, ec);
				fs::path path = dir / ToWide(settings.fileName);

				// 前回のログを History/ へ
				if (fs::exists(path, ec))
				{
					fs::path history = dir / L"History";
					fs::create_directories(history, ec);
					WIN32_FILE_ATTRIBUTE_DATA attr = {};
					SYSTEMTIME st = {};
					if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attr))
					{
						FILETIME local;
						FileTimeToLocalFileTime(&attr.ftLastWriteTime, &local);
						FileTimeToSystemTime(&local, &st);
					}
					fs::path stem = path.stem();
					fs::rename(path, history / (stem.wstring() + L"_" + ToWide(Timestamp(st)) + path.extension().wstring()), ec);

					// 古いものから消す
					std::vector<fs::path> files;
					for (auto& e : fs::directory_iterator(history, ec))
					{
						if (e.is_regular_file() && e.path().extension() == path.extension()) { files.push_back(e.path()); }
					}
					std::sort(files.begin(), files.end());// 名前に日時が入っているので名前順 = 古い順
					while (files.size() > settings.keepHistory)
					{
						fs::remove(files.front(), ec);
						files.erase(files.begin());
					}
				}

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

			void Write(const LogRecord& r) override
			{
				if (!mFile) { return; }
				std::string thread = r.mainThread ? std::string("main") : std::to_string(r.threadId);
				mFile << std::format("{:02}:{:02}:{:02}.{:03} {:9.3f} {:<5} {:<8} {:>6} | {}  ({}:{})\n",
					r.hour, r.minute, r.second, r.millisecond, r.time, ToString(r.level), r.category, thread,
					r.message, FileName(r.location.file_name()), r.location.line());
			}

			void Flush() override
			{
				if (mFile) { mFile.flush(); }
			}

		private:
			std::ofstream mFile;
		};

		// --------------------------------------------------
		// ロガーの状態（終了処理の順番に左右されないよう、作ったまま解放しない）
		// --------------------------------------------------
		struct LogState
		{
			std::mutex queueMutex;
			std::condition_variable queueCv;// 新しいログが来た / 止める
			std::condition_variable drainedCv;// 書き出しが追いついた
			std::deque<LogRecord> queue;
			uint64_t pushed = 0;// キューに入れた数
			uint64_t written = 0;// 出力先へ書いた数
			bool stop = false;
			std::thread worker;

			std::mutex sinkMutex;
			std::vector<std::shared_ptr<LogSink>> sinks;

			// エディタのコンソール用の履歴
			std::mutex historyMutex;
			std::deque<LogRecord> history;
			std::atomic<uint64_t> historyVersion = 0;

			std::atomic<LogLevel> minLevel = LogLevel::Debug;
			std::string filePath;
			std::string directory = "Logs";
			const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
			const DWORD mainThreadId = GetCurrentThreadId();
			bool crashHandlerInstalled = false;
		};

		constexpr size_t kMaxHistory = 2000;

		LogState& State()
		{
			static LogState* state = []
				{
					auto s = new LogState();
					s->sinks.push_back(std::make_shared<DebuggerSink>());
					return s;
				}();
			return *state;
		}

		void WorkerLoop(LogState& s)
		{
			std::vector<LogRecord> batch;
			while (true)
			{
				{
					std::unique_lock lock(s.queueMutex);
					s.queueCv.wait(lock, [&] { return s.stop || !s.queue.empty(); });
					if (s.queue.empty() && s.stop) { return; }
					batch.assign(std::make_move_iterator(s.queue.begin()), std::make_move_iterator(s.queue.end()));
					s.queue.clear();
				}
				{
					std::lock_guard lock(s.sinkMutex);
					for (const auto& r : batch)
					{
						for (auto& sink : s.sinks) { sink->Write(r); }
					}
					for (auto& sink : s.sinks) { sink->Flush(); }
				}
				{
					std::lock_guard lock(s.queueMutex);
					s.written += batch.size();
				}
				s.drainedCv.notify_all();
				batch.clear();
			}
		}

		void EnsureWorker(LogState& s)
		{
			// queueMutex を持った状態で呼ぶ
			if (!s.worker.joinable() && !s.stop)
			{
				s.worker = std::thread(WorkerLoop, std::ref(s));
				SetThreadDescription(s.worker.native_handle(), L"RD Log");
			}
		}

		// 未処理例外: ログを書き出してミニダンプを残す
		LONG WINAPI OnUnhandledException(EXCEPTION_POINTERS* info)
		{
			static std::atomic<bool> entered = false;
			if (entered.exchange(true)) { return EXCEPTION_CONTINUE_SEARCH; }

			LogState& s = State();
			SYSTEMTIME now;
			GetLocalTime(&now);
			std::filesystem::path dump = std::filesystem::path(ToWide(s.directory)) / ToWide("Crash_" + Timestamp(now) + ".dmp");
			HANDLE file = CreateFileW(dump.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
			bool dumped = false;
			if (file != INVALID_HANDLE_VALUE)
			{
				MINIDUMP_EXCEPTION_INFORMATION mei = { GetCurrentThreadId(), info, FALSE };
				dumped = MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file,
					MINIDUMP_TYPE(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo), &mei, nullptr, nullptr);
				CloseHandle(file);
			}
			Log::Write(LogLevel::Fatal, LogCat::Core, std::source_location::current(),
				std::format("Unhandled exception 0x{:08X} at {}{}", info->ExceptionRecord->ExceptionCode,
					info->ExceptionRecord->ExceptionAddress, dumped ? " (dump: " + ToUtf8(dump.wstring()) + ")" : ""));
			return EXCEPTION_CONTINUE_SEARCH;
		}

		void OnAbortSignal(int)
		{
			Log::Write(LogLevel::Fatal, LogCat::Core, std::source_location::current(), "abort() was called");
		}

		void OnTerminate()
		{
			std::string what = "std::terminate was called";
			if (auto e = std::current_exception())
			{
				try { std::rethrow_exception(e); }
				catch (const std::exception& ex) { what += std::format(" (uncaught exception: {})", ex.what()); }
				catch (...) { what += " (uncaught unknown exception)"; }
			}
			Log::Write(LogLevel::Fatal, LogCat::Core, std::source_location::current(), what);
			std::abort();
		}
	}

	// ==================================================
	// Log
	// ==================================================
	void Log::Initialize(const LogSettings& settings)
	{
		LogState& s = State();
		s.minLevel = settings.minLevel;
		s.directory = settings.directory;
		// 初期化前のログを出し切ってから、ファイルへまとめて書く（二重に書かないため）
		Flush();

		auto file = std::make_shared<FileSink>();
		std::string path;
		if (file->Open(settings, path))
		{
			s.filePath = path;
			// 初期化より前に出たログも残す
			std::lock_guard lock(s.historyMutex);
			for (const auto& r : s.history) { file->Write(r); }
		}
		AddSink(file);

		if (settings.crashHandler && !s.crashHandlerInstalled)
		{
			s.crashHandlerInstalled = true;
			SetUnhandledExceptionFilter(OnUnhandledException);
			std::set_terminate(OnTerminate);
			std::signal(SIGABRT, OnAbortSignal);
		}
		std::atexit([] { Log::Shutdown(); });
	}

	void Log::Shutdown()
	{
		LogState& s = State();
		{
			std::lock_guard lock(s.queueMutex);
			if (s.stop) { return; }
			s.stop = true;
		}
		s.queueCv.notify_all();
		if (s.worker.joinable()) { s.worker.join(); }
		std::lock_guard lock(s.sinkMutex);
		for (auto& sink : s.sinks) { sink->Flush(); }
	}

	void Log::Flush()
	{
		LogState& s = State();
		std::unique_lock lock(s.queueMutex);
		if (!s.worker.joinable() || s.stop) { return; }
		uint64_t target = s.pushed;
		// ログスレッドが止まっていても固まらないよう上限を付ける
		s.drainedCv.wait_for(lock, std::chrono::seconds(2), [&] { return s.written >= target; });
	}

	void Log::SetLevel(LogLevel level) { State().minLevel = level; }
	LogLevel Log::GetLevel() { return State().minLevel; }
	bool Log::IsEnabled(LogLevel level) { return level >= kLogCompileLevel && level >= State().minLevel.load(std::memory_order_relaxed); }

	void Log::AddSink(std::shared_ptr<LogSink> sink)
	{
		LogState& s = State();
		std::lock_guard lock(s.sinkMutex);
		s.sinks.push_back(std::move(sink));
	}

	void Log::RemoveSink(const std::shared_ptr<LogSink>& sink)
	{
		LogState& s = State();
		std::lock_guard lock(s.sinkMutex);
		s.sinks.erase(std::remove(s.sinks.begin(), s.sinks.end(), sink), s.sinks.end());
	}

	const std::string& Log::GetFilePath() { return State().filePath; }

	void Log::Write(LogLevel level, LogCategory category, const std::source_location& location, std::string message)
	{
		LogState& s = State();
		LogRecord r;
		r.level = level;
		r.category = category.name;
		r.time = std::chrono::duration<double>(std::chrono::steady_clock::now() - s.startTime).count();
		SYSTEMTIME now;
		GetLocalTime(&now);
		r.hour = now.wHour;
		r.minute = now.wMinute;
		r.second = now.wSecond;
		r.millisecond = now.wMilliseconds;
		r.threadId = GetCurrentThreadId();
		r.mainThread = r.threadId == s.mainThreadId;
		r.location = location;
		r.message = std::move(message);

		// エディタのコンソールはすぐに見えるよう、ここで履歴へ
		{
			std::lock_guard lock(s.historyMutex);
			if (s.history.size() >= kMaxHistory) { s.history.pop_front(); }
			s.history.push_back(r);
			++s.historyVersion;
		}

		bool stopped;
		{
			std::lock_guard lock(s.queueMutex);
			stopped = s.stop;
			if (!stopped)
			{
				EnsureWorker(s);
				s.queue.push_back(r);
				++s.pushed;
			}
		}
		if (stopped)
		{
			// 終了処理の後に出たログは、この場で直接書く
			std::lock_guard lock(s.sinkMutex);
			for (auto& sink : s.sinks) { sink->Write(r); sink->Flush(); }
			return;
		}
		s.queueCv.notify_one();

		// エラー以上はクラッシュに備えて書き出しを待つ
		if (level >= LogLevel::Error) { Flush(); }
	}

	void Log::Abort()
	{
		Flush();
		if (IsDebuggerPresent()) { __debugbreak(); }
		std::abort();
	}

	void Log::ForEach(const std::function<void(const LogRecord&)>& func)
	{
		LogState& s = State();
		std::lock_guard lock(s.historyMutex);
		for (const auto& r : s.history) { func(r); }
	}

	void Log::Clear()
	{
		LogState& s = State();
		std::lock_guard lock(s.historyMutex);
		s.history.clear();
		++s.historyVersion;
	}

	uint64_t Log::GetVersion() { return State().historyVersion; }

	void ReportAssertFailure(const char* expr, const char* file, int line, const std::string& message)
	{
		Log::Write(LogLevel::Error, LogCat::Core, std::source_location::current(),
			std::format("Assertion failed: {} ({}:{}) {}", expr, file, line, message));
	}
}
