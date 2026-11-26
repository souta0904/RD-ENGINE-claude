#pragma once
#include "Render/Renderer/RenderFeature.h"
#include "RHI/D3D12Common.h"

namespace rd
{
	class RenderTexture;

	// ブルーム: 明るい部分を取り出して半分に縮め、さらに段階的に縮めてから、拡大しながら上の段へ足し戻す
	//  結果（いちばん大きい段）を FrameContext::textures.bloom に入れ、トーンマップが足す
	class RD_API BloomFeature final : public RenderFeature
	{
	public:
		static constexpr uint32_t kLevels = 5;

		void CreatePipelines(const RenderShared& shared) override;
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

	private:
		// 描く先と大きさを置く（状態はレンダーグラフが切り替えてある）
		static void SetTarget(ID3D12GraphicsCommandList* cmd, RenderTexture& target);

		ComPtr<ID3D12PipelineState> mPrefilterPSO;
		ComPtr<ID3D12PipelineState> mDownPSO;
		ComPtr<ID3D12PipelineState> mUpPSO;
	};
}
