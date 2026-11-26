#pragma once
#include "Render/Debug/DebugDraw.h"
#include "Render/Renderer/RenderFeature.h"
#include "RHI/D3D12Common.h"
#include <vector>

namespace rd
{
	// トーンマップの後の画面に重ねるもの: デバッグの面と線（奥行きを見るもの / 手前に重ねるもの）と 2D のスプライト
	class RD_API OverlayFeature final : public RenderFeature
	{
	public:
		void CreatePipelines(const RenderShared& shared) override;
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

	private:
		// デバッグ描画の頂点を描く（topology が線なら 2 頂点で 1 本、三角形なら 3 頂点で 1 枚）
		void DrawDebugVertices(ID3D12GraphicsCommandList* cmd, const std::vector<DebugDraw::Vertex>& vertices, ID3D12PipelineState* pso,
			D3D12_PRIMITIVE_TOPOLOGY topology, FrameContext& frame);
		// レイヤーの順に並べて頂点を作り、同じテクスチャが続く範囲を 1 回の描画にまとめる
		void DrawSprites(ID3D12GraphicsCommandList* cmd, uint32_t width, uint32_t height, FrameContext& frame);

		ComPtr<ID3D12PipelineState> mLineDepthPSO;
		ComPtr<ID3D12PipelineState> mLineOverlayPSO;
		ComPtr<ID3D12PipelineState> mTriDepthPSO;
		ComPtr<ID3D12PipelineState> mTriOverlayPSO;
		ComPtr<ID3D12PipelineState> mSpritePSO;
	};
}
