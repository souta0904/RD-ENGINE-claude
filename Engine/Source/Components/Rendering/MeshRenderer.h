#pragma once
#include "Asset/Asset.h"
#include "Math/Geometry/AABB.h"
#include "Math/Geometry/Ray.h"
#include "Render/Material/Material.h"
#include "Scene/Component/Component.h"
#include <vector>

namespace rd
{
	class Model;
	class MaterialAsset;

	// モデルを描く（スキンメッシュは同じアクターの Animator の姿勢を使い、なければバインドポーズ）
	//
	// マテリアルの枠（スロット）はモデルのマテリアルの数だけある。Unity の Materials の要素と同じ。
	//  枠にマテリアルのアセット（.rdmat）を入れると、そのファイルの値で描く（ほかのレンダラーと共有）。
	//  空なら、このレンダラーだけが持つマテリアルで描く（はじめはモデルの既定値。前からのシーンはこちら）。
	//
	//  ゲームのコードから色などを変えるときは GetMaterials() を使う。Unity の renderer.materials と同じく、
	//  アセットを入れている枠はこのレンダラー専用の写しに切り替わるので、アセットのファイルは変わらない。
	//  アセットそのものを変えたいときは GetSharedMaterial() を使う（Unity の sharedMaterial）。
	class MeshRenderer final : public Component
	{
		RD_COMPONENT(MeshRenderer)
	public:
		std::string modelPath;
		Color tint = Color::kWhite;

		void Reflect(PropertyVisitor& v) override;
		void OnValidate() override;
		void OnRender(RenderList& list) override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif

		void SetModel(const std::string& path);
		const AssetRef<Model>& GetModel() const { return mModel; }

		// マテリアルの枠の数（モデルを読み終えるまでは、ファイルから読んだ数）
		size_t GetMaterialCount() const { return mMaterials.size(); }
		// このレンダラー専用のマテリアル（Unity の renderer.materials）。アセットを入れている枠は写しに切り替える
		std::vector<Material>& GetMaterials();
		// 枠に入れたマテリアルのアセット（なければ空 / nullptr）
		const std::string& GetSharedMaterialPath(size_t index) const;
		MaterialAsset* GetSharedMaterial(size_t index) const;
		// 枠にマテリアルのアセットを入れる（空のパスで外す。外すとアセットの値を写したこのレンダラー専用のものになる）
		bool SetSharedMaterial(size_t index, const std::string& path);
		// 描くときに使うマテリアル（アセットがあればその値、なければこのレンダラー専用のもの）
		const Material& GetRenderMaterial(size_t index) const;

		AABB GetWorldBounds() const;
		// ワールド空間のレイと三角形で当たりを調べる（エディタでの選択用）
		//  outSubMesh には当たった三角形のサブメッシュの番号が入る（箱で代わりにしたときは -1）
		bool Raycast(const Ray& ray, float& outDistance, int* outSubMesh = nullptr) const;

	private:
		void SyncMaterials();
		// 枠の数をそろえる（増えた枠は空）
		void ResizeSlots(size_t count);
		void MatchSlotArrays();
		void ResolveSharedMaterial(size_t index);
		D3D12_GPU_VIRTUAL_ADDRESS GetPalette();

		AssetRef<Model> mModel;
		// 枠ごとの、このレンダラー専用のマテリアル / アセットのパス / 読み込んだアセット（3 つは同じ数）
		std::vector<Material> mMaterials;
		std::vector<std::string> mMaterialPaths;
		std::vector<AssetRef<MaterialAsset>> mMaterialAssets;
		const Model* mSyncedModel = nullptr;
		bool mResetMaterials = false;
		std::vector<Matrix4> mBindPalette;
	};
}
