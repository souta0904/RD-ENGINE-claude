#include "RdPch.h"
#include "RHI/Pipeline/RootSignatureBuilder.h"

namespace rd
{
	RootSignatureBuilder& RootSignatureBuilder::AddCbv(uint32_t reg, D3D12_SHADER_VISIBILITY vis)
	{
		CD3DX12_ROOT_PARAMETER1 p;
		p.InitAsConstantBufferView(reg, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, vis);
		mParams.push_back(p);
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddSrv(uint32_t reg, D3D12_SHADER_VISIBILITY vis)
	{
		CD3DX12_ROOT_PARAMETER1 p;
		p.InitAsShaderResourceView(reg, 0, D3D12_ROOT_DESCRIPTOR_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE, vis);
		mParams.push_back(p);
		return *this;
	}

	// 範囲は Build まで生きている必要があるので、ポインタが変わらないよう unique_ptr で持つ
	RootSignatureBuilder& RootSignatureBuilder::AddSrvTable(uint32_t reg, uint32_t count, D3D12_SHADER_VISIBILITY vis)
	{
		auto range = std::make_unique<CD3DX12_DESCRIPTOR_RANGE1>();
		range->Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, count, reg, 0, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_STATIC_WHILE_SET_AT_EXECUTE);
		CD3DX12_ROOT_PARAMETER1 p;
		p.InitAsDescriptorTable(1, range.get(), vis);
		mRanges.push_back(std::move(range));
		mParams.push_back(p);
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddConstants(uint32_t reg, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY vis)
	{
		CD3DX12_ROOT_PARAMETER1 p;
		p.InitAsConstants(num32BitValues, reg, 0, vis);
		mParams.push_back(p);
		return *this;
	}

	RootSignatureBuilder& RootSignatureBuilder::AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
	{
		mSamplers.push_back(sampler);
		return *this;
	}

	ComPtr<ID3D12RootSignature> RootSignatureBuilder::Build(ID3D12Device* device, const wchar_t* name)
	{
		CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC desc;
		desc.Init_1_1(UINT(mParams.size()), mParams.data(), UINT(mSamplers.size()), mSamplers.data(),
			D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

		ComPtr<ID3DBlob> blob, error;
		HRESULT hr = D3D12SerializeVersionedRootSignature(&desc, &blob, &error);
		if (FAILED(hr))
		{
			Logger::Error(LogCat::RHI, "Root signature error: {}", error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown");
			return nullptr;
		}
		ComPtr<ID3D12RootSignature> rs;
		RD_CHECK_HR(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs)));
		SetDebugName(rs.Get(), name);
		return rs;
	}
}
