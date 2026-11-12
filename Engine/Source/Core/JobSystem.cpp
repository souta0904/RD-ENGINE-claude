#include "RdPch.h"
#include "Core/JobSystem.h"
#include <condition_variable>
#include <deque>
#include <thread>

namespace rd
{
	namespace
	{
		struct Job
		{
			JobSystem::JobFunc func;
			std::shared_ptr<std::atomic<uint32_t>> counter;
		};

		struct State
		{
			std::vector<std::thread> workers;
			std::mutex mutex;
			std::condition_variable cv;
			std::deque<Job> highQueue;
			std::deque<Job> lowQueue;
			bool quit = false;

			std::mutex mainMutex;
			std::vector<JobSystem::JobFunc> mainTasks;

			std::thread::id mainThreadId;
		};

		State* gState = nullptr;

		void Execute(Job& job)
		{
			job.func();
			job.counter->fetch_sub(1, std::memory_order_acq_rel);
		}

		// キューから1つ取り出す（allowLow: Low優先度も対象にするか）
		bool TryPop(Job& out, bool allowLow)
		{
			std::lock_guard lock(gState->mutex);
			if (!gState->highQueue.empty())
			{
				out = std::move(gState->highQueue.front());
				gState->highQueue.pop_front();
				return true;
			}
			if (allowLow && !gState->lowQueue.empty())
			{
				out = std::move(gState->lowQueue.front());
				gState->lowQueue.pop_front();
				return true;
			}
			return false;
		}

		void WorkerMain(uint32_t index)
		{
			// WIC（テクスチャ読み込み）用に COM を初期化
			HRESULT hr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
			SetThreadDescription(GetCurrentThread(), std::format(L"RD Worker {}", index).c_str());

			while (true)
			{
				Job job;
				{
					std::unique_lock lock(gState->mutex);
					gState->cv.wait(lock, []
						{
							return gState->quit || !gState->highQueue.empty() || !gState->lowQueue.empty();
						});
					if (gState->quit && gState->highQueue.empty() && gState->lowQueue.empty())
					{
						break;
					}
					auto& queue = !gState->highQueue.empty() ? gState->highQueue : gState->lowQueue;
					job = std::move(queue.front());
					queue.pop_front();
				}
				Execute(job);
			}

			if (SUCCEEDED(hr)) { CoUninitialize(); }
		}

		void Push(Job job, JobPriority priority)
		{
			{
				std::lock_guard lock(gState->mutex);
				(priority == JobPriority::High ? gState->highQueue : gState->lowQueue).push_back(std::move(job));
			}
			gState->cv.notify_one();
		}
	}

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
		Log::Info("JobSystem: {} worker threads", workerCount);
	}

	void JobSystem::Shutdown()
	{
		if (!gState) { return; }
		{
			std::lock_guard lock(gState->mutex);
			gState->quit = true;
		}
		gState->cv.notify_all();
		for (auto& t : gState->workers) { t.join(); }
		// 残ったメインスレッドタスクも実行しておく（リソース解放など）
		ExecuteMainThreadTasks();
		delete gState;
		gState = nullptr;
	}

	JobHandle JobSystem::Schedule(JobFunc func, JobPriority priority)
	{
		RD_ASSERT(gState);
		JobHandle handle;
		handle.mCounter = std::make_shared<std::atomic<uint32_t>>(1);
		Push({ std::move(func), handle.mCounter }, priority);
		return handle;
	}

	JobHandle JobSystem::ParallelFor(uint32_t count, uint32_t batchSize, RangeFunc func, JobPriority priority)
	{
		RD_ASSERT(gState);
		JobHandle handle;
		if (count == 0) { return handle; }
		batchSize = std::max(1u, batchSize);
		uint32_t jobCount = (count + batchSize - 1) / batchSize;
		handle.mCounter = std::make_shared<std::atomic<uint32_t>>(jobCount);

		// 関数オブジェクトはジョブ間で共有する
		auto shared = std::make_shared<RangeFunc>(std::move(func));
		{
			std::lock_guard lock(gState->mutex);
			auto& queue = priority == JobPriority::High ? gState->highQueue : gState->lowQueue;
			for (uint32_t i = 0; i < jobCount; ++i)
			{
				uint32_t begin = i * batchSize;
				uint32_t end = std::min(count, begin + batchSize);
				queue.push_back({ [shared, begin, end] { (*shared)(begin, end); }, handle.mCounter });
			}
		}
		gState->cv.notify_all();
		return handle;
	}

	void JobSystem::Wait(const JobHandle& handle)
	{
		while (!handle.IsDone())
		{
			// 待っている間に High ジョブを手伝う
			Job job;
			if (TryPop(job, false))
			{
				Execute(job);
			}
			else
			{
				std::this_thread::yield();
			}
		}
	}

	void JobSystem::RunOnMainThread(JobFunc func)
	{
		RD_ASSERT(gState);
		std::lock_guard lock(gState->mainMutex);
		gState->mainTasks.push_back(std::move(func));
	}

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
}
