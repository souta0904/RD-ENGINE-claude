#include "RdPch.h"
#include "Core/Log/Logger.h"
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

	// 全体の流れ
	//  1. どのスレッドからでも Write が呼ばれる → 履歴（history）とキュー（queue）に 1 件足す
	//  2. ログ専用スレッド（WorkerLoop）が起きて、キューの中身をまとめて取り出す
	//  3. 取り出した分を各出力先（Sink）へ書く
	//
	// ロックを 3 つに分けている理由
	//  queueMutex   … キューの出し入れ。Write が毎回取るので、短い時間しか持たない
	//  sinkMutex    … 出力先の一覧と書き込み。ファイルへの書き込みは遅いので、キューのロックとは別にする
	//                 （書いている間も、他のスレッドはキューへ足せる）
	//  historyMutex … エディタのコンソールが履歴を読む間、キューへの追加を止めないように別にする
	namespace
	{
		// エディタのコンソールに残す件数（これを超えたら古いものから捨てる）
		constexpr size_t kMaxHistory = 2000;

		// ロガーの状態（終了処理の順番に左右されないよう、作ったまま解放しない）
		//  queue: 書き出し待ち。pushed（足した数）/ written（書いた数）を比べて Flush の待ち合わせをする
		//  queueCv: キューに足されたことをログ専用スレッドへ知らせる
		//  drainedCv: 書き終えたことを Flush で待っている側へ知らせる
		//  history: エディタのコンソール用
		//  condition_variable … 「条件が満たされるまで眠って待つ」ための仕組み。
		//  ループで何度も確かめる（CPU を使い続ける）のと違い、知らせが来るまで CPU を使わない
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

		// VS の出力ウィンドウへの出力先だけは最初から入れておく（Initialize より前のログも見えるように）
		LogState* CreateState()
		{
			LogState* state = new LogState();
			state->sinks.push_back(std::make_shared<DebuggerSink>());
			return state;
		}

		// 最初に呼ばれたときに作る（関数の中の static 変数は、最初に通ったときに 1 回だけ初期化される。
		// 複数のスレッドから同時に呼ばれても 1 回だけになることが C++ で決まっている）
		// 他の static 変数のコンストラクタからログを出しても、その時点で作られるので順番の問題が起きない
		LogState& State()
		{
			static LogState* state = CreateState();
			return *state;
		}

		// ログ専用スレッド: キューをまとめて取り出し、ロックを外してから出力先へ書く
		//  1 件ずつ取り出すとロックの回数が増えるので、溜まっている分を batch へまとめて移す。
		//  wait の第 2 引数は「起きてよい条件」。偶然起きてしまう（spurious wakeup）ことがあるので、条件を必ず書く
		//  make_move_iterator … コピーではなく移動で batch へ移す（メッセージの文字列をコピーしない）
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

		// ログ専用スレッドがまだなければ作る（最初のログが出たときに作られる）
		//  queueMutex を持った状態で呼ぶ（2 つのスレッドが同時に作らないように）
		//  SetThreadDescription … スレッドに名前を付ける。VS のスレッドの一覧やプロファイラで見分けやすくなる
		void EnsureWorker(LogState& s)
		{
			if (!s.worker.joinable() && !s.stop)
			{
				s.worker = std::thread(WorkerLoop, std::ref(s));
				SetThreadDescription(s.worker.native_handle(), L"RD Log");
			}
		}
	}

	void Logger::Initialize(const LogSettings& settings)
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
		// exit や main の終わりでも Shutdown が呼ばれるようにする（残ったログを書き切ってからスレッドを止める）
		std::atexit([] { Logger::Shutdown(); });
	}

	// stop を立ててログ専用スレッドを起こし、キューが空になって終わるのを join で待つ
	// 2 回呼ばれても大丈夫なように、すでに stop なら何もしない
	void Logger::Shutdown()
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

	// 呼んだ時点までに足されたログ（target 件目まで）が書き終わるのを待つ
	// 待っている間に他のスレッドが足したログまでは待たない（いつまでも終わらなくなるため）
	void Logger::Flush()
	{
		LogState& s = State();
		std::unique_lock lock(s.queueMutex);
		if (!s.worker.joinable() || s.stop) { return; }
		uint64_t target = s.pushed;
		// ログスレッドが止まっていても固まらないよう上限を付ける
		s.drainedCv.wait_for(lock, std::chrono::seconds(2), [&] { return s.written >= target; });
	}

	void Logger::SetLevel(LogLevel level) { State().minLevel = level; }
	LogLevel Logger::GetLevel() { return State().minLevel; }
	// memory_order_relaxed … 他の値との順番を気にしない一番軽い読み方。
	// レベルが変わった直後の数件が古いレベルで判定されても困らないので、これで十分
	bool Logger::IsEnabled(LogLevel level) { return level >= kLogCompileLevel && level >= State().minLevel.load(std::memory_order_relaxed); }

	void Logger::AddSink(std::shared_ptr<LogSink> sink)
	{
		LogState& s = State();
		std::lock_guard lock(s.sinkMutex);
		s.sinks.push_back(std::move(sink));
	}

	// remove で消す要素を後ろへ寄せ、erase でまとめて消す（vector から値で消すときの決まった書き方）
	void Logger::RemoveSink(const std::shared_ptr<LogSink>& sink)
	{
		LogState& s = State();
		std::lock_guard lock(s.sinkMutex);
		s.sinks.erase(std::remove(s.sinks.begin(), s.sinks.end(), sink), s.sinks.end());
	}

	const std::string& Logger::GetFilePath() { return State().filePath; }

	// 1 件のログを作り、履歴とキューへ入れる
	//  時刻はここ（出したスレッド）で取る。ログ専用スレッドで取ると、書くのが遅れた分だけずれるため
	//  message は値で受け取って move する（呼んだ側で作った文字列を、コピーせずに LogRecord へ移せる）
	void Logger::Write(LogLevel level, LogCategory category, const std::source_location& location, std::string message)
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

	// Fatal の後: ログを書き切ってから終了する（abort はクラッシュハンドラが受け取る）
	void Logger::Abort()
	{
		Flush();
		if (IsDebuggerPresent()) { __debugbreak(); }
		std::abort();
	}

	void Logger::ForEach(const std::function<void(const LogRecord&)>& func)
	{
		LogState& s = State();
		std::lock_guard lock(s.historyMutex);
		for (const auto& r : s.history) { func(r); }
	}

	void Logger::Clear()
	{
		LogState& s = State();
		std::lock_guard lock(s.historyMutex);
		s.history.clear();
		++s.historyVersion;
	}

	uint64_t Logger::GetVersion() { return State().historyVersion; }

	// Assert.h で宣言だけしている関数の中身（Assert.h から Logger.h を読み込まずに済むよう、ここに置く）
	// 場所は RD_ASSERT を書いた file と line をメッセージに入れる（location はこの関数の場所になってしまうため）
	void ReportAssertFailure(const char* expr, const char* file, int line, const std::string& message)
	{
		Logger::Write(LogLevel::Error, LogCat::Core, std::source_location::current(),
			std::format("Assertion failed: {} ({}:{}) {}", expr, file, line, message));
	}
}
