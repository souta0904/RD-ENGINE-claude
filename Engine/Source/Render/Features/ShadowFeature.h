#pragma once
#include "Math/Geometry/Frustum.h"
#include "Render/GpuData.h"
#include "Render/Renderer/RenderFeature.h"
#include "Render/RenderList/DrawItems.h"
#include "RHI/Texture/DepthTexture.h"
#include <array>
#include <vector>

namespace rd
{
	class MeshDrawer;
	struct RenderCamera;
	struct RenderSettings;

	// 先頭の平行光源のカスケードシャドウ（影の深度を 2x2 のタイルに描く）
	//  1. Setup: カスケードの分け方と行列を決めて FrameContext::data に入れる（GPU へ送る前に呼ぶ）
	//  2. CollectCasters: カスケードごとに、その範囲に入る影を落とす物を集める
	//  3. AddPasses: "Shadow" パス（影を使わないフレームでは登録しない）
	class RD_API ShadowFeature final : public RenderFeature
	{
	public:
		explicit ShadowFeature(MeshDrawer& meshes) : mMeshes(meshes) {}

		void Initialize(uint32_t tileSize);
		void CreatePipelines(const RenderShared&) override {}
		void AddPasses(rg::RenderGraph& graph, FrameContext& frame) override;

		// 影の深度テクスチャ（大きさが設定と違えば作り直す）
		DepthTexture& GetShadowMap(const RenderSettings& settings);
		void Setup(const RenderCamera& camera, const Vector3& lightDir, const RenderSettings& settings, GpuFrameData& data);
		void CollectCasters(FrameContext& frame);

	private:
		MeshDrawer& mMeshes;
		DepthTexture mShadowMap;
		Frustum mCascadeFrustums[kMaxShadowCascades];
		std::array<std::vector<const MeshDrawItem*>, kMaxShadowCascades> mCasters;
	};
}
