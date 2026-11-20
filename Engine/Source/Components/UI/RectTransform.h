#pragma once
#include "Components/UI/UIDraw.h"
#include "Scene/Component/Component.h"

namespace rd
{
	// UI の要素の位置と大きさ（Unity の RectTransform と同じ決め方）
	//  親の長方形の中の Anchor Min～Max の範囲に、Size Delta を足した大きさで、
	//  Pivot の位置がアンカーの基準点から Anchored Position だけずれた所に置く。
	//  拡大と回転は Transform の Scale と Rotation の Z を使う（Position は使わない）
	class RD_API RectTransform final : public Component
	{
		RD_COMPONENT(RectTransform)
	public:
		Vector2 anchorMin = { 0.5f, 0.5f };
		Vector2 anchorMax = { 0.5f, 0.5f };
		Vector2 pivot = { 0.5f, 0.5f };
		Vector2 anchoredPosition;
		Vector2 sizeDelta = { 100.0f, 100.0f };

		void Reflect(PropertyVisitor& v) override;

		// 親の長方形の中での自分の長方形
		UIRect ComputeRect(const UIRect& parent) const;
		// 最後に描いたときの長方形（キャンバスの座標）。描かれていなければ大きさ 0
		const UIRect& GetLastRect() const { return mLastRect; }

	private:
		friend class Canvas;
		UIRect mLastRect;
	};
}
