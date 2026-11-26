#include "RdPch.h"
#include "Editor/Preview/MaterialPreviewWidget.h"
#include "Editor/Preview/OrbitViewGui.h"
#include <ImGui/imgui.h>

namespace rd
{
	void MaterialPreviewWidget::Draw(const std::string& path)
	{
		mView.SetMaterial(path);
		if (!mView.IsReady())
		{
			ImGui::TextDisabled("Failed to load");
			return;
		}
		DrawOrbitView(mView);
	}
}
