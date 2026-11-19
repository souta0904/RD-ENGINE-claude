#pragma once
#include "Math/Math.h"
#include <cstdint>

namespace rd
{
	class RenderList;
	class Texture;

	// UI の座標は Unity の uGUI と同じく、キャンバスの左下が原点で上が +Y（単位はキャンバスの大きさ）
	struct UIRect
	{
		Vector2 min;
		Vector2 max;
		float Width() const { return max.x - min.x; }
		float Height() const { return max.y - min.y; }
		Vector2 Size() const { return max - min; }
		Vector2 Lerp(const Vector2& t) const { return { min.x + Width() * t.x, min.y + Height() * t.y }; }
	};

	// 2D のアフィン変換（拡大・回転・移動）。UI の親子の Transform の Scale と Rotation の Z を重ねる
	struct UIAffine
	{
		float a = 1.0f, b = 0.0f, c = 0.0f, d = 1.0f, tx = 0.0f, ty = 0.0f;

		Vector2 Apply(const Vector2& p) const { return { a * p.x + c * p.y + tx, b * p.x + d * p.y + ty }; }
		UIAffine operator*(const UIAffine& o) const
		{
			return { a * o.a + c * o.b, b * o.a + d * o.b, a * o.c + c * o.d, b * o.c + d * o.d, a * o.tx + c * o.ty + tx, b * o.tx + d * o.ty + ty };
		}
		// 点 pivot を中心に拡大と回転をする
		static UIAffine AroundPivot(const Vector2& pivot, const Vector2& scale, float radians)
		{
			float cs = std::cos(radians), sn = std::sin(radians);
			UIAffine m{ cs * scale.x, sn * scale.x, -sn * scale.y, cs * scale.y, 0.0f, 0.0f };
			Vector2 moved = m.Apply(pivot);
			m.tx = pivot.x - moved.x;
			m.ty = pivot.y - moved.y;
			return m;
		}
	};

	// Canvas が子を辿りながら描くときの情報
	struct UIDrawContext
	{
		RenderList* list = nullptr;
		float screenScale = 1.0f;  // キャンバスの 1 単位が何ピクセルか
		float canvasHeight = 0.0f; // キャンバスの高さ（上下を画面の向きに直すのに使う）
		int32_t layer = 0;

		// 今の要素の変換と、親から重ねた不透明度
		UIAffine transform;
		float alpha = 1.0f;

		// 長方形を 1 枚描く（color はリニア。alpha は掛けてから渡す）
		void DrawQuad(const UIRect& rect, const Texture* texture, const Vector4& uv, const Color& color) const;
	};

	// Unity の UI の色は sRGB で書かれているので、描く前にリニアへ直す
	Color UIColorToLinear(const Color& srgb);
}
