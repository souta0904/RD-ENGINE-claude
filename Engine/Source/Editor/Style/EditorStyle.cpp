#include "RdPch.h"
#include "Editor/Style/EditorStyle.h"
#include <ImGui/imgui_internal.h>
#include <dwmapi.h>
#include <unordered_map>
#pragma comment(lib, "dwmapi.lib")

namespace rd
{
	namespace
	{
		// 色に透明度だけ付け替えた色を作る（差し色を薄く重ねるときに使う）
		constexpr ImVec4 WithAlpha(const ImVec4& c, float a) { return ImVec4(c.x, c.y, c.z, a); }
	}

	void EditorStyle::Apply(float dpiScale)
	{
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();

		// 角の丸み: すべてなし（直角のカクカクした見た目にそろえる）
		style.WindowRounding = 0.0f;
		style.ChildRounding = 0.0f;
		style.FrameRounding = 0.0f;
		style.PopupRounding = 0.0f;
		style.GrabRounding = 0.0f;
		style.TabRounding = 0.0f;
		style.ScrollbarRounding = 0.0f;

		// 線: 枠（フチ）はどこにも付けず、面の明るさの違いだけで区切る
		style.WindowBorderSize = 0.0f;
		style.ChildBorderSize = 0.0f;
		style.FrameBorderSize = 0.0f;
		style.PopupBorderSize = 0.0f;
		style.TabBorderSize = 0.0f;
		style.TabBarBorderSize = 0.0f;
		style.DockingSeparatorSize = 3.0f;
		style.SeparatorTextBorderSize = 1.0f;
		style.SeparatorTextPadding = ImVec2(10.0f, 4.0f);

		// 余白: 詰めすぎず、行の高さに少しゆとりを持たせる
		style.WindowPadding = ImVec2(10.0f, 8.0f);
		style.FramePadding = ImVec2(8.0f, 4.0f);
		style.CellPadding = ImVec2(6.0f, 3.0f);
		style.ItemSpacing = ImVec2(8.0f, 5.0f);
		style.ItemInnerSpacing = ImVec2(6.0f, 4.0f);
		style.IndentSpacing = 16.0f;
		style.ScrollbarSize = 11.0f;
		style.GrabMinSize = 10.0f;
		style.WindowMenuButtonPosition = ImGuiDir_None;
		style.WindowTitleAlign = ImVec2(0.0f, 0.5f);

		ImVec4* c = style.Colors;
		c[ImGuiCol_Text] = kText;
		c[ImGuiCol_TextDisabled] = kTextDim;
		c[ImGuiCol_WindowBg] = kWindowBg;
		c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_PopupBg] = ImVec4(0.125f, 0.129f, 0.137f, 0.97f);
		c[ImGuiCol_Border] = ImVec4(1.0f, 1.0f, 1.0f, 0.07f);
		c[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);

		c[ImGuiCol_FrameBg] = kFieldBg;
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.098f, 0.102f, 0.110f, 1.0f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.118f, 0.122f, 0.131f, 1.0f);

		c[ImGuiCol_TitleBg] = kBaseBg;
		c[ImGuiCol_TitleBgActive] = kBaseBg;
		c[ImGuiCol_TitleBgCollapsed] = kBaseBg;
		c[ImGuiCol_MenuBarBg] = kBaseBg;

		c[ImGuiCol_Header] = kHeaderBg;
		c[ImGuiCol_HeaderHovered] = kHover;
		c[ImGuiCol_HeaderActive] = kSelection;

		c[ImGuiCol_Button] = ImVec4(0.157f, 0.164f, 0.174f, 1.0f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.208f, 0.216f, 0.229f, 1.0f);
		c[ImGuiCol_ButtonActive] = kAccentDim;

		c[ImGuiCol_CheckMark] = kAccent;
		c[ImGuiCol_SliderGrab] = WithAlpha(kAccent, 0.85f);
		c[ImGuiCol_SliderGrabActive] = kAccent;

		c[ImGuiCol_Separator] = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
		c[ImGuiCol_SeparatorHovered] = kAccentDim;
		c[ImGuiCol_SeparatorActive] = kAccent;
		c[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_ResizeGripHovered] = WithAlpha(kAccent, 0.4f);
		c[ImGuiCol_ResizeGripActive] = kAccent;

		c[ImGuiCol_ScrollbarBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_ScrollbarGrab] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
		c[ImGuiCol_ScrollbarGrabHovered] = ImVec4(1.0f, 1.0f, 1.0f, 0.18f);
		c[ImGuiCol_ScrollbarGrabActive] = WithAlpha(kAccent, 0.7f);

		// タブ: 選ばれているタブはパネルと同じ色にしてつなげ、選ばれていないタブは奥の色に沈める
		c[ImGuiCol_Tab] = kBaseBg;
		c[ImGuiCol_TabHovered] = kHover;
		c[ImGuiCol_TabActive] = kWindowBg;
		c[ImGuiCol_TabUnfocused] = kBaseBg;
		c[ImGuiCol_TabUnfocusedActive] = kWindowBg;

		c[ImGuiCol_DockingPreview] = WithAlpha(kAccent, 0.45f);
		c[ImGuiCol_DockingEmptyBg] = kBaseBg;

		c[ImGuiCol_PlotLines] = kAccent;
		c[ImGuiCol_PlotLinesHovered] = kWarning;
		c[ImGuiCol_PlotHistogram] = kAccent;
		c[ImGuiCol_PlotHistogramHovered] = kWarning;

		c[ImGuiCol_TableHeaderBg] = kHeaderBg;
		c[ImGuiCol_TableBorderStrong] = ImVec4(1.0f, 1.0f, 1.0f, 0.08f);
		c[ImGuiCol_TableBorderLight] = ImVec4(1.0f, 1.0f, 1.0f, 0.05f);
		c[ImGuiCol_TableRowBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_TableRowBgAlt] = kRowAlt;

		c[ImGuiCol_DragDropTarget] = kAccent;
		c[ImGuiCol_NavHighlight] = kAccent;
		c[ImGuiCol_TextSelectedBg] = WithAlpha(kAccent, 0.30f);
		c[ImGuiCol_ModalWindowDimBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.55f);

		style.ScaleAllSizes(dpiScale);
	}

	// プレイ中・一時停止中はメニューバーに色をのせて、編集中でないことがひと目で分かるようにする
	void EditorStyle::SetPlayMode(bool playing, bool paused)
	{
		ImVec4 bar = kBaseBg;
		if (playing) { bar = paused ? kPauseTint : kPlayTint; }
		ImGui::GetStyle().Colors[ImGuiCol_MenuBarBg] = bar;
	}

	float EditorStyle::ToolbarHeight()
	{
		return ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
	}

	float EditorStyle::IconSize()
	{
		return ImGui::GetFontSize();
	}

	// タブの位置はタブバーの左端 + タブのずれ - スクロール量（ImGui が描くときと同じ計算）。
	//  タブバーは親（ドッキングの土台）のウィンドウが描くが、線はこのパネルの描画リストに描く
	//  （パネルは土台より後に描かれるので、線がタブの上に重なる）。パネルの外なので切り抜きを広げてから描く
	void EditorStyle::DrawTabHighlight()
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		if (!window->DockIsActive || !window->DockNode) { return; }
		ImGuiDockNode* node = window->DockNode;
		ImGuiTabBar* tabBar = node->TabBar;
		if (!tabBar || node->IsHiddenTabBar() || node->IsNoTabBar()) { return; }
		if (tabBar->SelectedTabId != window->TabId) { return; }
		ImGuiTabItem* tab = ImGui::TabBarFindTabByID(tabBar, window->TabId);
		if (!tab) { return; }

		float targetX0 = tabBar->BarRect.Min.x + tab->Offset - tabBar->ScrollingAnim;
		float targetX1 = targetX0 + tab->Width;
		float y = tabBar->BarRect.Min.y;
		float thickness = IM_ROUND(2.0f * ImGui::GetFontSize() / 15.0f);

		// 線はタブバーごとに位置を覚え、選んだタブへ滑っていく。フォーカスの有無は色をふわっと切り替える。
		//  タブを切り替えると線を描くパネルも替わるので、パネルの記憶ではなくタブバーの ID で覚える
		struct Line { float x0, x1, focus; int frame; };
		static std::unordered_map<ImGuiID, Line> sLines;
		int frame = ImGui::GetFrameCount();
		float targetFocus = node->IsFocused ? 1.0f : 0.0f;
		auto [it, added] = sLines.try_emplace(tabBar->ID, Line{ targetX0, targetX1, targetFocus, frame });
		Line& line = it->second;
		// しばらく描かれていなかった（隠れていた）タブバーは、滑らせずに今の位置から始める
		if (frame - line.frame > 2) { line = { targetX0, targetX1, targetFocus, frame }; }
		float k = 1.0f - std::exp(-20.0f * ImGui::GetIO().DeltaTime);
		line.x0 += (targetX0 - line.x0) * k;
		line.x1 += (targetX1 - line.x1) * k;
		line.focus += (targetFocus - line.focus) * (1.0f - std::exp(-14.0f * ImGui::GetIO().DeltaTime));
		line.frame = frame;
		float x0 = IM_ROUND(line.x0), x1 = IM_ROUND(line.x1), f = line.focus;

		ImDrawList* dl = window->DrawList;
		dl->PushClipRect(tabBar->BarRect.Min, tabBar->BarRect.Max, false);
		// フォーカスがないときの灰色の線の上に、フォーカスの分だけグラデーションの線と、その下の淡い光を重ねる
		dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + thickness), ImGui::ColorConvertFloat4ToU32(WithAlpha(kTextDim, 0.35f * (1.0f - f))));
		if (f > 0.01f)
		{
			DrawAccentGradient(dl, ImVec2(x0, y), ImVec2(x1, y + thickness), f);
			ImU32 glow = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent, 0.10f * f));
			ImU32 clear = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent, 0.0f));
			dl->AddRectFilledMultiColor(ImVec2(x0, y + thickness), ImVec2(x1, tabBar->BarRect.Max.y), glow, glow, clear, clear);
		}
		dl->PopClipRect();
	}

	void EditorStyle::DrawAccentGradient(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float alpha)
	{
		ImU32 left = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent, alpha));
		ImU32 right = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent2, alpha));
		dl->AddRectFilledMultiColor(min, max, left, right, right, left);
	}

	namespace
	{
		ImU32 U32(const ImVec4& c, float a) { return ImGui::ColorConvertFloat4ToU32(WithAlpha(c, a)); }

		// 左端の縦の帯（上が青緑、下がくすんだ青）
		void DrawSideBar(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float alpha)
		{
			float bar = std::max(2.0f, IM_ROUND(ImGui::GetFontSize() * 0.18f));
			ImU32 top = U32(EditorStyle::kAccent, alpha), bottom = U32(EditorStyle::kAccent2, alpha);
			dl->AddRectFilledMultiColor(min, ImVec2(min.x + bar, max.y), top, top, bottom, bottom);
		}
	}

	void EditorStyle::DrawSelection(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float reveal)
	{
		// 伸びていく途中は、右端の色も途中の色にする（全体を縮めただけに見えないように）
		float t = std::clamp(reveal, 0.0f, 1.0f);
		float right = min.x + (max.x - min.x) * t;
		float rightAlpha = 0.42f + (0.10f - 0.42f) * t;
		ImU32 l = U32(kAccent, 0.42f), r = U32(kAccent2, rightAlpha);
		dl->AddRectFilledMultiColor(min, ImVec2(right, max.y), l, r, r, l);
		DrawSideBar(dl, min, max, 0.4f + 0.6f * t);
	}

	void EditorStyle::DrawHeaderBackground(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float open, float hover)
	{
		ImVec4 bg = ImLerp(kHeaderBg, kHover, std::clamp(hover, 0.0f, 1.0f));
		dl->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(bg));
		// 差し色は見出しの左 6 割ほどで消える
		float mid = min.x + (max.x - min.x) * 0.6f;
		ImU32 tint = U32(kAccent, 0.12f + 0.14f * open), clear = U32(kAccent2, 0.0f);
		dl->AddRectFilledMultiColor(min, ImVec2(mid, max.y), tint, clear, clear, tint);
		// 左端の帯は真ん中から上下へ伸びる
		if (open > 0.01f)
		{
			float cy = (min.y + max.y) * 0.5f, h = (max.y - min.y) * 0.5f * open;
			DrawSideBar(dl, ImVec2(min.x, cy - h), ImVec2(max.x, cy + h), 0.9f);
		}
	}

	// ---- 動き ----

	namespace
	{
		// 同じ key から、用途ごとに別の記憶の場所を作る（ImGui 自身がその key で覚えている開閉の状態などとぶつけない）
		ImGuiID AnimKey(ImGuiID key, int slot) { return ImHashData(&slot, sizeof(slot), key ^ 0x5EED1234u); }
	}

	float EditorStyle::Animate(ImGuiID key, float target, float speed)
	{
		float* value = ImGui::GetStateStorage()->GetFloatRef(AnimKey(key, 1), target);
		float t = 1.0f - std::exp(-speed * ImGui::GetIO().DeltaTime);
		*value += (target - *value) * t;
		if (std::abs(target - *value) < 0.002f) { *value = target; }
		return *value;
	}

	float EditorStyle::HoverFade(ImGuiID key, float speed)
	{
		bool on = ImGui::GetStateStorage()->GetBool(AnimKey(key, 2), false);
		return Animate(AnimKey(key, 3), on ? 1.0f : 0.0f, speed);
	}

	void EditorStyle::SetHovered(ImGuiID key, bool hovered)
	{
		ImGui::GetStateStorage()->SetBool(AnimKey(key, 2), hovered);
	}

	void EditorStyle::DrawVerticalShade(ImDrawList* dl, const ImVec2& min, const ImVec2& max, const ImVec4& top, const ImVec4& bottom)
	{
		ImU32 t = ImGui::ColorConvertFloat4ToU32(top), b = ImGui::ColorConvertFloat4ToU32(bottom);
		dl->AddRectFilledMultiColor(min, max, t, t, b, b);
	}

	void EditorStyle::DrawGradientText(ImDrawList* dl, const ImVec2& pos, const char* text, const ImVec4& left, const ImVec4& right)
	{
		int start = dl->VtxBuffer.Size;
		dl->AddText(pos, IM_COL32_WHITE, text);
		float width = ImGui::CalcTextSize(text).x;
		ImGui::ShadeVertsLinearColorGradientKeepAlpha(dl, start, dl->VtxBuffer.Size, pos, ImVec2(pos.x + width, pos.y),
			ImGui::ColorConvertFloat4ToU32(left), ImGui::ColorConvertFloat4ToU32(right));
	}

	// ひし形を左右に分け、左をすみれ色・右を水色で塗ったマーク。横に名前を細めの色で出す
	void EditorStyle::DrawLogo()
	{
		float font = ImGui::GetFontSize();
		float size = font * 0.95f;
		ImVec2 p = ImGui::GetCursorScreenPos();
		float cy = ImGui::GetWindowPos().y + ImGui::GetWindowHeight() * 0.5f;
		float cx = p.x + size * 0.5f;
		float h = size * 0.5f;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 top(cx, cy - h), bottom(cx, cy + h), left(cx - h * 0.8f, cy), right(cx + h * 0.8f, cy);
		dl->AddTriangleFilled(top, bottom, left, ImGui::ColorConvertFloat4ToU32(kAccent));
		dl->AddTriangleFilled(top, right, bottom, ImGui::ColorConvertFloat4ToU32(kAccent2));
		// 真ん中に小さな穴を空けて、ただのひし形より「マーク」らしく見せる
		float hole = h * 0.32f;
		dl->AddQuadFilled(ImVec2(cx, cy - hole), ImVec2(cx + hole * 0.8f, cy), ImVec2(cx, cy + hole), ImVec2(cx - hole * 0.8f, cy),
			ImGui::GetColorU32(ImGuiCol_MenuBarBg));

		const char* name = "RD ENGINE";
		float textX = p.x + size + font * 0.45f;
		// 名前もマークと同じ 2 色のグラデーション（読みやすいよう明るめの色にする）
		DrawGradientText(dl, ImVec2(textX, cy - font * 0.5f), name, ImVec4(0.60f, 0.86f, 0.91f, 1.0f), ImVec4(0.68f, 0.72f, 0.98f, 1.0f));
		float width = size + font * 0.45f + ImGui::CalcTextSize(name).x + font * 0.6f;
		ImGui::Dummy(ImVec2(width, font));
		// ロゴとメニューの間の縦線
		float lineX = p.x + width - font * 0.3f;
		dl->AddLine(ImVec2(lineX, cy - font * 0.45f), ImVec2(lineX, cy + font * 0.45f), ImGui::GetColorU32(ImGuiCol_Border));
	}

	void EditorStyle::ApplyDarkTitleBar(void* hwnd)
	{
		HWND window = static_cast<HWND>(hwnd);
		BOOL dark = TRUE;
		// 20: DWMWA_USE_IMMERSIVE_DARK_MODE、35: DWMWA_CAPTION_COLOR、36: DWMWA_TEXT_COLOR、34: DWMWA_BORDER_COLOR
		DwmSetWindowAttribute(window, 20, &dark, sizeof(dark));
		auto toRef = [](const ImVec4& c) { return COLORREF(RGB(int(c.x * 255.0f), int(c.y * 255.0f), int(c.z * 255.0f))); };
		COLORREF caption = toRef(kBaseBg);
		COLORREF text = toRef(kTextDim);
		COLORREF border = toRef(kBaseBg);
		DwmSetWindowAttribute(window, 35, &caption, sizeof(caption));
		DwmSetWindowAttribute(window, 36, &text, sizeof(text));
		DwmSetWindowAttribute(window, 34, &border, sizeof(border));
	}
}
