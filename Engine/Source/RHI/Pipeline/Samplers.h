#pragma once
#include "RHI/D3D12Common.h"

// よく使う静的サンプラー
namespace rd::Samplers
{
	RD_API D3D12_STATIC_SAMPLER_DESC LinearWrap(uint32_t reg);
	RD_API D3D12_STATIC_SAMPLER_DESC LinearClamp(uint32_t reg);
	RD_API D3D12_STATIC_SAMPLER_DESC PointClamp(uint32_t reg);
	RD_API D3D12_STATIC_SAMPLER_DESC AnisotropicWrap(uint32_t reg);
	RD_API D3D12_STATIC_SAMPLER_DESC ShadowCompare(uint32_t reg);
}
