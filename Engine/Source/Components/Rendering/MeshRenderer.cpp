#include "RdPch.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Components/Animation/Animator.h"
#include "Math/Geometry/Intersection.h"
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
	void MeshRenderer::Reflect(PropertyVisitor& v)
	{
		if (v.Property("Model", modelPath, { .flags = PropertyMeta::AssetModel }))
		{
			if (!v.IsLoading()) { mResetMaterials = true; }
			SetModel(modelPath);
		}
		v.Property("Tint", tint);

		uint32_t count = uint32_t(mMaterials.size());
		v.Property("Material Count", count, { .flags = PropertyMeta::ReadOnly });
		if (v.IsLoading() && count != mMaterials.size()) { mMaterials.resize(count); }
		for (size_t i = 0; i < mMaterials.size(); ++i)
		{
			std::string name = std::format("Material {}", i);
			if (v.BeginGroup(name.c_str()))
			{
				mMaterials[i].Reflect(v);
				v.EndGroup();
			}
		}
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
			mMaterials.clear();
			for (const auto& d : defaults) { mMaterials.push_back(ToMaterial(d)); }
		}
		else if (mMaterials.size() != defaults.size())
		{
			size_t loaded = mMaterials.size();
			mMaterials.resize(defaults.size());
			for (size_t i = loaded; i < defaults.size(); ++i) { mMaterials[i] = ToMaterial(defaults[i]); }
		}
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
			Material& mat = mMaterials[std::min<size_t>(subs[i].materialIndex, mMaterials.size() - 1)];
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
	bool MeshRenderer::Raycast(const Ray& ray, float& outDistance) const
	{
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
		for (size_t i = 0; i + 2 < idx.size(); i += 3)
		{
			if (RayIntersectsTriangle(local, pos[idx[i]], pos[idx[i + 1]], pos[idx[i + 2]], t) && t < best) { best = t; }
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
