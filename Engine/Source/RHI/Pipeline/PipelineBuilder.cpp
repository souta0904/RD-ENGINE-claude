#include "RdPch.h"
#include "RHI/Pipeline/PipelineBuilder.h"

namespace rd
{
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
		switch (mode)
		{
		case CullMode::Back: mDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK; break;
		case CullMode::Front: mDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; break;
		case CullMode::None: mDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; break;
		}
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
		// ビルダーがコピーされていても正しく指すよう、ここでポインタを付け直す
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
