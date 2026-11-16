#pragma once
#include "RHI/D3D12Common.h"

namespace rd
{
	// フレームごとの一時アップロード領域から確保したメモリ（そのフレームの間だけ有効）
	struct DynamicAllocation
	{
		void* cpu = nullptr;
		D3D12_GPU_VIRTUAL_ADDRESS gpu = 0;
		ID3D12Resource* resource = nullptr;
		uint64_t offset = 0;
	};
}
