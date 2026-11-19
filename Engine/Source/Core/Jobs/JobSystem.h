#pragma once
#include "Core/Jobs/JobHandle.h"
#include <functional>

namespace rd
{
	// High: フレームの中で完了を待つ処理 / Low: 待たないバックグラウンド処理（アセットの読み込みなど）
	//  ワーカーは High を先に取るので、重い読み込みが溜まっていても、フレームの処理が後回しにならない
	enum class JobPriority : uint8_t
	{
		High,
		Low
	};

	// ワーカースレッドで動くジョブシステム
	//  - Schedule / ParallelFor でジョブを入れ、Wait で完了を待つ
	//  - Wait している間は呼んだスレッドも High のジョブを手伝うので、待ち合わせで止まらない
	//  - RunOnMainThread でワーカーからメインスレッドへ処理を戻せる（フレームの先頭で実行される）
	//
	//  ジョブ … 「後で、どれかのスレッドで実行してほしい関数」。CPU のコアの数だけワーカーを作っておき、
	//  ジョブをキューに入れると空いているワーカーが取り出して実行する。
	//  処理のたびにスレッドを作ると遅い（作るだけで数十マイクロ秒）ので、作ったスレッドを使い回す
	//
	//  例: 1000 個のパーティクルを 4 つのコアで分けて更新する
	//    auto h = JobSystem::ParallelFor(1000, 64, [&](uint32_t begin, uint32_t end)
	//    {
	//        for (uint32_t i = begin; i < end; ++i) { Update(particles[i]); }
	//    });
	//    JobSystem::Wait(h);
	class JobSystem
	{
	public:
		using JobFunc = std::function<void()>;
		using RangeFunc = std::function<void(uint32_t begin, uint32_t end)>;

		// workerCount が 0 なら「コアの数 - 1」（メインスレッドの分を空ける。最低 2）
		static void Initialize(uint32_t workerCount = 0);
		static void Shutdown();

		// name はプロファイラに出る名前（文字列リテラル）
		static JobHandle Schedule(JobFunc func, JobPriority priority = JobPriority::High, const char* name = "Job");
		// [0, count) を batchSize ずつに分けて並列に実行する
		// batchSize が小さすぎるとジョブの出し入れの手間が目立ち、大きすぎるとコアが余る
		static JobHandle ParallelFor(uint32_t count, uint32_t batchSize, RangeFunc func,
			JobPriority priority = JobPriority::High, const char* name = "ParallelFor");
		// 終わるまで待つ（待つ間は High のジョブを手伝う）
		static void Wait(const JobHandle& handle);

		// シーンの書き換えなど、メインスレッドでしかしてはいけない処理をワーカーから頼む
		// （今のエンジンではまだ使っていない。ワーカーで読んだ結果をメインスレッドで反映したいときに使う）
		static void RunOnMainThread(JobFunc func);
		// 頼まれた処理を実行する（エンジンが毎フレームの最初に呼ぶ）
		static void ExecuteMainThreadTasks();

		static uint32_t GetWorkerCount();
		static bool IsMainThread();
		// キューで待っているジョブの数
		static uint32_t GetPendingCount();
	};
}
