#pragma once
#include "RHI/D3D12Common.h"
#include "RHI/ShaderCompiler.h"
#include <vector>

namespace rd
{
	// ルートシグネチャの構築ヘルパー
	class RootSignatureBuilder
	{
	public:
		RootSignatureBuilder& AddCbv(uint32_t reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddSrv(uint32_t reg, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL);// ルート SRV（構造化バッファ用）
		RootSignatureBuilder& AddSrvTable(uint32_t reg, uint32_t count = 1, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_PIXEL);
		RootSignatureBuilder& AddConstants(uint32_t reg, uint32_t num32BitValues, D3D12_SHADER_VISIBILITY vis = D3D12_SHADER_VISIBILITY_ALL);
		RootSignatureBuilder& AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);
		ComPtr<ID3D12RootSignature> Build(ID3D12Device* device, const wchar_t* name = nullptr);

	private:
		std::vector<CD3DX12_ROOT_PARAMETER1> mParams;
		std::vector<std::unique_ptr<CD3DX12_DESCRIPTOR_RANGE1>> mRanges;
		std::vector<D3D12_STATIC_SAMPLER_DESC> mSamplers;
	};

	// よく使うサンプラー
	namespace Samplers
	{
		D3D12_STATIC_SAMPLER_DESC LinearWrap(uint32_t reg);
		D3D12_STATIC_SAMPLER_DESC LinearClamp(uint32_t reg);
		D3D12_STATIC_SAMPLER_DESC PointClamp(uint32_t reg);
		D3D12_STATIC_SAMPLER_DESC AnisotropicWrap(uint32_t reg);
		D3D12_STATIC_SAMPLER_DESC ShadowCompare(uint32_t reg);
	}

	enum class BlendMode : uint8_t { Opaque, Alpha, Additive, Multiply, Premultiplied };
	enum class CullMode : uint8_t { Back, Front, None };
	enum class DepthMode : uint8_t { ReadWrite, ReadOnly, Disabled };

	// グラフィックスパイプラインの構築ヘルパー（妥当な既定値付き）
	class PipelineBuilder
	{
	public:
		PipelineBuilder();
		PipelineBuilder& SetRootSignature(ID3D12RootSignature* rs);
		PipelineBuilder& SetVS(const Shader* shader);
		PipelineBuilder& SetPS(const Shader* shader);
		PipelineBuilder& SetInputLayout(std::span<const D3D12_INPUT_ELEMENT_DESC> elements);
		PipelineBuilder& SetBlend(BlendMode mode);
		PipelineBuilder& SetCull(CullMode mode);
		PipelineBuilder& SetDepth(DepthMode mode, D3D12_COMPARISON_FUNC func = D3D12_COMPARISON_FUNC_LESS_EQUAL);
		PipelineBuilder& SetDepthBias(int bias, float slopeScaled, float clamp = 0.0f);
		PipelineBuilder& SetWireframe(bool wireframe);
		PipelineBuilder& SetTopology(D3D12_PRIMITIVE_TOPOLOGY_TYPE type);
		PipelineBuilder& SetRenderTargets(std::initializer_list<DXGI_FORMAT> formats, DXGI_FORMAT depthFormat);
		ComPtr<ID3D12PipelineState> Build(ID3D12Device* device, const wchar_t* name = nullptr);

		D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc() { return mDesc; }

	private:
		D3D12_GRAPHICS_PIPELINE_STATE_DESC mDesc = {};
		std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
		bool mValid = true;
	};
}
