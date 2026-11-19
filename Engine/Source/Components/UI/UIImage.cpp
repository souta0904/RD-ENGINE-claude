#include "RdPch.h"
#include "Components/UI/UIImage.h"
#include "Asset/AssetManager.h"
#include "Asset/Texture/Texture.h"

namespace rd
{
	void UIImage::Reflect(PropertyVisitor& v)
	{
		v.Property("Sprite", sprite, { .flags = PropertyMeta::AssetTexture });
		v.Property("UV Rect", uvRect, { .speed = 0.01f });
		v.Property("Color", color);
		v.Property("Image Type", imageType);
		v.Property("Fill Method", fillMethod);
		v.Property("Fill Origin", fillOrigin);
		v.Property("Fill Amount", fillAmount, { .speed = 0.01f, .min = 0.0f, .max = 1.0f });
		v.Property("Preserve Aspect", preserveAspect);
		v.Property("Sprite Size", spriteSize, { .speed = 1.0f, .min = 0.0f, .max = 16384.0f });
		v.Property("Padding", padding, { .speed = 1.0f, .min = 0.0f, .max = 16384.0f });
		v.Property("Border", border, { .speed = 1.0f, .min = 0.0f, .max = 16384.0f });
		v.Property("Inner UV", innerUv, { .speed = 0.01f });
		v.Property("Pixels Per Unit", pixelsPerUnit, { .speed = 0.01f, .min = 0.01f, .max = 100.0f });
		v.Property("Fill Center", fillCenter);
	}

	// Unity の Image.GetDrawingDimensions。縦横比を保ち、透明な余白の分だけ内側に縮める
	//  Sprite Size がない（Unity から来ていない）ときは、テクスチャの UV Rect の範囲を大きさにして余白はなしとする
	UIRect UIImage::GetDrawingRect(const UIRect& rectIn, const Texture& texture, bool keepAspect) const
	{
		UIRect rect = rectIn;
		bool hasSize = spriteSize.x > 0.0f && spriteSize.y > 0.0f;
		Vector2 size = hasSize ? spriteSize
			: Vector2{ float(texture.GetWidth()) * std::fabs(uvRect.z - uvRect.x), float(texture.GetHeight()) * std::fabs(uvRect.w - uvRect.y) };
		if (size.x <= 0.0f || size.y <= 0.0f) { return rect; }

		if (keepAspect)
		{
			float spriteAspect = size.x / size.y;
			float rectAspect = rect.Width() / std::max(rect.Height(), 1e-4f);
			Vector2 center = (rect.min + rect.max) * 0.5f;
			Vector2 half = rect.Size() * 0.5f;
			if (spriteAspect > rectAspect) { half.y = half.x / spriteAspect; }
			else { half.x = half.y * spriteAspect; }
			rect = { center - half, center + half };
		}
		if (!hasSize) { return rect; }

		Vector2 rs = rect.Size();
		Vector2 min = rect.min;
		return {
			{ min.x + rs.x * (padding.x / size.x), min.y + rs.y * (padding.y / size.y) },
			{ min.x + rs.x * ((size.x - padding.z) / size.x), min.y + rs.y * ((size.y - padding.w) / size.y) } };
	}

	// Unity の Image.GenerateSlicedSprite と同じく 3×3 に分けて描く。四隅は伸ばさず、辺と中央だけ伸ばす
	void UIImage::DrawSliced(const UIRect& rect, const Texture& texture, const Color& tint, const UIDrawContext& ctx) const
	{
		float ppu = std::max(pixelsPerUnit, 1e-4f);
		// 枠の大きさ（Canvas の単位）。左・下・右・上
		float left = border.x / ppu, bottom = border.y / ppu, right = border.z / ppu, top = border.w / ppu;
		// 長方形が枠の合計より小さいときは、枠を同じ比率で縮める（Unity の GetAdjustedBorders）
		float w = rect.Width(), h = rect.Height();
		if (left + right > w && left + right > 0.0f) { float s = w / (left + right); left *= s; right *= s; }
		if (bottom + top > h && bottom + top > 0.0f) { float s = h / (bottom + top); bottom *= s; top *= s; }

		// 外側は透明な余白を除いた端、内側は枠の位置。UV の v は上が小さいので、下から上へ並べると w→y になる
		const float xs[4] = { rect.min.x + padding.x / ppu, rect.min.x + left, rect.max.x - right, rect.max.x - padding.z / ppu };
		const float ys[4] = { rect.min.y + padding.y / ppu, rect.min.y + bottom, rect.max.y - top, rect.max.y - padding.w / ppu };
		const float us[4] = { uvRect.x, innerUv.x, innerUv.z, uvRect.z };
		const float vs[4] = { uvRect.w, innerUv.w, innerUv.y, uvRect.y };

		for (int y = 0; y < 3; ++y)
		{
			for (int x = 0; x < 3; ++x)
			{
				if (x == 1 && y == 1 && !fillCenter) { continue; }
				UIRect cell{ { xs[x], ys[y] }, { xs[x + 1], ys[y + 1] } };
				if (cell.Width() <= 1e-5f || cell.Height() <= 1e-5f) { continue; }
				ctx.DrawQuad(cell, &texture, { us[x], vs[y + 1], us[x + 1], vs[y] }, tint);
			}
		}
	}

	void UIImage::Draw(const UIRect& rectIn, const UIDrawContext& ctx)
	{
		const Texture* texture = nullptr;
		if (!sprite.empty())
		{
			if (!mTexture || mTexture->GetPath() != sprite) { mTexture = AssetManager::Get().LoadTexture(sprite); }
			// 読み込みが終わるまでは白い四角にならないよう描かない
			if (!mTexture->IsReady()) { return; }
			texture = mTexture.get();
		}

		Color c = UIColorToLinear(color);
		c.a *= ctx.alpha;

		bool hasBorder = border.x > 0.0f || border.y > 0.0f || border.z > 0.0f || border.w > 0.0f;
		if ((imageType == ImageType::Sliced || imageType == ImageType::Tiled) && texture && hasBorder)
		{
			DrawSliced(rectIn, *texture, c, ctx);
			return;
		}

		UIRect rect = texture ? GetDrawingRect(rectIn, *texture, preserveAspect) : rectIn;
		Vector4 uv = uvRect;
		if (imageType == ImageType::Filled)
		{
			float amount = math::Saturate(fillAmount);
			if (amount <= 0.0f) { return; }
			if (fillMethod == FillMethod::Horizontal)
			{
				float fw = rect.Width() * amount, du = (uv.z - uv.x) * amount;
				if (fillOrigin == 0) { rect.max.x = rect.min.x + fw; uv.z = uv.x + du; }
				else { rect.min.x = rect.max.x - fw; uv.x = uv.z - du; }
			}
			else if (fillMethod == FillMethod::Vertical)
			{
				// テクスチャの V は上が小さいので、下から伸ばすときは uv.w の側を残す
				float fh = rect.Height() * amount, dv = (uv.w - uv.y) * amount;
				if (fillOrigin == 0) { rect.max.y = rect.min.y + fh; uv.y = uv.w - dv; }
				else { rect.min.y = rect.max.y - fh; uv.w = uv.y + dv; }
			}
			// 円形の塗り（Radial）はまだないので全体を描く
		}
		ctx.DrawQuad(rect, texture, uv, c);
	}
}
