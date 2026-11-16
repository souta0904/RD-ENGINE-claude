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

	// モデルを描く（スキンメッシュは同じアクターの Animator の姿勢を使い、なければバインドポーズ）
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
		// ワールド空間のレイと三角形で当たりを調べる（エディタでの選択用）
		bool Raycast(const Ray& ray, float& outDistance) const;

	private:
		void SyncMaterials();
		D3D12_GPU_VIRTUAL_ADDRESS GetPalette();

		AssetRef<Model> mModel;
		std::vector<Material> mMaterials;
		const Model* mSyncedModel = nullptr;
		bool mResetMaterials = false;
		std::vector<Matrix4> mBindPalette;
	};
}
