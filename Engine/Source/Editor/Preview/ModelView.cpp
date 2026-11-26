#include "RdPch.h"
#include "Editor/Preview/ModelView.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "RHI/Device/GraphicsDevice.h"

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
			// 裏側から見ることもあるので両面にし、床がないので影は落とさない
			m.doubleSided = true;
			m.castShadows = false;
			return m;
		}
	}

	void ModelView::SetModel(const std::string& path)
	{
		if (path == mPath && (mModel || path.empty())) { return; }
		mPath = path;
		mModel = path.empty() ? nullptr : AssetManager::Get().LoadModel(path);
		mSynced = nullptr;
		mMaterials.clear();
		mBindingClip = -2;
		clip = -1;
		time = 0.0f;
	}

	bool ModelView::IsReady() const
	{
		return mModel && mModel->IsReady();
	}

	bool ModelView::IsFullyLoaded()
	{
		if (!mModel) { return true; }
		if (!mModel->IsReady()) { return mModel->IsFailed(); }
		SyncMaterials();
		for (const auto& m : mMaterials)
		{
			if (!IsSettled(m.albedo) || !IsSettled(m.normal) || !IsSettled(m.emissiveMap)) { return false; }
		}
		return true;
	}

	void ModelView::SyncMaterials()
	{
		if (mSynced == mModel.get()) { return; }
		mMaterials.clear();
		for (const auto& d : mModel->GetMaterials()) { mMaterials.push_back(ToMaterial(d)); }
		if (mMaterials.empty()) { mMaterials.push_back(ToMaterial(MaterialDesc{})); }
		for (auto& m : mMaterials) { m.ResolveTextures(); }
		mSynced = mModel.get();
	}

	// クリップを選んでいればその時刻の姿勢、なければバインドポーズ
	D3D12_GPU_VIRTUAL_ADDRESS ModelView::BuildPalette()
	{
		const auto& skeleton = mModel->GetSkeleton();
		if (!skeleton || skeleton->joints.empty()) { return 0; }
		mLocal.resize(skeleton->joints.size());
		for (size_t i = 0; i < mLocal.size(); ++i) { mLocal[i] = skeleton->joints[i].bindLocal; }
		const auto& clips = mModel->GetClips();
		if (clip >= 0 && clip < int(clips.size()))
		{
			const AnimationClip& c = *clips[size_t(clip)];
			if (mBindingClip != clip)
			{
				mBinding = c.BuildBinding(*skeleton);
				mBindingClip = clip;
			}
			float t = c.duration > 0.0f ? std::fmod(time, c.duration) : 0.0f;
			c.Sample(t, mBinding, mLocal);
		}
		ComputeSkinningPalette(*skeleton, mLocal, mGlobal, mPalette);
		return GraphicsDevice::Get().UploadData(mPalette.data(), sizeof(Matrix4) * mPalette.size());
	}

	bool ModelView::Render(uint32_t width, uint32_t height)
	{
		if (!IsReady() || width == 0 || height == 0) { return false; }
		SyncMaterials();
		const Model& model = *mModel;
		const AABB& bounds = model.GetBounds();
		Vector3 center = bounds.GetCenter();
		float radius = std::max(Length(bounds.GetExtents()), 1.0e-3f);

		D3D12_GPU_VIRTUAL_ADDRESS palette = model.HasSkinning() ? BuildPalette() : 0;
		const auto& subs = model.GetSubMeshes();
		for (uint32_t i = 0; i < subs.size(); ++i)
		{
			MeshDrawItem item;
			item.model = &model;
			item.subMesh = i;
			item.material = &mMaterials[std::min<size_t>(subs[i].materialIndex, mMaterials.size() - 1)];
			item.palette = subs[i].skinned ? palette : 0;
			// 動かすと境界から出るので、切り捨てられないように大きめにする
			item.worldBounds = AABB::FromCenterExtents(center, Vector3(radius, radius, radius) * 4.0f);
			mList.meshes.push_back(item);
		}
		DrawList(bounds, width, height);
		return true;
	}
}
