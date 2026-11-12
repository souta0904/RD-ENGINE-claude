#pragma once
#include "Asset/Asset.h"
#include "Math/Math.h"
#include "Scene/Component.h"

namespace rd
{
	class Texture;

	// 2D スプライト（画面座標で描く UI 向け）
	//  Transform の Position.xy がスクリーン座標（左上原点、ピクセル）、Rotation の Z が回転
	class SpriteRenderer final : public Component
	{
		RD_COMPONENT(SpriteRenderer)
	public:
		std::string texturePath;
		Vector2 size = { 128.0f, 128.0f };
		Vector2 pivot = { 0.5f, 0.5f };
		Color color = Color::kWhite;
		Vector4 uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
		int32_t layer = 0;
		bool flipX = false;
		bool flipY = false;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;

	private:
		AssetRef<Texture> mTexture;
	};
}
