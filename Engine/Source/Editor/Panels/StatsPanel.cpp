#include "RdPch.h"
#include "Editor/Panels/StatsPanel.h"
#include "Asset/AssetManager.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Time/Time.h"
#include "Editor/Editor.h"
#include "Engine.h"
#include "Physics/World/PhysicsWorld.h"
#include "Render/Renderer/Renderer.h"
#include "RHI/Device/GraphicsDevice.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Style/EditorGui.h"

namespace rd
{
	StatsPanel::StatsPanel(Editor& editor) : EditorPanel(editor, "Stats") {}

	void StatsPanel::Update(float dt)
	{
		mFrameTimes.push_back(dt * 1000.0f);
		if (mFrameTimes.size() > 240) { mFrameTimes.erase(mFrameTimes.begin()); }
	}

	void StatsPanel::OnGui()
	{
		Engine& engine = mEditor.GetEngine();
		auto& device = GraphicsDevice::Get();
		const auto& rs = Renderer::Get().GetStats();
		Scene& scene = mEditor.GetScene();

		ImGui::Text("FPS %.1f  (%.2f ms)", engine.GetFps(), engine.GetFrameMs());
		float maxMs = 1.0f;
		for (float t : mFrameTimes) { maxMs = std::max(maxMs, t); }
		ImGui::PlotLines("##frame", mFrameTimes.data(), int(mFrameTimes.size()), 0, nullptr, 0.0f, maxMs * 1.2f, ImVec2(-1, 60));

		if (ImGui::CollapsingHeader(TrId("Rendering"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			uint32_t w, h;
			mEditor.GetViewportSize(w, h);
			ImGui::Text("GPU: %s", device.GetAdapterName().c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Viewport: {} x {}", w, h).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Draw calls: {}   Triangles: {}", rs.drawCalls, rs.triangles).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Meshes: {} visible / {} culled", rs.meshesVisible, rs.meshesCulled).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Shadow casters: {}   Lights: {}", rs.shadowCasters, rs.lights).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Particles: {}   Sprites: {}", rs.particles, rs.sprites).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Dynamic upload: {:.1f} KB/frame", double(device.GetDynamicBytesUsed()) / 1024.0).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Descriptors: {} / {}", device.GetSrvHeap().GetUsedCount(), device.GetSrvHeap().GetCapacity()).c_str());
			bool vsync = device.GetVSync();
			if (EditorGui::Checkbox("VSync", &vsync)) { device.SetVSync(vsync); }
		}
		if (ImGui::CollapsingHeader(TrId("Systems"), ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::TextUnformatted(EditorLocale::Format("Job workers: {}   Pending jobs: {}", JobSystem::GetWorkerCount(), JobSystem::GetPendingCount()).c_str());
			auto assets = AssetManager::Get().GetAllAssets();
			ImGui::TextUnformatted(EditorLocale::Format("Assets: {} (loading {})", assets.size(), AssetManager::Get().GetLoadingCount()).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Actors: {}", scene.GetActorCount()).c_str());
			auto& physics = scene.GetSystem<PhysicsWorld>();
			ImGui::TextUnformatted(EditorLocale::Format("Physics: {} colliders, {} contacts", physics.GetBodyCount(), physics.GetContactCount()).c_str());
			ImGui::TextUnformatted(EditorLocale::Format("Time: {:.2f} s  (scale {:.2f})", Time::TotalTime(), Time::TimeScale()).c_str());
		}
		if (ImGui::CollapsingHeader(TrId("Loaded Assets")))
		{
			for (auto& a : AssetManager::Get().GetAllAssets())
			{
				const char* state = a->IsReady() ? "OK" : (a->IsLoading() ? "..." : "FAIL");
				ImGui::Text("[%s] %-9s %s", state, a->GetTypeName(), a->GetPath().c_str());
			}
		}
	}
}
