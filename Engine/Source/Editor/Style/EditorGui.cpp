#include "RdPch.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	// ツールバーはパネルの端まで背景を塗った帯にする（パネルの余白を打ち消す）
	void EditorGui::BeginToolbar(const char* id)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImVec2 min(ImGui::GetWindowPos().x, pos.y - style.WindowPadding.y);
		ImVec2 max(min.x + ImGui::GetWindowWidth(), pos.y + ImGui::GetFrameHeight() + style.WindowPadding.y);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		dl->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(EditorStyle::kToolbarBg));
		dl->AddLine(ImVec2(min.x, max.y), ImVec2(max.x, max.y), ImGui::GetColorU32(ImGuiCol_Border));
		ImGui::PushID(id);
		ImGui::BeginGroup();
	}

	void EditorGui::EndToolbar()
	{
		ImGui::EndGroup();
		ImGui::PopID();
		ImGui::Dummy(ImVec2(0.0f, ImGui::GetStyle().WindowPadding.y));
	}

	namespace
	{
		// 押された状態のボタンの色: 差し色を薄く敷く（塗りつぶすより軽く見える）。PopStyleColor(3) で戻す
		void PushActiveColors(const ImVec4& color)
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x, color.y, color.z, 0.22f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x, color.y, color.z, 0.32f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x, color.y, color.z, 0.42f));
		}

		// ふだんのアイコンボタン: 背景なし、マウスを乗せたときだけ背景を出す。PopStyleColor(3) で戻す
		void PushFlatColors()
		{
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::kHover);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::kHeaderBg);
		}
	}

	bool EditorGui::ToggleButton(const char* label, bool active)
	{
		if (active) { PushActiveColors(EditorStyle::kAccent); }
		bool pressed = ImGui::Button(label);
		if (active) { ImGui::PopStyleColor(3); }
		return pressed;
	}

	bool EditorGui::IconButton(const char* id, Icon icon, const char* tooltip, bool active, bool enabled, const ImVec4* activeColor)
	{
		float h = ImGui::GetFrameHeight();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::BeginDisabled(!enabled);
		if (active) { PushActiveColors(activeColor ? *activeColor : EditorStyle::kAccent); }
		else { PushFlatColors(); }
		bool pressed = ImGui::Button(id, ImVec2(h, h));
		ImGui::PopStyleColor(3);
		float iconSize = h * 0.6f;
		float pad = (h - iconSize) * 0.5f;
		ImU32 color = EditorIcons::DefaultColor(icon);
		if (active && activeColor) { color = ImGui::ColorConvertFloat4ToU32(*activeColor); }
		if (!enabled) { color = ImGui::GetColorU32(ImGuiCol_TextDisabled); }
		EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(pos.x + pad, pos.y + pad), iconSize, icon, color);
		ImGui::EndDisabled();
		if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) { ImGui::SetTooltip("%s", tooltip); }
		return pressed && enabled;
	}

	bool EditorGui::IconTextButton(const char* id, Icon icon, const char* label, const char* tooltip)
	{
		float h = ImGui::GetFrameHeight();
		float iconSize = h * 0.6f;
		ImGuiStyle& style = ImGui::GetStyle();
		float width = style.FramePadding.x * 2.0f + iconSize + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::PushID(id);
		bool pressed = ImGui::Button("##b", ImVec2(width, h));
		ImGui::PopID();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		EditorIcons::Draw(dl, ImVec2(pos.x + style.FramePadding.x, pos.y + (h - iconSize) * 0.5f), iconSize, icon);
		dl->AddText(ImVec2(pos.x + style.FramePadding.x + iconSize + style.ItemInnerSpacing.x, pos.y + style.FramePadding.y),
			ImGui::GetColorU32(ImGuiCol_Text), label);
		if (tooltip && ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", tooltip); }
		return pressed;
	}

	// 左端に虫眼鏡を描き、その分だけ文字を右へずらす
	bool EditorGui::SearchField(const char* id, std::string& text, const char* hint, float width)
	{
		float w = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
		float h = ImGui::GetFrameHeight();
		float iconSize = h * 0.55f;
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::SetNextItemWidth(w);
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(h * 0.95f, ImGui::GetStyle().FramePadding.y));
		bool changed = ImGui::InputTextWithHint(id, hint, &text);
		ImGui::PopStyleVar();
		EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(pos.x + h * 0.25f, pos.y + (h - iconSize) * 0.5f), iconSize, Icon::Search,
			ImGui::GetColorU32(ImGuiCol_TextDisabled));
		return changed;
	}

	bool EditorGui::Checkbox(const char* label, bool* value)
	{
		bool on = *value;
		// 箱は差し色を薄く敷き、チェックだけはっきりした差し色にする（塗りつぶすと強すぎるため）
		if (on)
		{
			const ImVec4& a = EditorStyle::kAccent;
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(a.x, a.y, a.z, 0.18f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(a.x, a.y, a.z, 0.28f));
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(a.x, a.y, a.z, 0.38f));
			ImGui::PushStyleColor(ImGuiCol_CheckMark, EditorStyle::kAccentHover);
		}
		bool changed = ImGui::Checkbox(label, value);
		if (on) { ImGui::PopStyleColor(4); }
		return changed;
	}

	void EditorGui::ToolbarSeparator()
	{
		ImGui::SameLine();
		ImVec2 p = ImGui::GetCursorScreenPos();
		float h = ImGui::GetFrameHeight();
		ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y + 3.0f), ImVec2(p.x, p.y + h - 3.0f), ImGui::GetColorU32(ImGuiCol_Border), 1.0f);
		ImGui::Dummy(ImVec2(1.0f, h));
		ImGui::SameLine();
	}

	void EditorGui::EmptyHint(const char* text)
	{
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kTextDim);
		ImGui::TextWrapped("%s", text);
		ImGui::PopStyleColor();
	}

	void EditorGui::SectionLabel(const char* text)
	{
		ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kTextDim);
		ImGui::SeparatorText(text);
		ImGui::PopStyleColor();
	}

	bool EditorGui::ConfirmPopup(const char* id, const char* message, const char* const* buttons, int buttonCount, int& choice)
	{
		bool closed = false;
		ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
		if (ImGui::BeginPopupModal(id, nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			ImGui::TextUnformatted(message);
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			for (int i = 0; i < buttonCount; ++i)
			{
				if (i > 0) { ImGui::SameLine(); }
				// 最初のボタン（おすすめの答え）だけ差し色で塗る
				if (i == 0)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::kAccent);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, EditorStyle::kAccentHover);
				}
				bool pressed = ImGui::Button(buttons[i], ImVec2(ImGui::GetFontSize() * 7.0f, 0.0f));
				if (i == 0) { ImGui::PopStyleColor(2); }
				if (pressed)
				{
					choice = i;
					closed = true;
				}
			}
			if (ImGui::IsKeyPressed(ImGuiKey_Escape, false))
			{
				choice = -1;
				closed = true;
			}
			if (closed) { ImGui::CloseCurrentPopup(); }
			ImGui::EndPopup();
		}
		return closed;
	}
}
