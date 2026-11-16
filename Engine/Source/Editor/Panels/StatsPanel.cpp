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

		if (ImGui::CollapsingHeader("Rendering", ImGuiTreeNodeFlags_DefaultOpen))
		{
			uint32_t w, h;
			mEditor.GetViewportSize(w, h);
			ImGui::Text("GPU: %s", device.GetAdapterName().c_str());
			ImGui::Text("Viewport: %u x %u", w, h);
			ImGui::Text("Draw calls: %u   Triangles: %u", rs.drawCalls, rs.triangles);
			ImGui::Text("Meshes: %u visible / %u culled", rs.meshesVisible, rs.meshesCulled);
			ImGui::Text("Shadow casters: %u   Lights: %u", rs.shadowCasters, rs.lights);
			ImGui::Text("Particles: %u   Sprites: %u", rs.particles, rs.sprites);
			ImGui::Text("Dynamic upload: %.1f KB/frame", double(device.GetDynamicBytesUsed()) / 1024.0);
			ImGui::Text("Descriptors: %u / %u", device.GetSrvHeap().GetUsedCount(), device.GetSrvHeap().GetCapacity());
			bool vsync = device.GetVSync();
			if (ImGui::Checkbox("VSync", &vsync)) { device.SetVSync(vsync); }
		}
		if (ImGui::CollapsingHeader("Systems", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Job workers: %u   Pending jobs: %u", JobSystem::GetWorkerCount(), JobSystem::GetPendingCount());
			auto assets = AssetManager::Get().GetAllAssets();
			ImGui::Text("Assets: %zu (loading %u)", assets.size(), AssetManager::Get().GetLoadingCount());
			ImGui::Text("Actors: %zu", scene.GetActorCount());
			auto& physics = scene.GetSystem<PhysicsWorld>();
			ImGui::Text("Physics: %u colliders, %u contacts", physics.GetBodyCount(), physics.GetContactCount());
			ImGui::Text("Time: %.2f s  (scale %.2f)", Time::TotalTime(), Time::TimeScale());
		}
		if (ImGui::CollapsingHeader("Loaded Assets"))
		{
			for (auto& a : AssetManager::Get().GetAllAssets())
			{
				const char* state = a->IsReady() ? "OK" : (a->IsLoading() ? "..." : "FAIL");
				ImGui::Text("[%s] %-9s %s", state, a->GetTypeName(), a->GetPath().c_str());
			}
		}
	}
}
