#pragma once
#include "Render/Renderer/RenderFeature.h"
#include "RHI/D3D12Common.h"

namespace rd
{
	// トーンマップ: HDR（とブルーム）を、色の調整・ビネット・レトロ表示をかけて sRGB の結果へ書く
	class RD_API TonemapFeature final : public RenderFeature
	{
	public:
		void CreatePipelines(const RenderShared& shared) override;
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

	private:
		ComPtr<ID3D12PipelineState> mPSO;
	};
}
