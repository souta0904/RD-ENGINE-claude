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
		static constexpr ImVec4 kAccent2Dim{ 0.25f, 0.29f, 0.43f, 1.0f };
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
		// 選んでいる行の背景: 左から右へ薄れていく差し色と、左端の縦グラデーションの細い帯。
		//  reveal（0〜1）で左から伸びていく途中を描く（選んだ直後の動き）
		static void DrawSelection(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float reveal = 1.0f);
		// 折りたたみの見出しの背景: 面の色に、左から差し色をうっすら重ねる。
		//  open（0〜1）で左端の帯が伸び縮みし、hover（0〜1）でマウスを乗せたときの明るさがふわっと変わる
		static void DrawHeaderBackground(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float open, float hover);

		// ---- 動き ----
		//  ImGui は毎フレーム描き直すので、動きの途中の値は今のウィンドウの記憶（StateStorage）に key ごとに残す
		// 値を target へなめらかに近づけて返す（speed が大きいほど速い。初めて呼ばれたときは target から始める）
		static float Animate(ImGuiID key, float target, float speed = 16.0f);
		// 前のフレームでマウスが乗っていたかを元に、0（乗っていない）〜 1（乗っている）をなめらかに動かす
		static float HoverFade(ImGuiID key, float speed = 18.0f);
		// 今のフレームでマウスが乗っているかを残す（次のフレームの HoverFade が使う）
		static void SetHovered(ImGuiID key, bool hovered);
		// 上から下への縦のグラデーション（ツールバーの帯などに、わずかな立体感を付ける）
		static void DrawVerticalShade(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ImVec4& top, const ImVec4& bottom);
		// 文字を左から右への 2 色のグラデーションで描く
		static void DrawGradientText(ImDrawList* dl, const ImVec2& pos, const char* text, const ImVec4& left, const ImVec4& right);
		// メニューバーの左端のロゴ（ひし形のマークと名前）。メニューバーの中で呼ぶ
		static void DrawLogo();
		// OS のタイトルバーを暗い色にしてエディタとつなげる（Windows 11 で効く。古い Windows では何もしない）
		static void ApplyDarkTitleBar(void* hwnd);
	};
}
