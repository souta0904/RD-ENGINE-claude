#include "RdPch.h"
#include "RHI/Pipeline.h"

namespace rd
{
	// ==================================================
	// RootSignatureBuilder
	// ==================================================
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
			Log::Error(LogCat::RHI, "Root signature error: {}", error ? static_cast<const char*>(error->GetBufferPointer()) : "unknown");
			return nullptr;
		}
		ComPtr<ID3D12RootSignature> rs;
		RD_CHECK_HR(device->CreateRootSignature(0, blob->GetBufferPointer(), blob->GetBufferSize(), IID_PPV_ARGS(&rs)));
		SetDebugName(rs.Get(), name);
		return rs;
	}

	// ==================================================
	// Samplers
	// ==================================================
	namespace Samplers
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

	// ==================================================
	// PipelineBuilder
	// ==================================================
	PipelineBuilder::PipelineBuilder()
	{
		mDesc.SampleMask = UINT_MAX;
		mDesc.SampleDesc.Count = 1;
		mDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
		mDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
		mDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
		mDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
		mDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
		mDesc.NumRenderTargets = 1;
		mDesc.RTVFormats[0] = DXGI_FORMAT_R16G16B16A16_FLOAT;
		mDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	}

	PipelineBuilder& PipelineBuilder::SetRootSignature(ID3D12RootSignature* rs) { mDesc.pRootSignature = rs; return *this; }

	PipelineBuilder& PipelineBuilder::SetVS(const Shader* shader)
	{
		if (!shader || !shader->IsValid()) { mValid = false; return *this; }
		mDesc.VS = shader->GetBytecode();
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetPS(const Shader* shader)
	{
		if (!shader || !shader->IsValid()) { mValid = false; return *this; }
		mDesc.PS = shader->GetBytecode();
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetInputLayout(std::span<const D3D12_INPUT_ELEMENT_DESC> elements)
	{
		mInputLayout.assign(elements.begin(), elements.end());
		mDesc.InputLayout = { mInputLayout.data(), UINT(mInputLayout.size()) };
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetBlend(BlendMode mode)
	{
		auto& rt = mDesc.BlendState.RenderTarget[0];
		rt = {};
		rt.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		rt.BlendOp = rt.BlendOpAlpha = D3D12_BLEND_OP_ADD;
		rt.SrcBlendAlpha = D3D12_BLEND_ONE;
		rt.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		switch (mode)
		{
		case BlendMode::Opaque:
			rt.BlendEnable = FALSE;
			rt.SrcBlend = D3D12_BLEND_ONE;
			rt.DestBlend = D3D12_BLEND_ZERO;
			break;
		case BlendMode::Alpha:
			rt.BlendEnable = TRUE;
			rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			break;
		case BlendMode::Additive:
			rt.BlendEnable = TRUE;
			rt.SrcBlend = D3D12_BLEND_SRC_ALPHA;
			rt.DestBlend = D3D12_BLEND_ONE;
			rt.DestBlendAlpha = D3D12_BLEND_ONE;
			break;
		case BlendMode::Multiply:
			rt.BlendEnable = TRUE;
			rt.SrcBlend = D3D12_BLEND_ZERO;
			rt.DestBlend = D3D12_BLEND_SRC_COLOR;
			break;
		case BlendMode::Premultiplied:
			rt.BlendEnable = TRUE;
			rt.SrcBlend = D3D12_BLEND_ONE;
			rt.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
			break;
		}
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetCull(CullMode mode)
	{
		mDesc.RasterizerState.CullMode =
			mode == CullMode::Back ? D3D12_CULL_MODE_BACK : (mode == CullMode::Front ? D3D12_CULL_MODE_FRONT : D3D12_CULL_MODE_NONE);
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetDepth(DepthMode mode, D3D12_COMPARISON_FUNC func)
	{
		auto& ds = mDesc.DepthStencilState;
		ds.DepthEnable = mode != DepthMode::Disabled;
		ds.DepthWriteMask = mode == DepthMode::ReadWrite ? D3D12_DEPTH_WRITE_MASK_ALL : D3D12_DEPTH_WRITE_MASK_ZERO;
		ds.DepthFunc = func;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetDepthBias(int bias, float slopeScaled, float clamp)
	{
		mDesc.RasterizerState.DepthBias = bias;
		mDesc.RasterizerState.SlopeScaledDepthBias = slopeScaled;
		mDesc.RasterizerState.DepthBiasClamp = clamp;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetWireframe(bool wireframe)
	{
		mDesc.RasterizerState.FillMode = wireframe ? D3D12_FILL_MODE_WIREFRAME : D3D12_FILL_MODE_SOLID;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type)
	{
		mDesc.PrimitiveTopologyType = type;
		return *this;
	}

	PipelineBuilder& PipelineBuilder::SetRenderTargets(std::initializer_list<DXGI_FORMAT> formats, DXGI_FORMAT depthFormat)
	{
		mDesc.NumRenderTargets = UINT(formats.size());
		UINT i = 0;
		for (auto f : formats) { mDesc.RTVFormats[i++] = f; }
		for (; i < 8; ++i) { mDesc.RTVFormats[i] = DXGI_FORMAT_UNKNOWN; }
		mDesc.DSVFormat = depthFormat;
		return *this;
	}

	ComPtr<ID3D12PipelineState> PipelineBuilder::Build(ID3D12Device* device, const wchar_t* name)
	{
		if (!mValid || !mDesc.pRootSignature)
		{
			Log::Error(LogCat::RHI, "Pipeline build skipped (invalid shader or root signature)");
			return nullptr;
		}
		// ビルダーがコピーされてもよいように、ここでポインタを結び直す
		mDesc.InputLayout = { mInputLayout.empty() ? nullptr : mInputLayout.data(), UINT(mInputLayout.size()) };
		ComPtr<ID3D12PipelineState> pso;
		HRESULT hr = device->CreateGraphicsPipelineState(&mDesc, IID_PPV_ARGS(&pso));
		if (FAILED(hr))
		{
			Log::Error(LogCat::RHI, "CreateGraphicsPipelineState failed (0x{:08X})", uint32_t(hr));
			return nullptr;
		}
		SetDebugName(pso.Get(), name);
		return pso;
	}
}
