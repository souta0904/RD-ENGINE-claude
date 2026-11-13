#pragma once
#include "Asset/Asset.h"
#include "Render/RenderTypes.h"
#include "Scene/Component.h"
#include <vector>

namespace rd
{
	class Model;

	// モデルを描画する（スキンメッシュの場合は同じアクターの Animator の姿勢を使う）
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
		std::vector<Material>& GetMaterials() { return mMaterials; }
		AABB GetWorldBounds() const;
		// ワールド空間のレイと三角形で交差判定（エディタのピッキング用）
		bool Raycast(const Ray& ray, float& outDistance) const;

	private:
		void SyncMaterials();
		D3D12_GPU_VIRTUAL_ADDRESS GetPalette();

		AssetRef<Model> mModel;
		std::vector<Material> mMaterials;
		const Model* mSyncedModel = nullptr;
		bool mResetMaterials = false;
		// アニメーターがないスキンメッシュ用のバインドポーズ
		std::vector<Matrix4> mBindPalette;
	};
}
