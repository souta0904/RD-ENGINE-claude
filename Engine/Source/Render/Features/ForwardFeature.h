#pragma once
#include "Render/Renderer/RenderFeature.h"

namespace rd
{
	class MeshDrawer;
	class TransparentDrawer;

	// フォワードの不透明: 1 つのパス "Forward" で、不透明 → 空 → 半透明 / パーティクルを HDR に描く
	//  ライトは物ごとにシェーダーの中で全部足す
	class RD_API ForwardFeature final : public RenderFeature
	{
	public:
		ForwardFeature(MeshDrawer& meshes, TransparentDrawer& transparent) : mMeshes(meshes), mTransparent(transparent) {}

		void CreatePipelines(const RenderShared&) override {}
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

	private:
		MeshDrawer& mMeshes;
		TransparentDrawer& mTransparent;
	};
}
