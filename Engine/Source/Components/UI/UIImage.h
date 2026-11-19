#pragma once
#include "Asset/Asset.h"
#include "Components/UI/UIDraw.h"
#include "Scene/Component/Component.h"
#include <string>

namespace rd
{
	class Texture;

	// UI の画像（Unity の Image と RawImage）。画像がなければ色だけの長方形
	//  Image Type が Filled で Fill Method が Horizontal / Vertical なら Fill Amount の分だけ描く（ゲージ）。
	//  Sliced は 9 分割し、四隅は伸ばさず辺と中央だけ伸ばす（Tiled も同じに描く。繰り返しはまだない）。
	//  Unity と同じく UV Rect は透明な余白を除いた外側、Inner UV は枠の内側。Padding・Border はスプライトのピクセル
	//  （左・下・右・上）で、Canvas の単位にすると ÷ Pixels Per Unit
	class UIImage final : public Component
	{
		RD_COMPONENT(UIImage)
	public:
		enum class ImageType { Simple, Sliced, Tiled, Filled };
		enum class FillMethod { Horizontal, Vertical, Radial90, Radial180, Radial360 };

		std::string sprite;
		Vector4 uvRect = { 0.0f, 0.0f, 1.0f, 1.0f };
		Color color = Color::kWhite; // sRGB
		ImageType imageType = ImageType::Simple;
		FillMethod fillMethod = FillMethod::Horizontal;
		int32_t fillOrigin = 0;
		float fillAmount = 1.0f;
		bool preserveAspect = false;
		Vector2 spriteSize = { 0.0f, 0.0f };          // 余白を含むスプライトの大きさ（ピクセル）。0 なら余白なし
		Vector4 padding = { 0.0f, 0.0f, 0.0f, 0.0f }; // 除いた透明な余白
		Vector4 border = { 0.0f, 0.0f, 0.0f, 0.0f };  // 9 分割の枠
		Vector4 innerUv = { 0.0f, 0.0f, 1.0f, 1.0f };
		float pixelsPerUnit = 1.0f;
		bool fillCenter = true;

		void Reflect(PropertyVisitor& v) override;
		void Draw(const UIRect& rect, const UIDrawContext& ctx);

	private:
		UIRect GetDrawingRect(const UIRect& rect, const Texture& texture, bool keepAspect) const;
		void DrawSliced(const UIRect& rect, const Texture& texture, const Color& tint, const UIDrawContext& ctx) const;

		AssetRef<Texture> mTexture;
	};
}
