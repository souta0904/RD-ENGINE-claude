#include "RdPch.h"
#include "RHI/DescriptorHeap.h"

namespace rd
{
	void DescriptorHeap::Create(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE type, uint32_t capacity, bool shaderVisible)
	{
		D3D12_DESCRIPTOR_HEAP_DESC desc = {};
		desc.Type = type;
		desc.NumDescriptors = capacity;
		desc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
		RD_CHECK_HR(device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&mHeap)));

		mCapacity = capacity;
		mShaderVisible = shaderVisible;
		mIncrement = device->GetDescriptorHandleIncrementSize(type);
		mCpuStart = mHeap->GetCPUDescriptorHandleForHeapStart();
		if (shaderVisible) { mGpuStart = mHeap->GetGPUDescriptorHandleForHeapStart(); }

		// 小さい番号から使われるように逆順に積む
		mFreeList.resize(capacity);
		for (uint32_t i = 0; i < capacity; ++i) { mFreeList[i] = capacity - 1 - i; }
	}

	DescriptorHandle DescriptorHeap::Allocate()
	{
		std::lock_guard lock(mMutex);
		RD_VERIFY(!mFreeList.empty(), "Descriptor heap is full (capacity {})", mCapacity);
		uint32_t index = mFreeList.back();
		mFreeList.pop_back();
		return GetHandle(index);
	}

	void DescriptorHeap::Free(DescriptorHandle& handle)
	{
		if (!handle.IsValid()) { return; }
		std::lock_guard lock(mMutex);
		mFreeList.push_back(handle.index);
		handle = {};
	}

	DescriptorHandle DescriptorHeap::GetHandle(uint32_t index) const
	{
		DescriptorHandle h;
		h.index = index;
		h.cpu.ptr = mCpuStart.ptr + SIZE_T(index) * mIncrement;
		if (mShaderVisible) { h.gpu.ptr = mGpuStart.ptr + UINT64(index) * mIncrement; }
		return h;
	}
}
