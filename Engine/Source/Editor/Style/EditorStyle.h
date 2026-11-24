#pragma once
#include <ImGui/imgui.h>

namespace rd
{
	// エディタの配色と寸法（すべてのパネルがここの値を使って見た目をそろえる）
	//  Apply で ImGui のスタイルに反映する。プレイ中はメニューバーの色を変えて分かるようにする
	class RD_API EditorStyle
	{
	public:
		static void Apply(float dpiScale);
		static void SetPlayMode(bool playing, bool paused);

		// 面の明るさは 3 段: 一番奥（kBaseBg: メニュー・パネルのすき間）→ パネル（kWindowBg）→ 入力欄（kFieldBg: へこんで見えるよう暗く）。
		//  テーマは「落ち着いた青緑」: 灰色はほぼ無彩色（ごくわずかに青み）、差し色はくすんだ青緑（kAccent）。
		//  目立たせたい所（選ばれたタブ・ロゴ）は青緑からくすんだ青（kAccent2）へのゆるいグラデーションにする
		static constexpr ImVec4 kAccent{ 0.47f, 0.69f, 0.74f, 1.0f };
		static constexpr ImVec4 kAccent2{ 0.50f, 0.58f, 0.78f, 1.0f };
		static constexpr ImVec4 kAccentHover{ 0.58f, 0.78f, 0.83f, 1.0f };
		static constexpr ImVec4 kAccentDim{ 0.24f, 0.37f, 0.41f, 1.0f };
		static constexpr ImVec4 kBaseBg{ 0.071f, 0.075f, 0.080f, 1.0f };
		static constexpr ImVec4 kWindowBg{ 0.114f, 0.118f, 0.125f, 1.0f };
		static constexpr ImVec4 kToolbarBg{ 0.094f, 0.098f, 0.104f, 1.0f };
		static constexpr ImVec4 kFieldBg{ 0.078f, 0.082f, 0.088f, 1.0f };
		static constexpr ImVec4 kHeaderBg{ 0.165f, 0.172f, 0.182f, 1.0f };
		static constexpr ImVec4 kHover{ 0.204f, 0.212f, 0.224f, 1.0f };
		static constexpr ImVec4 kSelection{ 0.47f, 0.69f, 0.74f, 0.22f };
		static constexpr ImVec4 kRowAlt{ 1.0f, 1.0f, 1.0f, 0.012f };
		static constexpr ImVec4 kText{ 0.89f, 0.90f, 0.91f, 1.0f };
		static constexpr ImVec4 kTextDim{ 0.54f, 0.57f, 0.61f, 1.0f };
		static constexpr ImVec4 kInfo{ 0.80f, 0.82f, 0.86f, 1.0f };
		static constexpr ImVec4 kWarning{ 1.00f, 0.77f, 0.33f, 1.0f };
		static constexpr ImVec4 kError{ 1.00f, 0.45f, 0.45f, 1.0f };
		static constexpr ImVec4 kSuccess{ 0.36f, 0.84f, 0.55f, 1.0f };
		static constexpr ImVec4 kPlay{ 0.30f, 0.80f, 0.50f, 1.0f };
		static constexpr ImVec4 kPlayTint{ 0.09f, 0.20f, 0.14f, 1.0f };
		static constexpr ImVec4 kPauseTint{ 0.24f, 0.18f, 0.07f, 1.0f };

		// アイコンの色（種類ごとに決めて、どのパネルでも同じ色にする）
		static constexpr ImVec4 kIconNeutral{ 0.72f, 0.74f, 0.78f, 1.0f };
		static constexpr ImVec4 kIconMesh{ 0.55f, 0.72f, 1.00f, 1.0f };
		static constexpr ImVec4 kIconLight{ 1.00f, 0.84f, 0.30f, 1.0f };
		static constexpr ImVec4 kIconCamera{ 0.62f, 0.86f, 0.62f, 1.0f };
		static constexpr ImVec4 kIconEffect{ 1.00f, 0.60f, 0.30f, 1.0f };
		static constexpr ImVec4 kIconPhysics{ 0.45f, 0.90f, 0.55f, 1.0f };
		static constexpr ImVec4 kIconAudio{ 0.85f, 0.55f, 0.95f, 1.0f };
		static constexpr ImVec4 kIconFolder{ 0.90f, 0.74f, 0.38f, 1.0f };

		static float ToolbarHeight();
		static float IconSize();

		// 選ばれているドッキングのタブの上に差し色の線を引く（パネルの Begin の直後に呼ぶ）。
		//  フォーカスがあるパネルは差し色、ないパネルは控えめな色にして、どこを操作しているか分かるようにする
		static void DrawTabHighlight();
		// 横向きのグラデーションの帯（すみれ色 → 水色）。選ばれたタブの線やロゴに使う
		static void DrawAccentGradient(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float alpha = 1.0f);
		// メニューバーの左端のロゴ（ひし形のマークと名前）。メニューバーの中で呼ぶ
		static void DrawLogo();
		// OS のタイトルバーを暗い色にしてエディタとつなげる（Windows 11 で効く。古い Windows では何もしない）
		static void ApplyDarkTitleBar(void* hwnd);
	};
}
