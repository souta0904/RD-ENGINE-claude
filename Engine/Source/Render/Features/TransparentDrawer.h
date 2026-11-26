#pragma once
#include "RHI/D3D12Common.h"

namespace rd
{
	class MeshDrawer;
	class RenderShared;
	struct FrameContext;

	// 空・半透明のメッシュ・パーティクル・半透明の三角形を描く道具
	//  フォワードでは不透明の続きに、ディファードではライティングの後に、同じものを同じ順に描く
	class RD_API TransparentDrawer
	{
	public:
		explicit TransparentDrawer(MeshDrawer& meshes) : mMeshes(meshes) {}

		void CreatePipelines(const RenderShared& shared);
		// HDR と深度を描く先に置き、FrameContext::BindMeshPass を呼んだ後に呼ぶ
		void Draw(ID3D12GraphicsCommandList* cmd, FrameContext& frame);

	private:
		void DrawParticles(ID3D12GraphicsCommandList* cmd, FrameContext& frame);
		// ルートシグネチャを simple に切り替えるので、最後に描く
		void DrawTranslucentTriangles(ID3D12GraphicsCommandList* cmd, FrameContext& frame);

		MeshDrawer& mMeshes;
		ComPtr<ID3D12PipelineState> mSkyPSO;
		ComPtr<ID3D12PipelineState> mParticleAlphaPSO;
		ComPtr<ID3D12PipelineState> mParticleAddPSO;
		ComPtr<ID3D12PipelineState> mTranslucentTrianglePSO;
	};
}
