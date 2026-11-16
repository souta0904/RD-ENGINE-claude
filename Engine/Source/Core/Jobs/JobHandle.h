#pragma once
#include <atomic>
#include <cstdint>
#include <memory>

namespace rd
{
	// ジョブの完了待ちに使うハンドル（中身は「残りのジョブ数」のカウンタを共有している）
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
}
