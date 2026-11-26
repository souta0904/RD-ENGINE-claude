#include "RdPch.h"
#include "Editor/Preview/PrefabView.h"
#include "Asset/Model/Model.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Scene/Scene.h"
#include <fstream>

namespace rd
{
	namespace
	{
		// Unity のプレハブは "%YAML" で始まるテキスト。エンジンのプレハブは JSON なので読めない
		bool IsUnityYaml(const std::string& path)
		{
			std::ifstream file(ToWide(path), std::ios::binary);
			char head[5] = {};
			file.read(head, sizeof(head));
			return file.gcount() == sizeof(head) && std::string_view(head, sizeof(head)) == "%YAML";
		}
	}

	PrefabView::PrefabView() = default;
	PrefabView::~PrefabView() = default;

	// 選び直すたびにシーンごと作り直す（前のプレハブのアクターは残さない）
	void PrefabView::SetPrefab(const std::string& path)
	{
		if (path == mPath) { return; }
		mPath = path;
		mError.clear();
		mScene.reset();
		if (path.empty()) { return; }
		if (IsUnityYaml(path))
		{
			mError = "Unity prefab (YAML) cannot be previewed. Use the exported one.";
			return;
		}
		mScene = std::make_unique<Scene>();
		mScene->SetName("Prefab Preview");
		if (!mScene->Instantiate(path))
		{
			mScene.reset();
			mError = "Failed to load prefab";
		}
	}

	void PrefabView::Reload()
	{
		std::string path = mPath;
		mPath.clear();
		SetPrefab(path);
	}

	size_t PrefabView::GetActorCount() const
	{
		return mScene ? mScene->GetActorCount() : 0;
	}

	bool PrefabView::IsReady() const
	{
		return mScene != nullptr;
	}

	// どのモデルもテクスチャも読み終わったか（マテリアルは一度描くとそろうので、まだなら未完了）
	bool PrefabView::IsFullyLoaded()
	{
		if (!mScene) { return true; }
		for (MeshRenderer* mr : mScene->FindComponents<MeshRenderer>())
		{
			const auto& model = mr->GetModel();
			if (!model || model->IsFailed()) { continue; }
			// GetMaterials() はアセットを外して写しに切り替えてしまうので、描く用の読み取りだけを使う
			if (!model->IsReady() || mr->GetMaterialCount() == 0) { return false; }
			for (size_t i = 0; i < mr->GetMaterialCount(); ++i)
			{
				const Material& m = mr->GetRenderMaterial(i);
				if (!IsSettled(m.albedo) || !IsSettled(m.normal) || !IsSettled(m.emissiveMap)) { return false; }
			}
		}
		return true;
	}

	// MeshRenderer の箱をまとめる。何もなければアクターの位置のまわり
	AABB PrefabView::ComputeBounds() const
	{
		AABB bounds;
		for (MeshRenderer* mr : mScene->FindComponents<MeshRenderer>()) { bounds.Expand(mr->GetWorldBounds()); }
		if (!bounds.IsValid())
		{
			mScene->ForEachActor([&](Actor* a) { bounds.Expand(a->GetTransform().GetPosition()); });
			bounds = AABB::FromCenterExtents(bounds.IsValid() ? bounds.GetCenter() : Vector3::kZero, Vector3(0.5f, 0.5f, 0.5f));
		}
		return bounds;
	}

	// プレイ中ではないので、描くのはエディタで置いたときと同じ姿（アニメーションはバインドポーズ）
	bool PrefabView::Render(uint32_t width, uint32_t height)
	{
		if (!mScene || width == 0 || height == 0) { return false; }
		mScene->CollectRender(mList);
		DrawList(ComputeBounds(), width, height);
		return true;
	}
}
