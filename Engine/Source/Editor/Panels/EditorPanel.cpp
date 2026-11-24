#include "RdPch.h"
#include "Editor/Panels/EditorPanel.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Style/EditorStyle.h"

namespace rd
{
	void EditorPanel::Draw()
	{
		if (!mOpen) { return; }
		if (!HasPadding()) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); }
		ImGui::SetNextWindowSize(GetDefaultSize(), ImGuiCond_FirstUseEver);
		// 見出しは訳、ID は英語の名前（言語を変えてもドッキングの配置が保たれる）
		bool visible = ImGui::Begin(TrId(mTitle), &mOpen, GetWindowFlags());
		if (!HasPadding()) { ImGui::PopStyleVar(); }
		if (visible)
		{
			EditorStyle::DrawTabHighlight();
			OnGui();
		}
		ImGui::End();
	}
}
