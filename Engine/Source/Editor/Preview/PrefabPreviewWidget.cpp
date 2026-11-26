#include "RdPch.h"
#include "Editor/Preview/PrefabPreviewWidget.h"
#include "Editor/Preview/OrbitViewGui.h"
#include <ImGui/imgui.h>

namespace rd
{
	void PrefabPreviewWidget::Draw(const std::string& path)
	{
		mView.SetPrefab(path);
		if (!mView.IsReady())
		{
			ImGui::TextDisabled("%s", mView.GetError().c_str());
			return;
		}
		ImGui::SameLine();
		ImGui::TextDisabled("%zu actors", mView.GetActorCount());
		DrawOrbitView(mView);
	}
}
