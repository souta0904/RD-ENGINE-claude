#include "RdPch.h"
#include "Editor/Panels/EditorPanel.h"

namespace rd
{
	void EditorPanel::Draw()
	{
		if (!mOpen) { return; }
		if (!HasPadding()) { ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f)); }
		ImGui::SetNextWindowSize(GetDefaultSize(), ImGuiCond_FirstUseEver);
		bool visible = ImGui::Begin(mTitle, &mOpen, GetWindowFlags());
		if (!HasPadding()) { ImGui::PopStyleVar(); }
		if (visible) { OnGui(); }
		ImGui::End();
	}
}
