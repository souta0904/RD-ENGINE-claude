#pragma once
#include "RHI/D3D12Common.h"

namespace rd
{
	// デスクリプタヒープの中の 1 つ（gpu はシェーダーから見えるヒープのときだけ有効）
	struct DescriptorHandle
	{
		D3D12_CPU_DESCRIPTOR_HANDLE cpu = {};
		D3D12_GPU_DESCRIPTOR_HANDLE gpu = {};
		uint32_t index = UINT32_MAX;

		bool IsValid() const { return index != UINT32_MAX; }
	};
}
