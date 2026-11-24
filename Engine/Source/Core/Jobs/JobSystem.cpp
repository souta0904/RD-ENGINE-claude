#include "RdPch.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Profiling/ProfileScope.h"
#include <condition_variable>
#include <deque>

namespace rd
{
	namespace
	{
		// キューに入る 1 つのジョブ（実行する関数と、終わったら 1 減らすカウンタ）
		struct Job
		{
			JobSystem::JobFunc func;
			std::shared_ptr<std::atomic<uint32_t>> counter;
			const char* name = "Job";
		};

		// ジョブシステムの状態
		//  mutex と cv … キューを守るロックと、ジョブが入ったことをワーカーへ知らせる仕組み
		//  キューは優先度ごとに 2 本。deque は先頭からも末尾からも速く出し入れできる（先に入れたものから取り出す）
		//  mainTasks … メインスレッドで実行してほしい処理（キューとは別のロックで守る）
		struct State
		{
			std::vector<std::thread> workers;
			std::mutex mutex;
			std::condition_variable cv;
			std::deque<Job> highQueue;
			std::deque<Job> lowQueue;
			bool quit = false;
			// キューから取り出して、まだ終わっていないジョブの数（取り出すときにロックの中で増やすので、
			//  「キューが空で running が 0」なら本当に何も動いていない）
			std::atomic<uint32_t> running = 0;

			std::mutex mainMutex;
			std::vector<JobSystem::JobFunc> mainTasks;

			std::thread::id mainThreadId;
		};

		// Initialize で作り、Shutdown で消す（その間だけ有効）
		State* gState = nullptr;

		// ジョブを 1 つ実行して、カウンタを 1 減らす
		// memory_order_acq_rel … このジョブが書いた結果が、カウンタの 0 を見たスレッドから必ず見えるようにする
		void Execute(Job& job)
		{
			{
				RD_PROFILE_SCOPE(job.name);
				job.func();
			}
			job.counter->fetch_sub(1, std::memory_order_acq_rel);
			gState->running.fetch_sub(1, std::memory_order_acq_rel);
		}

		// キューから 1 つ取り出す（なければ待たずに false）。High を先に見る
		// allowLow が false なら High だけ（Wait で手伝うときに、長い読み込みを掴んで待ちが延びないように）
		bool TryPop(Job& out, bool allowLow)
		{
			std::lock_guard lock(gState->mutex);
			if (!gState->highQueue.empty())
			{
				out = std::move(gState->highQueue.front());
				gState->highQueue.pop_front();
				gState->running.fetch_add(1, std::memory_order_acq_rel);
				return true;
			}
			if (allowLow && !gState->lowQueue.empty())
			{
				out = std::move(gState->lowQueue.front());
				gState->lowQueue.pop_front();
				gState->running.fetch_add(1, std::memory_order_acq_rel);
				return true;
			}
			return false;
		}

		// ワーカーが起きてよい条件（終了の合図か、ジョブがある）
		bool HasWork()
		{
			return gState->quit || !gState->highQueue.empty() || !gState->lowQueue.empty();
		}

		// ワーカースレッドの中身: ジョブが来るまで眠り、来たら取り出して実行する、を終了の合図まで繰り返す
		//  終了の合図が来ても、キューに残っているジョブは全部実行してから抜ける
		//  ジョブの実行はロックを外してから行う（実行中も他のワーカーがキューから取れるように）
		void WorkerMain(uint32_t index)
		{
			// テクスチャの読み込み（WIC）に COM が要る
			// COM … Windows の部品の仕組み。使うスレッドごとに CoInitializeEx で準備し、終わりに CoUninitialize で片付ける
			HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			SetThreadDescription(GetCurrentThread(), std::format(L"RD Worker {}", index).c_str());
			Profiler::SetThreadName(std::format("Worker {}", index).c_str());

			while (true)
			{
				Job job;
				{
					std::unique_lock lock(gState->mutex);
					gState->cv.wait(lock, HasWork);
					if (gState->quit && gState->highQueue.empty() && gState->lowQueue.empty()) { break; }
					auto& queue = !gState->highQueue.empty() ? gState->highQueue : gState->lowQueue;
					job = std::move(queue.front());
					queue.pop_front();
					gState->running.fetch_add(1, std::memory_order_acq_rel);
				}
				Execute(job);
			}

			if (SUCCEEDED(hr)) { CoUninitialize(); }
		}

		// キューへ入れて、眠っているワーカーを 1 つ起こす
		// notify はロックを外してから呼ぶ（起きたワーカーがすぐロックを取れるように）
		void Push(Job job, JobPriority priority)
		{
			{
				std::lock_guard lock(gState->mutex);
				(priority == JobPriority::High ? gState->highQueue : gState->lowQueue).push_back(std::move(job));
			}
			gState->cv.notify_one();
		}
	}

	// hardware_concurrency … CPU が同時に動かせるスレッドの数（8 コア 16 スレッドなら 16）
	void JobSystem::Initialize(uint32_t workerCount)
	{
		RD_ASSERT(!gState);
		gState = new State();
		gState->mainThreadId = std::this_thread::get_id();
		if (workerCount == 0)
		{
			uint32_t hw = std::thread::hardware_concurrency();
			workerCount = std::max(2u, hw > 1 ? hw - 1 : 1u);
		}
		for (uint32_t i = 0; i < workerCount; ++i)
		{
			gState->workers.emplace_back(WorkerMain, i);
		}
		Logger::Info(LogCat::Core, "JobSystem: {} worker threads", workerCount);
	}

	// quit を立てて全部のワーカーを起こし、残りのジョブを片付けて抜けるのを join で待つ
	void JobSystem::Shutdown()
	{
		if (!gState) { return; }
		{
			std::lock_guard lock(gState->mutex);
			gState->quit = true;
		}
		gState->cv.notify_all();
		for (auto& t : gState->workers) { t.join(); }
		// 残ったメインスレッドの処理（リソースの解放など）も実行しておく
		ExecuteMainThreadTasks();
		delete gState;
		gState = nullptr;
	}

	JobHandle JobSystem::Schedule(JobFunc func, JobPriority priority, const char* name)
	{
		RD_ASSERT(gState);
		JobHandle handle;
		handle.mCounter = std::make_shared<std::atomic<uint32_t>>(1);
		Push({ std::move(func), handle.mCounter, name }, priority);
		return handle;
	}

	JobHandle JobSystem::ParallelFor(uint32_t count, uint32_t batchSize, RangeFunc func, JobPriority priority, const char* name)
	{
		RD_ASSERT(gState);
		JobHandle handle;
		if (count == 0) { return handle; }
		batchSize = std::max(1u, batchSize);
		uint32_t jobCount = (count + batchSize - 1) / batchSize;
		handle.mCounter = std::make_shared<std::atomic<uint32_t>>(jobCount);

		// 関数はすべてのジョブで共有する（ジョブごとにコピーしない）
		// 例: count = 1000、batchSize = 64 なら、ジョブは 16 個（最後の 1 つは 960～999 の 40 個だけ）
		// (count + batchSize - 1) / batchSize … 割り算の切り上げ
		// 全部入れてから notify_all で全ワーカーを起こす（1 つずつ notify するより速い）
		auto shared = std::make_shared<RangeFunc>(std::move(func));
		{
			std::lock_guard lock(gState->mutex);
			auto& queue = priority == JobPriority::High ? gState->highQueue : gState->lowQueue;
			for (uint32_t i = 0; i < jobCount; ++i)
			{
				uint32_t begin = i * batchSize;
				uint32_t end = std::min(count, begin + batchSize);
				queue.push_back({ [shared, begin, end] { (*shared)(begin, end); }, handle.mCounter, name });
			}
		}
		gState->cv.notify_all();
		return handle;
	}

	// ただ眠って待つと、そのスレッドの分だけ CPU が遊ぶ。待つ間も High のジョブを取って手伝う。
	// ワーカーの中から Wait しても、自分で残りを実行できるので、全員が待ち合って止まる（デッドロック）ことがない
	// 手伝うジョブがなければ yield（他のスレッドに CPU を譲る）してから、もう一度確かめる
	void JobSystem::Wait(const JobHandle& handle)
	{
		while (!handle.IsDone())
		{
			Job job;
			if (TryPop(job, false)) { Execute(job); }
			else { std::this_thread::yield(); }
		}
	}

	void JobSystem::RunOnMainThread(JobFunc func)
	{
		RD_ASSERT(gState);
		std::lock_guard lock(gState->mainMutex);
		gState->mainTasks.push_back(std::move(func));
	}

	// swap で一覧ごと取り出してから、ロックを外して実行する
	// （実行中の処理がさらに RunOnMainThread を呼んでも固まらない。その分は次のフレームで実行される）
	void JobSystem::ExecuteMainThreadTasks()
	{
		if (!gState) { return; }
		std::vector<JobFunc> tasks;
		{
			std::lock_guard lock(gState->mainMutex);
			tasks.swap(gState->mainTasks);
		}
		for (auto& task : tasks) { task(); }
	}

	uint32_t JobSystem::GetWorkerCount()
	{
		return gState ? uint32_t(gState->workers.size()) : 0;
	}

	bool JobSystem::IsMainThread()
	{
		return gState && std::this_thread::get_id() == gState->mainThreadId;
	}

	uint32_t JobSystem::GetPendingCount()
	{
		if (!gState) { return 0; }
		std::lock_guard lock(gState->mutex);
		return uint32_t(gState->highQueue.size() + gState->lowQueue.size());
	}

	// キューのジョブを手伝いながら、全部が終わるまで待つ（ゲームのコードを読み直す前に使う）
	void JobSystem::WaitIdle()
	{
		if (!gState) { return; }
		while (true)
		{
			Job job;
			if (TryPop(job, true)) { Execute(job); continue; }
			if (gState->running.load(std::memory_order_acquire) == 0) { break; }
			std::this_thread::yield();
		}
	}
}
