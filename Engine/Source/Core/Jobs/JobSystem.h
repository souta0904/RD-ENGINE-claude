#pragma once
#include "Core/Jobs/JobHandle.h"
#include <functional>

namespace rd
{
	// High: フレームの中で完了を待つ処理 / Low: 待たないバックグラウンド処理（アセットの読み込みなど）
	enum class JobPriority : uint8_t
	{
		High,
		Low
	};

	// ワーカースレッドで動くジョブシステム
	//  - Schedule / ParallelFor でジョブを入れ、Wait で完了を待つ
	//  - Wait している間は呼んだスレッドも High のジョブを手伝うので、待ち合わせで止まらない
	//  - RunOnMainThread でワーカーからメインスレッドへ処理を戻せる（フレームの先頭で実行される）
	class JobSystem
	{
	public:
		using JobFunc = std::function<void()>;
		using RangeFunc = std::function<void(uint32_t begin, uint32_t end)>;

		static void Initialize(uint32_t workerCount = 0);
		static void Shutdown();

		// name はプロファイラに出る名前（文字列リテラル）
		static JobHandle Schedule(JobFunc func, JobPriority priority = JobPriority::High, const char* name = "Job");
		// [0, count) を batchSize ずつに分けて並列に実行する
		static JobHandle ParallelFor(uint32_t count, uint32_t batchSize, RangeFunc func,
			JobPriority priority = JobPriority::High, const char* name = "ParallelFor");
		static void Wait(const JobHandle& handle);

		static void RunOnMainThread(JobFunc func);
		static void ExecuteMainThreadTasks();

		static uint32_t GetWorkerCount();
		static bool IsMainThread();
		static uint32_t GetPendingCount();
	};
}
