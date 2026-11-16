#pragma once
#include "RHI/D3D12Common.h"
#include <memory>
#include <vector>

namespace rd
{
	// ルートシグネチャを組み立てる（AddSrv はルート SRV。構造化バッファ用）
	class RootSignatureBuilder
	{
	public:
		RootSignatureBuilder& AddCbv(uint32_t reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddSrv(uint32_t reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddSrvTable(uint32_t reg, uint32_t count = 1, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
		RootSignatureBuilder& AddConstants(uint32_t reg, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);
		ComPtr<ID3D12RootSignature> Build(ID3D12Device* device, const wchar_t* name = nullptr);

	private:
		std::vector<CD3DX12_ROOT_PARAMETER1> mParams;
		std::vector<std::unique_ptr<CD3DX12_DESCRIPTOR_RANGE1>> mRanges;
		std::vector<D3D12_STATIC_SAMPLER_DESC> mSamplers;
	};
}
