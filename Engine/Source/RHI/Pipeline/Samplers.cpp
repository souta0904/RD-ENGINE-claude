#include "RdPch.h"
#include "RHI/Pipeline/Samplers.h"

namespace rd::Samplers
{
	namespace
	{
		D3D12_STATIC_SAMPLER_DESC Make(uint32_t reg, D3D12_FILTER filter, D3D12_TEXTURE_ADDRESS_MODE address)
		{
			D3D12_STATIC_SAMPLER_DESC s = {};
			s.Filter = filter;
			s.AddressU = s.AddressV = s.AddressW = address;
			s.MaxAnisotropy = 8;
			s.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
			s.BorderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE;
			s.MaxLOD = D3D12_FLOAT32_MAX;
			s.ShaderRegister = reg;
			s.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;
			return s;
		}
	}

	D3D12_STATIC_SAMPLER_DESC LinearWrap(uint32_t reg) { return Make(reg, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_WRAP); }
	D3D12_STATIC_SAMPLER_DESC LinearClamp(uint32_t reg) { return Make(reg, D3D12_FILTER_MIN_MAG_MIP_LINEAR, D3D12_TEXTURE_ADDRESS_MODE_CLAMP); }
	D3D12_STATIC_SAMPLER_DESC PointClamp(uint32_t reg) { return Make(reg, D3D12_FILTER_MIN_MAG_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_CLAMP); }
	D3D12_STATIC_SAMPLER_DESC AnisotropicWrap(uint32_t reg) { return Make(reg, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP); }
	D3D12_STATIC_SAMPLER_DESC ShadowCompare(uint32_t reg)
	{
		auto s = Make(reg, D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, D3D12_TEXTURE_ADDRESS_MODE_BORDER);
		s.ComparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		return s;
	}
}
