#pragma once
#include "Render/Renderer/RenderFeature.h"
#include "RHI/D3D12Common.h"

namespace rd
{
	class GpuTexture;

	// 縁取り（RenderList::outlines）。形を印に塗ってから、広げた形を印のない所だけ HDR に描く（Unity のステンシルの代わり）
	//  1 物ごとの定数は縁の色・幅・点滅（マテリアルの metallic / roughness の場所に入れる）
	class RD_API OutlineFeature final : public RenderFeature
	{
	public:
		void CreatePipelines(const RenderShared& shared) override;
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

	private:
		// mask = true で形を印に塗る、false で広げた形を印のない所だけ描く（印は maskTexture を t0 に置いて読む）
		void Draw(ID3D12GraphicsCommandList* cmd, bool mask, const GpuTexture* maskTexture, const GpuTexture& shadowMap, FrameContext& frame);

		// [0] は静的、[1] はスキン
		ComPtr<ID3D12PipelineState> mMaskPSO[2];
		ComPtr<ID3D12PipelineState> mOutlinePSO[2];
	};
}
