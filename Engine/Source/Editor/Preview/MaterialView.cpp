#include "RdPch.h"
#include "Editor/Preview/MaterialView.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Render/Material/MaterialAsset.h"

namespace rd
{
	void MaterialView::SetMaterial(const std::string& path)
	{
		if (path == mPath && mAsset) { return; }
		mPath = path;
		mAsset = path.empty() ? nullptr : AssetManager::Get().LoadNow<MaterialAsset>(path);
		if (!mSphere) { mSphere = AssetManager::Get().LoadModel("Engine/Sphere"); }
		mRenderedVersion = ~0u;
	}

	bool MaterialView::IsReady() const
	{
		return mAsset && mAsset->IsReady() && mSphere && mSphere->IsReady();
	}

	bool MaterialView::IsFullyLoaded()
	{
		if (!mAsset || mAsset->IsFailed() || (mSphere && mSphere->IsFailed())) { return true; }
		if (!IsReady()) { return false; }
		Material& m = mAsset->material;
		m.ResolveTextures();
		return IsSettled(m.albedo) && IsSettled(m.normal) && IsSettled(m.emissiveMap);
	}

	bool MaterialView::NeedsRefresh() const
	{
		return mAsset && mAsset->GetVersion() != mRenderedVersion;
	}

	// アセットの値をそのまま使う（写しを作ると、編集した値がプレビューに出るまで遅れる）
	bool MaterialView::Render(uint32_t width, uint32_t height)
	{
		if (!IsReady() || width == 0 || height == 0) { return false; }
		Material& m = mAsset->material;
		m.ResolveTextures();
		const Model& sphere = *mSphere;
		MeshDrawItem item;
		item.model = &sphere;
		item.subMesh = 0;
		item.material = &m;
		item.worldBounds = sphere.GetBounds();
		mList.meshes.push_back(item);
		// 球はどの向きでも同じなので、包む箱ではなく球の大きさで寄せる（箱の対角だと遠くなりすぎる）
		AABB bounds = AABB::FromCenterExtents(sphere.GetBounds().GetCenter(), sphere.GetBounds().GetExtents() * 0.62f);
		DrawList(bounds, width, height);
		mRenderedVersion = mAsset->GetVersion();
		return true;
	}
}
