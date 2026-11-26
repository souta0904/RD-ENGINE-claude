#include "RdPch.h"
#include "Editor/Preview/ModelPreviewWidget.h"
#include "Asset/Model/Model.h"
#include "Editor/Preview/OrbitViewGui.h"
#include <ImGui/imgui.h>

namespace rd
{
	void ModelPreviewWidget::Draw(const std::string& path)
	{
		mView.SetModel(path);
		Model* model = mView.GetModel();
		if (!model || !model->IsReady()) { return; }

		const auto& clips = model->GetClips();
		if (!clips.empty())
		{
			const char* current = mView.clip >= 0 && mView.clip < int(clips.size()) ? clips[size_t(mView.clip)]->name.c_str() : "Bind Pose";
			ImGui::SetNextItemWidth(-ImGui::GetFrameHeight() * 2.5f);
			if (ImGui::BeginCombo("##clip", current))
			{
				if (ImGui::Selectable("Bind Pose", mView.clip < 0)) { mView.clip = -1; }
				for (int i = 0; i < int(clips.size()); ++i)
				{
					ImGui::PushID(i);
					if (ImGui::Selectable(clips[size_t(i)]->name.c_str(), mView.clip == i))
					{
						mView.clip = i;
						mView.time = 0.0f;
						mPlaying = true;
					}
					ImGui::PopID();
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			ImGui::BeginDisabled(mView.clip < 0);
			if (ImGui::Button(mPlaying ? "Pause" : "Play", ImVec2(-1.0f, 0.0f))) { mPlaying = !mPlaying; }
			ImGui::EndDisabled();
			if (mView.clip >= 0 && mView.clip < int(clips.size()))
			{
				float duration = clips[size_t(mView.clip)]->duration;
				float t = duration > 0.0f ? std::fmod(mView.time, duration) : 0.0f;
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::SliderFloat("##time", &t, 0.0f, std::max(duration, 1.0e-3f), "%.2f s"))
				{
					mView.time = t;
					mPlaying = false;
				}
			}
		}
		if (mPlaying && mView.clip >= 0) { mView.time += ImGui::GetIO().DeltaTime; }
		DrawOrbitView(mView);
	}
}
