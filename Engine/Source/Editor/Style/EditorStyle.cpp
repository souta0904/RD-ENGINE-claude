#include "RdPch.h"
#include "Editor/Style/EditorStyle.h"
#include <ImGui/imgui_internal.h>
#include <dwmapi.h>
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

		float x0 = tabBar->BarRect.Min.x + tab->Offset - tabBar->ScrollingAnim;
		float x1 = x0 + tab->Width;
		float y = tabBar->BarRect.Min.y;
		float thickness = IM_ROUND(2.0f * ImGui::GetFontSize() / 15.0f);

		ImDrawList* dl = window->DrawList;
		dl->PushClipRect(tabBar->BarRect.Min, tabBar->BarRect.Max, false);
		if (node->IsFocused)
		{
			// フォーカスのあるタブ: グラデーションの線と、その下にうっすら光が漏れたような影
			DrawAccentGradient(dl, ImVec2(x0, y), ImVec2(x1, y + thickness));
			ImU32 glow = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent, 0.10f));
			ImU32 clear = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent, 0.0f));
			dl->AddRectFilledMultiColor(ImVec2(x0, y + thickness), ImVec2(x1, tabBar->BarRect.Max.y), glow, glow, clear, clear);
		}
		else
		{
			dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + thickness), ImGui::ColorConvertFloat4ToU32(WithAlpha(kTextDim, 0.35f)));
		}
		dl->PopClipRect();
	}

	void EditorStyle::DrawAccentGradient(ImDrawList* dl, const ImVec2& min, const ImVec2& max, float alpha)
	{
		ImU32 left = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent, alpha));
		ImU32 right = ImGui::ColorConvertFloat4ToU32(WithAlpha(kAccent2, alpha));
		dl->AddRectFilledMultiColor(min, max, left, right, right, left);
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
		dl->AddText(ImVec2(textX, cy - font * 0.5f), ImGui::ColorConvertFloat4ToU32(kText), name);
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
