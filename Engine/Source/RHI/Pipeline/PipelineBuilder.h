#pragma once
#include "RHI/Shader/Shader.h"
#include <initializer_list>
#include <span>
#include <vector>

namespace rd
{
	enum class BlendMode : uint8_t { Opaque, Alpha, Additive, Multiply, Premultiplied };
	enum class CullMode : uint8_t { Back, Front, None };
	enum class DepthMode : uint8_t { ReadWrite, ReadOnly, Disabled };

	// グラフィックスパイプラインを組み立てる（よく使う既定値が入っている）
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
