#include "RdPch.h"
#include "Editor/Style/EditorStyle.h"

namespace rd
{
	void EditorStyle::Apply(float dpiScale)
	{
		ImGui::StyleColorsDark();
		ImGuiStyle& style = ImGui::GetStyle();
		style.WindowRounding = 3.0f;
		style.ChildRounding = 3.0f;
		style.FrameRounding = 3.0f;
		style.PopupRounding = 3.0f;
		style.GrabRounding = 3.0f;
		style.TabRounding = 3.0f;
		style.ScrollbarRounding = 3.0f;
		style.WindowBorderSize = 1.0f;
		style.FrameBorderSize = 0.0f;
		style.PopupBorderSize = 1.0f;
		style.WindowPadding = ImVec2(8.0f, 6.0f);
		style.FramePadding = ImVec2(6.0f, 4.0f);
		style.ItemSpacing = ImVec2(6.0f, 4.0f);
		style.ItemInnerSpacing = ImVec2(4.0f, 4.0f);
		style.IndentSpacing = 14.0f;
		style.ScrollbarSize = 12.0f;
		style.WindowMenuButtonPosition = ImGuiDir_None;

		ImVec4* c = style.Colors;
		c[ImGuiCol_Text] = kText;
		c[ImGuiCol_TextDisabled] = kTextDim;
		c[ImGuiCol_WindowBg] = kWindowBg;
		c[ImGuiCol_ChildBg] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_PopupBg] = ImVec4(0.15f, 0.15f, 0.16f, 0.98f);
		c[ImGuiCol_Border] = ImVec4(0.07f, 0.07f, 0.08f, 1.0f);
		c[ImGuiCol_FrameBg] = kFieldBg;
		c[ImGuiCol_FrameBgHovered] = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
		c[ImGuiCol_FrameBgActive] = ImVec4(0.17f, 0.17f, 0.20f, 1.0f);
		c[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		c[ImGuiCol_TitleBgActive] = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
		c[ImGuiCol_MenuBarBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		c[ImGuiCol_Header] = kHeaderBg;
		c[ImGuiCol_HeaderHovered] = ImVec4(0.25f, 0.27f, 0.32f, 1.0f);
		c[ImGuiCol_HeaderActive] = kAccentDim;
		c[ImGuiCol_Button] = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
		c[ImGuiCol_ButtonHovered] = ImVec4(0.29f, 0.30f, 0.34f, 1.0f);
		c[ImGuiCol_ButtonActive] = kAccentDim;
		c[ImGuiCol_CheckMark] = kAccent;
		c[ImGuiCol_SliderGrab] = kAccent;
		c[ImGuiCol_SliderGrabActive] = kAccent;
		c[ImGuiCol_Separator] = ImVec4(0.07f, 0.07f, 0.08f, 1.0f);
		c[ImGuiCol_SeparatorHovered] = kAccentDim;
		c[ImGuiCol_SeparatorActive] = kAccent;
		c[ImGuiCol_ResizeGrip] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		c[ImGuiCol_Tab] = ImVec4(0.12f, 0.12f, 0.13f, 1.0f);
		c[ImGuiCol_TabHovered] = ImVec4(0.22f, 0.23f, 0.27f, 1.0f);
		c[ImGuiCol_TabActive] = kWindowBg;
		c[ImGuiCol_TabUnfocused] = ImVec4(0.11f, 0.11f, 0.12f, 1.0f);
		c[ImGuiCol_TabUnfocusedActive] = ImVec4(0.13f, 0.13f, 0.14f, 1.0f);
		c[ImGuiCol_DockingPreview] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.5f);
		c[ImGuiCol_TableHeaderBg] = kHeaderBg;
		c[ImGuiCol_TableRowBgAlt] = kRowAlt;
		c[ImGuiCol_DragDropTarget] = kAccent;
		c[ImGuiCol_NavHighlight] = kAccent;
		c[ImGuiCol_TextSelectedBg] = ImVec4(kAccent.x, kAccent.y, kAccent.z, 0.35f);

		style.ScaleAllSizes(dpiScale);
	}

	void EditorStyle::SetPlayMode(bool playing, bool paused)
	{
		ImVec4 bar = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
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
}
