#include "RdPch.h"
#include "Editor/Panels/LightingPanel.h"
#include "Core/Time/Time.h"
#include "Editor/Editor.h"
#include "Editor/Inspector/ImGuiInspector.h"
#include "Physics/World/PhysicsWorld.h"
#include "Render/Renderer/Renderer.h"
#include "Scene/Scene.h"

namespace rd
{
	LightingPanel::LightingPanel(Editor& editor) : EditorPanel(editor, "Lighting") {}

	void LightingPanel::OnGui()
	{
		ImGuiInspector inspector(&mEditor);
		if (ImGui::CollapsingHeader("Renderer", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Renderer::Get().GetSettings().Reflect(inspector);
		}
		inspector.ResetFrameState();

		Scene& scene = mEditor.GetScene();
		if (ImGui::CollapsingHeader("Environment (Scene)", ImGuiTreeNodeFlags_DefaultOpen))
		{
			scene.GetEnvironment().Reflect(inspector);
		}
		if (ImGui::CollapsingHeader("Physics / Time"))
		{
			Vector3 gravity = scene.GetGravity();
			if (inspector.Property("Gravity", gravity)) { scene.SetGravity(gravity); }
			inspector.Property("Solver Iterations", scene.GetSystem<PhysicsWorld>().solverIterations, { .speed = 0.1f, .min = 1.0f, .max = 50.0f });
			float timeScale = Time::TimeScale();
			if (inspector.Property("Time Scale", timeScale, { .speed = 0.01f, .min = 0.0f, .max = 10.0f })) { Time::SetTimeScale(timeScale); }
		}
		if (inspector.AnyCommitted()) { mEditor.CommitSettingsEdit("Environment"); }
	}
}
