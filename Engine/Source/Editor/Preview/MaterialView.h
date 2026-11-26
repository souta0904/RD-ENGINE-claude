#pragma once
#include "Editor/Preview/PreviewView.h"
#include <string>

namespace rd
{
	class MaterialAsset;
	class Model;

	// マテリアル（.rdmat）を球に塗って描く（Unity のマテリアルのプレビューと同じ）
	//  値を変えると GetVersion が増えるので、サムネイルはそれを見て描き直す
	class RD_API MaterialView final : public PreviewView
	{
	public:
		void SetMaterial(const std::string& path);
		bool IsReady() const override;
		bool IsFullyLoaded() override;
		bool Render(uint32_t width, uint32_t height) override;
		bool NeedsRefresh() const override;

	private:
		AssetRef<Model> mSphere;
		AssetRef<MaterialAsset> mAsset;
		std::string mPath;
		uint32_t mRenderedVersion = ~0u;
	};
}
