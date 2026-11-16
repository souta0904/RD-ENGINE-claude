#pragma once
#include "RHI/Descriptor/DescriptorHandle.h"
#include <mutex>
#include <vector>

namespace rd
{
	// 空き番号の一覧で管理するデスクリプタヒープ（スレッドセーフ）
	class DescriptorHeap
	{
	public:
		void Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible);

		DescriptorHandle Allocate();
		// すぐに返す（GPU が使っているかもしれないときは GraphicsDevice::DeferFreeDescriptor を使う）
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
