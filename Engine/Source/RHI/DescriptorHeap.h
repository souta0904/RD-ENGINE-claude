#pragma once
#include "RHI/D3D12Common.h"
#include <mutex>
#include <vector>

namespace rd
{
	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
		uint32_t index = UINT32_MAX;

		bool IsValid() const { return index != UINT32_MAX; }
	};

	// フリーリスト方式のデスクリプタヒープ（スレッドセーフ）
	class DescriptorHeap
	{
	public:
		void Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);

		DescriptorHandle Allocate();
		// 即時解放（GPU が使用中の可能性があるなら GraphicsDevice::DeferFree を使う）
		void Free(DescriptorHandle& handle);

		DescriptorHandle GetHandle(uint32_t index) const;
		void Destroy() { mHeap.Reset(); mFreeList.clear(); }
		ID3D12DescriptorHeap* Get() const { return mHeap.Get(); }
		uint32_t GetUsedCount() const { return mCapacity - uint32_t(mFreeList.size()); }
		uint32_t GetCapacity() const { return mCapacity; }

	private:
		ComPtr<ID3D12DescriptorHeap> mHeap;
		D3D12_CPU_DESCRIPTOR_HANDLE mCpuStart = {};
		D3D12_GPU_DESCRIPTOR_HANDLE mGpuStart = {};
		uint32_t mIncrement = 0;
		uint32_t mCapacity = 0;
		bool mShaderVisible = false;
		std::vector<uint32_t> mFreeList;
		std::mutex mMutex;
	};
}
