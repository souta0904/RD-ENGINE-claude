#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>

namespace rd
{
	// ジョブの完了待ちに使うハンドル
	// 内部的には「残りジョブ数」のカウンタを共有している
	class JobHandle
	{
	public:
		JobHandle() = default;
		bool IsValid() const { return mCounter != nullptr; }
		bool IsDone() const { return !mCounter || mCounter->load(std::memory_order_acquire) == 0; }

	private:
		friend class JobSystem;
		std::shared_ptr<std::atomic<uint32_t>> mCounter;
	};

	enum class JobPriority : uint8_t
	{
		High,// フレーム内で完了を待つ処理（パーティクル更新など）
		Low// 完了を待たないバックグラウンド処理（アセット読み込みなど）
	};

	// ワーカースレッドによるジョブシステム
	//  - Schedule / ParallelFor でジョブを投入し、Wait で完了を待つ
	//  - Wait 中の呼び出しスレッドも High ジョブを手伝うのでデッドロックしない
	//  - RunOnMainThread でワーカーからメインスレッドへ処理を戻せる
	class JobSystem
	{
	public:
		using JobFunc = std::function<void()>;
		using RangeFunc = std::function<void(uint32_t begin, uint32_t end)>;

		static void Initialize(uint32_t workerCount = 0);
		static void Shutdown();

		static JobHandle Schedule(JobFunc func, JobPriority priority = JobPriority::High);
		// [0, count) を batchSize ごとに分割して並列実行する
		static JobHandle ParallelFor(uint32_t count, uint32_t batchSize, RangeFunc func,
			JobPriority priority = JobPriority::High);
		static void Wait(const JobHandle& handle);

		// メインスレッドで実行する処理を登録（フレーム先頭で実行される）
		static void RunOnMainThread(JobFunc func);
		static void ExecuteMainThreadTasks();

		static uint32_t GetWorkerCount();
		static bool IsMainThread();
		// 実行待ちのジョブ数（統計用）
		static uint32_t GetPendingCount();
	};
}
