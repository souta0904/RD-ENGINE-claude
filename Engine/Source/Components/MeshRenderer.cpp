#include "RdPch.h"
#include "Components/MeshRenderer.h"
#include "Asset/AssetManager.h"
#include "Asset/Model.h"
#include "Components/Animator.h"
#include "Render/DebugDraw.h"
#include "RHI/GraphicsDevice.h"
#include "Scene/Actor.h"

namespace rd
{
	void MeshRenderer::Reflect(PropertyVisitor& v)
	{
		if (v.Property("Model", modelPath, { .flags = PropertyMeta::AssetModel }))
		{
			// 読み込み時はファイルのマテリアルを維持、編集時はモデルの既定値へ戻す
			if (!v.IsLoading()) { mResetMaterials = true; }
			SetModel(modelPath);
		}
		v.Property("Tint", tint);

		// マテリアル数はモデル次第なので、読み込み時は JSON 側の個数に合わせる
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
		const auto& defaults = mModel->GetMaterials();
		bool reset = mResetMaterials || mMaterials.size() != defaults.size();
		if (reset)
		{
			mMaterials.clear();
			for (const auto& d : defaults)
			{
				Material m;
				m.baseColor = d.baseColor;
				m.metallic = d.metallic;
				m.roughness = d.roughness;
				m.emissive = d.emissive;
				m.albedoPath = d.albedoPath;
				m.normalPath = d.normalPath;
				m.blend = d.transparent ? BlendMode::Alpha : BlendMode::Opaque;
				mMaterials.push_back(std::move(m));
			}
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
		// アニメーターがなければバインドポーズ
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
			// スキンメッシュは動くので境界を少し広げておく
			AABB local = subs[i].bounds;
			if (subs[i].skinned) { local = AABB::FromCenterExtents(local.GetCenter(), local.GetExtents() * 1.5f); }
			item.worldBounds = local.Transform(world);
			list.meshes.push_back(item);
		}
	}

	void MeshRenderer::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		if (selected && mModel && mModel->IsReady())
		{
			draw.Box(GetWorldBounds(), Color(1.0f, 0.6f, 0.1f, 0.8f));
		}
	}

	AABB MeshRenderer::GetWorldBounds() const
	{
		if (!mModel || !mModel->IsReady()) { return AABB::FromCenterExtents(GetTransform().GetPosition(), Vector3(0.5f, 0.5f, 0.5f)); }
		return mModel->GetBounds().Transform(GetTransform().GetWorldMatrix());
	}

	bool MeshRenderer::Raycast(const Ray& ray, float& outDistance) const
	{
		if (!mModel || !mModel->IsReady()) { return false; }
		float t;
		if (!RayIntersectsAABB(ray, GetWorldBounds(), t)) { return false; }
		// ローカル空間で三角形と判定
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
			// スキンメッシュは姿勢が変わるので境界箱で代用
			if (!mModel->HasSkinning()) { return false; }
			RayIntersectsAABB(ray, GetWorldBounds(), best);
			outDistance = best;
			return true;
		}
		outDistance = best / scale;
		return true;
	}
}
