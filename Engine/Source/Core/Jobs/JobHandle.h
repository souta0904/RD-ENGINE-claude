#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

namespace rd
{
	// ジョブの完了待ちに使うハンドル（中身は「残りのジョブ数」のカウンタを共有している）
	//  Schedule なら 1、ParallelFor なら分けたジョブの数から始まり、ジョブが 1 つ終わるたびに 1 減る。
	//  0 になったら全部終わった。
	//  カウンタは atomic（複数のスレッドから同時に増減しても壊れない整数）で、
	//  shared_ptr なので、ハンドルを捨ててもジョブが終わるまではカウンタが残る
	class JobHandle
	{
	public:
		JobHandle() = default;
		// ジョブに結び付いているか（何も入れていない ParallelFor などでは false）
		bool IsValid() const { return mCounter != nullptr; }
		// 終わったか（結び付いていないハンドルは、終わったものとして扱う）
		// memory_order_acquire … 0 を読んだら、ジョブが書いた結果もすべて見えることを保証する読み方
		bool IsDone() const { return !mCounter || mCounter->load(std::memory_order_acquire) == 0; }

	private:
		// カウンタを作ったり渡したりするのは JobSystem だけ
		friend class JobSystem;
		std::shared_ptr<std::atomic<uint32_t>> mCounter;
	};
}
