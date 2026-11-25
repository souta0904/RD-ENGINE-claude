#include "RdPch.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include <ImGui/imgui_internal.h>
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
		// 上をほんの少し明るくして、帯がわずかに浮いて見えるようにする
		const ImVec4& bg = EditorStyle::kToolbarBg;
		EditorStyle::DrawVerticalShade(dl, min, max, ImVec4(bg.x + 0.028f, bg.y + 0.030f, bg.z + 0.034f, 1.0f), bg);
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

		// ふだんのボタン: 背景なし、マウスを乗せると背景がふわっと出る（hover は 0〜1）。PopStyleColor(3) で戻す
		void PushFlatColors(float hover)
		{
			ImVec4 bg = EditorStyle::kHover;
			bg.w *= hover;
			ImGui::PushStyleColor(ImGuiCol_Button, bg);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, EditorStyle::kHeaderBg);
		}

		// ふだんの面のボタン（ToggleButton のオフ）: 面の色から、乗せたときの色へふわっと変える
		void PushSolidColors(float hover)
		{
			ImVec4 base = ImGui::GetStyleColorVec4(ImGuiCol_Button);
			ImVec4 bg = ImLerp(base, ImGui::GetStyleColorVec4(ImGuiCol_ButtonHovered), hover);
			ImGui::PushStyleColor(ImGuiCol_Button, bg);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, bg);
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
		}
	}

	bool EditorGui::ToggleButton(const char* label, bool active)
	{
		ImGuiID key = ImGui::GetID(label);
		float hover = EditorStyle::HoverFade(key);
		if (active) { PushActiveColors(EditorStyle::kAccent); }
		else { PushSolidColors(hover); }
		bool pressed = ImGui::Button(label);
		ImGui::PopStyleColor(3);
		EditorStyle::SetHovered(key, ImGui::IsItemHovered());
		return pressed;
	}

	bool EditorGui::IconButton(const char* id, Icon icon, const char* tooltip, bool active, bool enabled, const ImVec4* activeColor)
	{
		float h = ImGui::GetFrameHeight();
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGuiID key = ImGui::GetID(id);
		float hover = EditorStyle::HoverFade(key);
		ImGui::BeginDisabled(!enabled);
		if (active) { PushActiveColors(activeColor ? *activeColor : EditorStyle::kAccent); }
		else { PushFlatColors(hover); }
		bool pressed = ImGui::Button(id, ImVec2(h, h));
		ImGui::PopStyleColor(3);
		EditorStyle::SetHovered(key, enabled && ImGui::IsItemHovered());
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
		ImGuiID key = ImGui::GetID("##b");
		PushSolidColors(EditorStyle::HoverFade(key));
		bool pressed = ImGui::Button("##b", ImVec2(width, h));
		ImGui::PopStyleColor(3);
		EditorStyle::SetHovered(key, ImGui::IsItemHovered());
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

	// 何もないときの案内: 残りの広さの真ん中に、大きめの薄いアイコンと、折り返した文字を中央ぞろえで出す
	void EditorGui::EmptyState(Icon icon, const char* text)
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 origin = ImGui::GetCursorScreenPos();
		float font = ImGui::GetFontSize();
		float iconSize = font * 2.6f;
		float wrap = std::min(avail.x - font * 2.0f, font * 22.0f);
		if (wrap < font * 4.0f) { EmptyHint(text); return; }
		ImVec2 textSize = ImGui::CalcTextSize(text, nullptr, false, wrap);
		float total = iconSize + font * 0.9f + textSize.y;
		float top = origin.y + std::max(font, (avail.y - total) * 0.42f);
		float cx = origin.x + avail.x * 0.5f;
		ImDrawList* dl = ImGui::GetWindowDrawList();
		// アイコンの後ろに、差し色のごく淡い丸い光を置く
		float glowR = iconSize * 0.95f;
		ImVec2 c(cx, top + iconSize * 0.5f);
		for (int i = 6; i >= 1; --i)
		{
			float r = glowR * float(i) / 6.0f;
			dl->AddCircleFilled(c, r, ImGui::ColorConvertFloat4ToU32(ImVec4(EditorStyle::kAccent.x, EditorStyle::kAccent.y, EditorStyle::kAccent.z, 0.022f)), 48);
		}
		EditorIcons::Draw(dl, ImVec2(cx - iconSize * 0.5f, top), iconSize, icon, ImGui::ColorConvertFloat4ToU32(ImVec4(0.55f, 0.60f, 0.65f, 0.55f)));
		// 1 行ずつ中央にそろえて書く（ImGui の折り返しは左ぞろえなので、自分で行に分ける）
		float y = top + iconSize + font * 0.9f;
		const char* p = text;
		const char* end = text + std::strlen(text);
		ImFont* f = ImGui::GetFont();
		while (p < end)
		{
			const char* lineEnd = f->CalcWordWrapPositionA(1.0f, p, end, wrap);
			if (lineEnd == p) { lineEnd = p + 1; }
			float w = ImGui::CalcTextSize(p, lineEnd).x;
			dl->AddText(ImVec2(IM_ROUND(cx - w * 0.5f), y), ImGui::ColorConvertFloat4ToU32(EditorStyle::kTextDim), p, lineEnd);
			y += ImGui::GetTextLineHeightWithSpacing();
			p = lineEnd;
			while (p < end && (*p == ' ' || *p == '\n')) { ++p; }
		}
		ImGui::Dummy(ImVec2(avail.x, std::max(1.0f, y - origin.y)));
	}

	// 背景・矢印・見出しの文字は ImGui に描かせず（透明にして）、自分で描く。
	//  背景は見出しの下の層に描く（描く層を 2 つに分けて順番を入れ替える）。
	//  矢印は開くと右向きから下向きへ回り、文字はマウスを乗せると少し明るくなる
	bool EditorGui::CollapsingHeader(const char* label, int flags)
	{
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		ImDrawList* dl = window->DrawList;
		ImGuiID key = window->GetID(label);
		const ImVec2 cursor = window->DC.CursorPos;
		const ImGuiStyle& style = ImGui::GetStyle();
		float hover = EditorStyle::HoverFade(key, 16.0f);

		ImDrawListSplitter splitter;
		splitter.Split(dl, 2);
		splitter.SetCurrentChannel(dl, 1);
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(1.0f, 1.0f, 1.0f, 0.05f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		bool open = ImGui::CollapsingHeader(label, flags);
		ImGui::PopStyleColor(4);
		EditorStyle::SetHovered(key, ImGui::IsItemHovered());
		const ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
		float openAnim = EditorStyle::Animate(key, open ? 1.0f : 0.0f, 18.0f);

		// 矢印（ImGui と同じ場所・大きさ。開き具合に合わせて回す）
		float font = ImGui::GetFontSize();
		float textY = cursor.y + std::max(style.FramePadding.y, window->DC.CurrLineTextBaseOffset);
		ImVec2 center(cursor.x + style.FramePadding.x + font * 0.5f, textY + font * 0.5f);
		float r = font * 0.40f;
		float angle = openAnim * IM_PI * 0.5f;
		float cs = std::cos(angle), sn = std::sin(angle);
		auto rot = [&](float x, float y) { return ImVec2(center.x + x * cs - y * sn, center.y + x * sn + y * cs); };
		ImU32 arrowColor = ImGui::ColorConvertFloat4ToU32(ImLerp(EditorStyle::kTextDim, EditorStyle::kText, 0.5f + 0.5f * std::max(hover, openAnim)));
		dl->AddTriangleFilled(rot(0.75f * r, 0.0f), rot(-0.75f * r, 0.866f * r), rot(-0.75f * r, -0.866f * r), arrowColor);

		// 見出しの文字（"##" から後ろは ID なので出さない。前に付いた空白はアイコンの場所なのでそのまま幅に使う）
		const char* labelEnd = ImGui::FindRenderedTextEnd(label);
		std::string shown(label, labelEnd);
		size_t lead = shown.find_first_not_of(' ');
		float x = cursor.x + font + style.FramePadding.x * 3.0f;
		if (lead != std::string::npos)
		{
			x += ImGui::CalcTextSize(shown.c_str(), shown.c_str() + lead).x;
			ImU32 color = ImGui::ColorConvertFloat4ToU32(ImLerp(ImVec4(0.80f, 0.82f, 0.85f, 1.0f), EditorStyle::kText, hover));
			dl->PushClipRect(min, max, true);
			dl->AddText(ImVec2(x, textY), color, shown.c_str() + lead);
			dl->PopClipRect();
		}

		splitter.SetCurrentChannel(dl, 0);
		EditorStyle::DrawHeaderBackground(dl, min, max, openAnim, hover);
		splitter.Merge(dl);
		return open;
	}

	bool EditorGui::AccentButton(const char* label, const ImVec2& size)
	{
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImGuiID key = ImGui::GetID(label);
		float hover = EditorStyle::HoverFade(key, 14.0f);
		ImDrawListSplitter splitter;
		splitter.Split(dl, 2);
		splitter.SetCurrentChannel(dl, 1);
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		bool pressed = ImGui::Button(label, size);
		ImGui::PopStyleColor(3);
		EditorStyle::SetHovered(key, ImGui::IsItemHovered());
		bool held = ImGui::IsItemActive();
		splitter.SetCurrentChannel(dl, 0);
		// マウスを乗せるとふわっと明るく、押している間はさらに明るくする
		float lift = (held ? 0.16f : 0.0f) + 0.08f * hover;
		auto up = [lift](const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(ImVec4(c.x + lift, c.y + lift, c.z + lift, 1.0f)); };
		ImU32 left = up(EditorStyle::kAccentDim), right = up(EditorStyle::kAccent2Dim);
		ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
		dl->AddRectFilledMultiColor(min, max, left, right, right, left);
		// 上の縁に 1 本だけ明るい線を入れて、光が当たっているように見せる
		EditorStyle::DrawAccentGradient(dl, min, ImVec2(max.x, min.y + 1.0f), 0.55f);
		splitter.Merge(dl);
		return pressed;
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
				// 最初のボタン（おすすめの答え）だけ差し色のグラデーションで塗る
				ImVec2 size(ImGui::GetFontSize() * 7.0f, 0.0f);
				bool pressed = i == 0 ? AccentButton(buttons[i], size) : ImGui::Button(buttons[i], size);
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
