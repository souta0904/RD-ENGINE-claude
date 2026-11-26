#pragma once
#include "Asset/Animation/Skeleton.h"
#include "Editor/Preview/PreviewView.h"
#include "Render/Material/Material.h"
#include <string>
#include <vector>

namespace rd
{
	class Model;

	// モデル 1 つだけを描く（マテリアルはモデルの既定値）。スキンメッシュはクリップを選ぶと動く
	class RD_API ModelView final : public PreviewView
	{
	public:
		// -1 ならバインドポーズ
		int clip = -1;
		float time = 0.0f;

		void SetModel(const std::string& path);
		const std::string& GetPath() const { return mPath; }
		Model* GetModel() const { return mModel.get(); }
		bool IsReady() const override;
		bool IsFullyLoaded() override;
		bool Render(uint32_t width, uint32_t height) override;

	private:
		// モデルの既定のマテリアルから、描く用のマテリアルを作る（モデルが変わったときだけ）
		void SyncMaterials();
		// 骨の行列を GPU へ送り、そのアドレスを返す
		D3D12_GPU_VIRTUAL_ADDRESS BuildPalette();

		AssetRef<Model> mModel;
		std::string mPath;
		std::vector<Material> mMaterials;
		const Model* mSynced = nullptr;
		std::vector<TransformTRS> mLocal;
		std::vector<Matrix4> mGlobal;
		std::vector<Matrix4> mPalette;
		std::vector<int32_t> mBinding;
		int mBindingClip = -2;
	};
}
