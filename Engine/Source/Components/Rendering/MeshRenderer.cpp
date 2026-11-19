#include "RdPch.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Components/Animation/Animator.h"
#include "Core/Util/StringUtil.h"
#include "Math/Geometry/Intersection.h"
#include "Render/Material/MaterialAsset.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/RenderList/RenderList.h"
#include "RHI/Device/GraphicsDevice.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	namespace
	{
		Material ToMaterial(const MaterialDesc& d)
		{
			Material m;
			m.baseColor = d.baseColor;
			m.metallic = d.metallic;
			m.roughness = d.roughness;
			m.emissive = d.emissive;
			m.albedoPath = d.albedoPath;
			m.normalPath = d.normalPath;
			m.blend = d.transparent ? BlendMode::Alpha : BlendMode::Opaque;
			return m;
		}
	}

	// 読み込むときはファイルのマテリアルを残し、編集でモデルを変えたときはモデルの既定値へ戻す
	// マテリアルの数はモデルで決まるので、読み込むときは JSON の数に合わせる
	//
	// 枠ごとに "Material i" のまとまりを作る:
	//  "Asset" にアセットのパス（空でも必ず書く。Undo で「入れる前」へ戻したときに空へ戻すため）。
	//  アセットがなければ、続けてこのレンダラー専用のマテリアルの値を並べる。
	//  前からのシーンには "Asset" がないので、空のまま専用のマテリアルを読む
	void MeshRenderer::Reflect(PropertyVisitor& v)
	{
		if (v.Property("Model", modelPath, { .flags = PropertyMeta::AssetModel }))
		{
			if (!v.IsLoading()) { mResetMaterials = true; }
			SetModel(modelPath);
		}
		v.Property("Tint", tint);

		MatchSlotArrays();
		uint32_t count = uint32_t(mMaterials.size());
		v.Property("Material Count", count, { .flags = PropertyMeta::ReadOnly });
		if (v.IsLoading() && count != mMaterials.size()) { ResizeSlots(count); }
		for (size_t i = 0; i < mMaterials.size(); ++i)
		{
			std::string name = std::format("Material {}", i);
			if (v.BeginGroup(name.c_str()))
			{
				if (v.Property("Asset", mMaterialPaths[i], { .flags = PropertyMeta::AssetMaterial,
					.tooltip = "Material asset (.rdmat). Empty = this renderer's own material below" }))
				{
					ResolveSharedMaterial(i);
				}
				if (mMaterialPaths[i].empty()) { mMaterials[i].Reflect(v); }
				v.EndGroup();
			}
		}
	}

	void MeshRenderer::ResizeSlots(size_t count)
	{
		mMaterials.resize(count);
		mMaterialPaths.resize(count);
		mMaterialAssets.resize(count);
	}

	// ゲームのコードが GetMaterials() の配列ごと入れ替えて数が変わったときに、パスとアセットの数を合わせる
	void MeshRenderer::MatchSlotArrays()
	{
		if (mMaterialPaths.size() == mMaterials.size()) { return; }
		mMaterialPaths.resize(mMaterials.size());
		mMaterialAssets.resize(mMaterials.size());
	}

	void MeshRenderer::ResolveSharedMaterial(size_t index)
	{
		const std::string& path = mMaterialPaths[index];
		AssetRef<MaterialAsset>& asset = mMaterialAssets[index];
		if (path.empty()) { asset.reset(); return; }
		if (!asset || asset->GetPath() != NormalizePath(path)) { asset = AssetManager::Get().LoadNow<MaterialAsset>(path); }
	}

	// アセットを入れている枠は、今のアセットの値を写して専用のものに切り替える（アセットは変わらない）
	std::vector<Material>& MeshRenderer::GetMaterials()
	{
		for (size_t i = 0; i < mMaterials.size(); ++i)
		{
			if (mMaterialPaths[i].empty()) { continue; }
			if (mMaterialAssets[i] && mMaterialAssets[i]->IsReady()) { mMaterials[i] = mMaterialAssets[i]->material; }
			mMaterialPaths[i].clear();
			mMaterialAssets[i].reset();
		}
		return mMaterials;
	}

	const std::string& MeshRenderer::GetSharedMaterialPath(size_t index) const
	{
		static const std::string kEmpty;
		return index < mMaterialPaths.size() ? mMaterialPaths[index] : kEmpty;
	}

	MaterialAsset* MeshRenderer::GetSharedMaterial(size_t index) const
	{
		return index < mMaterialAssets.size() ? mMaterialAssets[index].get() : nullptr;
	}

	bool MeshRenderer::SetSharedMaterial(size_t index, const std::string& path)
	{
		if (index >= mMaterials.size()) { return false; }
		if (path.empty())
		{
			// 外すときは、見た目が急に変わらないようにアセットの値を専用のものへ写しておく
			if (mMaterialAssets[index] && mMaterialAssets[index]->IsReady()) { mMaterials[index] = mMaterialAssets[index]->material; }
		}
		mMaterialPaths[index] = path;
		ResolveSharedMaterial(index);
		return true;
	}

	const Material& MeshRenderer::GetRenderMaterial(size_t index) const
	{
		const MaterialAsset* asset = GetSharedMaterial(index);
		return asset && asset->IsReady() ? asset->material : mMaterials[index];
	}

	void MeshRenderer::OnValidate()
	{
		SetModel(modelPath);
	}

	void MeshRenderer::SetModel(const std::string& path)
	{
		modelPath = path;
		if (path.empty()) { mModel.reset(); return; }
		if (!mModel || mModel->GetPath() != path)
		{
			mModel = AssetManager::Get().LoadModel(path);
			mSyncedModel = nullptr;
			mBindPalette.clear();
		}
	}

	void MeshRenderer::SyncMaterials()
	{
		if (!mModel || !mModel->IsReady() || mSyncedModel == mModel.get()) { return; }
		// 数が合わないときは、読み込んだマテリアルを残して足りない分だけモデルの既定値で埋める
		const auto& defaults = mModel->GetMaterials();
		if (mResetMaterials || mMaterials.empty())
		{
			// モデルを変えたら、入れていたアセットも外してモデルの既定値から始める
			ResizeSlots(0);
			ResizeSlots(defaults.size());
			for (size_t i = 0; i < defaults.size(); ++i) { mMaterials[i] = ToMaterial(defaults[i]); }
		}
		else if (mMaterials.size() != defaults.size())
		{
			size_t loaded = mMaterials.size();
			ResizeSlots(defaults.size());
			for (size_t i = loaded; i < defaults.size(); ++i) { mMaterials[i] = ToMaterial(defaults[i]); }
		}
		MatchSlotArrays();
		mResetMaterials = false;
		mSyncedModel = mModel.get();
	}

	D3D12_GPU_VIRTUAL_ADDRESS MeshRenderer::GetPalette()
	{
		if (auto animator = GetComponent<Animator>(); animator && animator->IsEnabled())
		{
			if (auto gpu = animator->GetPaletteGpu(mModel.get())) { return gpu; }
		}
		const auto& skeleton = mModel->GetSkeleton();
		if (!skeleton) { return 0; }
		if (mBindPalette.empty())
		{
			std::vector<TransformTRS> local;
			for (const auto& j : skeleton->joints) { local.push_back(j.bindLocal); }
			std::vector<Matrix4> global;
			ComputeSkinningPalette(*skeleton, local, global, mBindPalette);
		}
		return GraphicsDevice::Get().UploadData(mBindPalette.data(), sizeof(Matrix4) * mBindPalette.size());
	}

	// スキンメッシュは動くので境界を少し広げておく
	void MeshRenderer::OnRender(RenderList& list)
	{
		if (!mModel || !mModel->IsReady()) { return; }
		SyncMaterials();
		if (mMaterials.empty()) { return; }

		const Matrix4& world = GetTransform().GetWorldMatrix();
		D3D12_GPU_VIRTUAL_ADDRESS palette = mModel->HasSkinning() ? GetPalette() : 0;
		const auto& subs = mModel->GetSubMeshes();
		for (uint32_t i = 0; i < subs.size(); ++i)
		{
			size_t slot = std::min<size_t>(subs[i].materialIndex, mMaterials.size() - 1);
			// アセットを入れた枠はアセットの値で描く（読めなかったときは専用のもので代わりにする）
			MaterialAsset* shared = GetSharedMaterial(slot);
			Material& mat = shared && shared->IsReady() ? shared->material : mMaterials[slot];
			mat.ResolveTextures();
			MeshDrawItem item;
			item.model = mModel.get();
			item.subMesh = i;
			item.world = world;
			item.material = &mat;
			item.palette = subs[i].skinned ? palette : 0;
			item.tint = tint;
			AABB local = subs[i].bounds;
			if (subs[i].skinned) { local = AABB::FromCenterExtents(local.GetCenter(), local.GetExtents() * 1.5f); }
			item.worldBounds = local.Transform(world);
			list.meshes.push_back(item);
		}
	}

#if RD_EDITOR
	void MeshRenderer::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		if (selected && mModel && mModel->IsReady())
		{
			draw.Box(GetWorldBounds(), Color(1.0f, 0.6f, 0.1f, 0.8f));
		}
	}
#endif

	AABB MeshRenderer::GetWorldBounds() const
	{
		if (!mModel || !mModel->IsReady()) { return AABB::FromCenterExtents(GetTransform().GetPosition(), Vector3(0.5f, 0.5f, 0.5f)); }
		return mModel->GetBounds().Transform(GetTransform().GetWorldMatrix());
	}

	// レイをモデルのローカル空間へ移して三角形と調べる（距離は拡大率で戻す）
	// スキンメッシュは姿勢が変わるので、三角形に当たらなければ境界の箱で代わりにする
	bool MeshRenderer::Raycast(const Ray& ray, float& outDistance, int* outSubMesh) const
	{
		if (outSubMesh) { *outSubMesh = -1; }
		if (!mModel || !mModel->IsReady()) { return false; }
		float t;
		if (!RayIntersectsAABB(ray, GetWorldBounds(), t)) { return false; }
		const Matrix4& world = GetTransform().GetWorldMatrix();
		Matrix4 inv = Inverse(world);
		Vector3 o = TransformPoint(ray.origin, inv);
		Vector3 d = TransformVector(ray.direction, inv);
		float scale = Length(d);
		Ray local{ o, d / scale };
		const auto& pos = mModel->GetCollisionPositions();
		const auto& idx = mModel->GetCollisionIndices();
		float best = math::kInfinity;
		size_t bestIndex = 0;
		for (size_t i = 0; i + 2 < idx.size(); i += 3)
		{
			if (RayIntersectsTriangle(local, pos[idx[i]], pos[idx[i + 1]], pos[idx[i + 2]], t) && t < best) { best = t; bestIndex = i; }
		}
		// 当たった三角形のインデックスの位置から、どのサブメッシュの範囲かを探す
		if (outSubMesh && best != math::kInfinity)
		{
			const auto& subs = mModel->GetSubMeshes();
			for (size_t s = 0; s < subs.size(); ++s)
			{
				if (bestIndex >= subs[s].indexStart && bestIndex < size_t(subs[s].indexStart) + subs[s].indexCount) { *outSubMesh = int(s); break; }
			}
		}
		if (best == math::kInfinity)
		{
			if (!mModel->HasSkinning()) { return false; }
			RayIntersectsAABB(ray, GetWorldBounds(), best);
			outDistance = best;
			return true;
		}
		outDistance = best / scale;
		return true;
	}
}
