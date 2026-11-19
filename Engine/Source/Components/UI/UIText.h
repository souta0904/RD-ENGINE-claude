#pragma once
#include "Components/UI/UIDraw.h"
#include "Scene/Component/Component.h"
#include <string>

namespace rd
{
	// UI の文字（Unity の TextMeshPro の一部）
	//  長方形の中で折り返し、Alignment（TopLeft / Center / BottomRight など）にそろえて描く。
	//  Auto Size なら入りきるまで小さくする（Font Size が上限）。
	//  リッチテキストは <color=#RRGGBB>、<size=70%>、<sprite name="x">（ボタンの名前を色付きの文字で出す）が使える
	class UIText final : public Component
	{
		RD_COMPONENT(UIText)
	public:
		std::string text;
		std::string font; // 空なら UIFont の既定のフォント
		float fontSize = 36.0f;
		Color color = Color::kWhite; // sRGB
		std::string alignment = "Center";
		bool autoSize = false;
		float lineSpacing = 0.0f; // 行の間隔に足す量（em の 1/100 単位。TextMeshPro と同じ）
		float outlineWidth = 0.0f; // 縁取りの太さ（TextMeshPro と同じ 0〜1。0 なら縁取りなし）
		Color outlineColor = Color::kBlack; // sRGB

		void Reflect(PropertyVisitor& v) override;
		void Draw(const UIRect& rect, const UIDrawContext& ctx);
		// 折り返さずに置いたときの大きさ（Unity のレイアウトの preferred size）
		Vector2 GetPreferredSize() const;
	};
}
