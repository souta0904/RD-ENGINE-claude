#pragma once
#include "Render/Renderer/RenderFeature.h"
#include "RHI/D3D12Common.h"

namespace rd
{
	class MeshDrawer;
	class TransparentDrawer;

	// ディファードの不透明: 不透明を G バッファーへ書く → 画面全体で光を当てる → 空と半透明を重ねる
	//  G バッファー（RGBA16F x 3）: 0 色 + 金属感 / 1 法線 + 粗さ / 2 発光 + 種類（色だけ・影を受けるか）
	//  ライティングはメッシュ用のルートシグネチャで、G バッファーを t0 / t1 / t4、深度を t5 に置いて DeferredLighting.hlsl で行う
	class RD_API DeferredFeature final : public RenderFeature
	{
	public:
		DeferredFeature(MeshDrawer& meshes, TransparentDrawer& transparent) : mMeshes(meshes), mTransparent(transparent) {}

		void CreatePipelines(const RenderShared& shared) override;
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;
		// ライティングのシェーダーが作れなかったら false（そのときはフォワードで描く）
		bool IsAvailable() const { return mLightingPSO != nullptr; }

	private:
		MeshDrawer& mMeshes;
		TransparentDrawer& mTransparent;
		ComPtr<ID3D12PipelineState> mLightingPSO;
	};
}
