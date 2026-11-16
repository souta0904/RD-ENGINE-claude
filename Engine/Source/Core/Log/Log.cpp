#include "RdPch.h"
#include "Core/Log/Log.h"
#include "Core/Log/DebuggerSink.h"
#include "Core/Log/FileSink.h"
#include <chrono>
#include <condition_variable>
#include <deque>

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
		constexpr size_t kMaxHistory = 2000;

		// ロガーの状態（終了処理の順番に左右されないよう、作ったまま解放しない）
		//  queue: 書き出し待ち。pushed / written の差で Flush の待ち合わせをする
		//  history: エディタのコンソール用
		struct LogState
		{
			std::mutex queueMutex;
			std::condition_variable queueCv;
			std::condition_variable drainedCv;
			std::deque<LogRecord> queue;
			uint64_t pushed = 0;
			uint64_t written = 0;
			bool stop = false;
			std::thread worker;

			std::mutex sinkMutex;
			std::vector<std::shared_ptr<LogSink>> sinks;

			std::mutex historyMutex;
			std::deque<LogRecord> history;
			std::atomic<uint64_t> historyVersion = 0;

			std::atomic<LogLevel> minLevel = LogLevel::Debug;
			std::string filePath;
			const std::chrono::steady_clock::time_point startTime = std::chrono::steady_clock::now();
			const DWORD mainThreadId = GetCurrentThreadId();
		};

		LogState* CreateState()
		{
			LogState* state = new LogState();
			state->sinks.push_back(std::make_shared<DebuggerSink>());
			return state;
		}

		LogState& State()
		{
			static LogState* state = CreateState();
			return *state;
		}

		// ログ専用スレッド: キューをまとめて取り出し、ロックを外してから出力先へ書く
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

		// queueMutex を持った状態で呼ぶ
		void EnsureWorker(LogState& s)
		{
			if (!s.worker.joinable() && !s.stop)
			{
				s.worker = std::thread(WorkerLoop, std::ref(s));
				SetThreadDescription(s.worker.native_handle(), L"RD Log");
			}
		}
	}

	void Log::Initialize(const LogSettings& settings)
	{
		LogState& s = State();
		s.minLevel = settings.minLevel;
		// 初期化前のログを出し切ってから、履歴をファイルへまとめて書く（二重に書かないため）
		Flush();

		auto file = std::make_shared<FileSink>();
		std::string path;
		if (file->Open(settings, path))
		{
			s.filePath = path;
			std::lock_guard lock(s.historyMutex);
			for (const auto& r : s.history) { file->Write(r); }
		}
		AddSink(file);
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
