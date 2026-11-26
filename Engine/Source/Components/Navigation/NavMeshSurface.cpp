#include "RdPch.h"
#include "Components/Navigation/NavMeshSurface.h"
#include "AI/Navigation/NavMeshBuilder.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Asset/Model/Model.h"
#include "Components/Navigation/NavMeshAgent.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Math/Geometry/AABB.h"
#include "Render/Debug/DebugDraw.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Scene.h"
#include <chrono>
#if RD_EDITOR
#include "Editor/Core/EditorLocale.h"
#include "Editor/Style/EditorGui.h"
#include <ImGui/imgui.h>
#endif

namespace rd
{
	void NavMeshSurface::Reflect(PropertyVisitor& v)
	{
		v.Property("Agent Radius", agentRadius, { .speed = 0.01f, .min = 0.01f, .max = 100.0f, .tooltip = "How far the walkable area stays away from walls" });
		v.Property("Agent Height", agentHeight, { .speed = 0.01f, .min = 0.01f, .max = 100.0f, .tooltip = "Places with less headroom than this are not walkable" });
		v.Property("Agent Max Slope", agentMaxSlope, { .speed = 0.1f, .min = 0.0f, .max = 89.0f, .tooltip = "Steeper slopes (degrees) are not walkable" });
		v.Property("Agent Step Height", agentStepHeight, { .speed = 0.01f, .min = 0.0f, .max = 100.0f, .tooltip = "Steps up to this height can be climbed" });
		v.Property("Collect Objects", collectObjects);
		v.Property("Size", size, { .tooltip = "Box used when Collect Objects is Volume (local to this actor)" });
		v.Property("Center", center, { .tooltip = "Box used when Collect Objects is Volume (local to this actor)" });
		v.Property("Layer Mask", layerMask);
		v.Property("Default Area", defaultArea, { .speed = 0.1f, .min = 0.0f, .max = 31.0f });
		v.Property("Override Voxel Size", overrideVoxelSize);
		v.Property("Voxel Size", voxelSize, { .speed = 0.001f, .min = 0.01f, .max = 10.0f,
			.flags = overrideVoxelSize ? PropertyMeta::None : PropertyMeta::ReadOnly });
		v.Property("Min Region Area", minRegionArea, { .speed = 0.1f, .min = 0.0f, .max = 10000.0f });
		v.Property("Data", data);
		if (!overrideVoxelSize) { voxelSize = agentRadius / 3.0f; }
	}

	void NavMeshSurface::Awake()
	{
		if (!data.empty()) { GetScene()->GetSystem<NavigationSystem>().LoadSurface(data); }
	}

	std::string NavMeshSurface::GetBakePath() const
	{
		std::filesystem::path scene = GetScene() ? GetScene()->GetPath() : std::string();
		std::string file = "NavMesh-" + GetActor()->GetName() + ".json";
		if (scene.empty()) { return "Assets/" + file; }
		return (scene.parent_path() / scene.stem() / file).generic_string();
	}

	bool NavMeshSurface::Bake()
	{
		Scene* scene = GetScene();
		Actor* self = GetActor();
		if (!scene || !self) { return false; }
		const auto startTime = std::chrono::steady_clock::now();

		NavMeshBuildSettings settings;
		settings.agentRadius = agentRadius;
		settings.agentHeight = agentHeight;
		settings.agentMaxSlope = agentMaxSlope;
		settings.agentClimb = agentStepHeight;
		settings.cellSize = std::max(0.01f, GetVoxelSize());
		settings.minRegionArea = minRegionArea;
		AABB volume;
		if (collectObjects == NavMeshCollectObjects::Volume)
		{
			volume = AABB::FromCenterExtents(center, size * 0.5f).Transform(GetTransform().GetWorldMatrix());
			settings.useBounds = true;
			settings.boundsMin = volume.min;
			settings.boundsMax = volume.max;
		}

		// 集める: 有効な MeshRenderer のうち、範囲・レイヤーに合い、エージェント（動くもの）でないもの
		NavMeshBuilder builder;
		int meshCount = 0, notReady = 0;
		std::vector<Vector3> positions;
		for (MeshRenderer* renderer : scene->FindComponents<MeshRenderer>())
		{
			Actor* actor = renderer->GetActor();
			if (!renderer->IsEnabled() || !actor->IsActive()) { continue; }
			if (collectObjects == NavMeshCollectObjects::Children && actor != self && !actor->IsDescendantOf(self)) { continue; }
			if ((uint32_t(layerMask) & (1u << actor->GetLayer())) == 0) { continue; }
			if (actor->GetComponentInParent<NavMeshAgent>()) { continue; }
			const auto& model = renderer->GetModel();
			if (!model) { continue; }
			if (!model->IsReady())
			{
				++notReady;
				continue;
			}
			if (model->HasSkinning()) { continue; }
			if (settings.useBounds && !renderer->GetWorldBounds().Overlaps(volume)) { continue; }

			const Matrix4& world = actor->GetTransform().GetWorldMatrix();
			positions.clear();
			positions.reserve(model->GetCollisionPositions().size());
			for (const Vector3& p : model->GetCollisionPositions()) { positions.push_back(TransformPoint(p, world)); }
			// 裏返しの拡大（負の拡大率が奇数個。行列式が負）では三角形の向きが逆になるので並びを戻す
			const auto& m = world.m;
			float det = m[0][0] * (m[1][1] * m[2][2] - m[1][2] * m[2][1]) - m[0][1] * (m[1][0] * m[2][2] - m[1][2] * m[2][0]) +
				m[0][2] * (m[1][0] * m[2][1] - m[1][1] * m[2][0]);
			if (det < 0.0f)
			{
				std::vector<uint32_t> flipped = model->GetCollisionIndices();
				for (size_t i = 0; i + 2 < flipped.size(); i += 3) { std::swap(flipped[i + 1], flipped[i + 2]); }
				builder.AddTriangles(positions, flipped, defaultArea);
			}
			else
			{
				builder.AddTriangles(positions, model->GetCollisionIndices(), defaultArea);
			}
			++meshCount;
		}
		if (notReady > 0) { Logger::Warning(LogCat::Scene, "NavMeshSurface: {} models are still loading and were skipped", notReady); }

		NavMeshBuildResult result;
		if (!builder.Build(settings, result))
		{
			mLastBake = { true, 0, meshCount, 0.0 };
			Logger::Warning(LogCat::Scene, "NavMeshSurface: no walkable surface ({} meshes, {} triangles)", meshCount, builder.GetInputTriangleCount());
			return false;
		}
		std::string path = GetBakePath();
		if (!result.SaveJson(path))
		{
			Logger::Error(LogCat::Scene, "NavMeshSurface: failed to save {}", path);
			return false;
		}
		data = path;
		scene->GetSystem<NavigationSystem>().LoadSurface(data, true);

		double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - startTime).count();
		mLastBake = { true, result.GetTriangleCount(), meshCount, ms };
		Logger::Info(LogCat::Scene, "NavMeshSurface: baked {} ({} input triangles, {} meshes, {:.0f} ms)", path, builder.GetInputTriangleCount(), meshCount, ms);
		return true;
	}

	void NavMeshSurface::ClearData()
	{
		data.clear();
		mLastBake = {};
		if (Scene* scene = GetScene()) { scene->GetSystem<NavigationSystem>().ClearSurface(); }
	}

#if RD_EDITOR
	bool NavMeshSurface::OnInspectorGUI()
	{
		bool changed = false;
		ImGui::Spacing();
		if (EditorGui::AccentButton(Tr("Bake"))) { changed = Bake(); }
		ImGui::SameLine();
		if (ImGui::Button(Tr("Clear")) && !data.empty())
		{
			ClearData();
			changed = true;
		}
		if (mLastBake.done)
		{
			ImGui::SameLine();
			if (mLastBake.triangles == 0) { ImGui::TextDisabled("%s", Tr("No walkable surface was found")); }
			else
			{
				ImGui::TextDisabled("%s", EditorLocale::Format("{} triangles from {} meshes ({:.0f} ms)", mLastBake.triangles, mLastBake.meshes, mLastBake.ms).c_str());
			}
		}
		return changed;
	}
#endif

	void NavMeshLink::Reflect(PropertyVisitor& v)
	{
		v.Property("Start Point", startPoint);
		v.Property("End Point", endPoint);
		v.Property("Width", width, { .speed = 0.1f, .min = 0.0f, .max = 100.0f });
		v.Property("Cost Modifier", costModifier);
		v.Property("Is Overriding Cost", isOverridingCost);
		v.Property("Bidirectional", bidirectional);
		v.Property("Activated", activated);
		v.Property("Area", area);
	}

	void NavMeshLink::Start()
	{
		Register();
	}

	void NavMeshLink::OnDestroy()
	{
		if (Scene* scene = GetScene()) { scene->GetSystem<NavigationSystem>().RemoveLink(this); }
	}

	void NavMeshLink::Register()
	{
		NavigationSystem& nav = GetScene()->GetSystem<NavigationSystem>();
		if (!activated || !IsEnabled())
		{
			nav.RemoveLink(this);
			return;
		}
		const Matrix4& m = GetTransform().GetWorldMatrix();
		NavMesh::Link link;
		link.start = TransformPoint(startPoint, m);
		link.end = TransformPoint(endPoint, m);
		link.bidirectional = bidirectional;
		link.costModifier = isOverridingCost ? costModifier : 1.0f;
		link.area = area;
		nav.SetLink(this, link);
	}

#if RD_EDITOR
	void NavMeshLink::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		const Matrix4& m = GetTransform().GetWorldMatrix();
		Color color = selected ? Color(1.0f, 0.8f, 0.2f, 1.0f) : Color(1.0f, 0.6f, 0.1f, 0.5f);
		draw.Arrow(TransformPoint(startPoint, m), TransformPoint(endPoint, m), color);
	}
#endif
}
