#include "RdPch.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Asset/AssetManager.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Commands/ActorLifetimeCommand.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Physics/Collision/ShapeCollision.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/World/PhysicsWorld.h"
#include "Platform/Input/Input.h"
#include "Scene/Scene.h"
#include <ImGui/ImGuizmo.h>

namespace rd
{
	namespace
	{
		bool IsModelFile(const std::string& ext)
		{
			return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".dae";
		}
	}

	SceneViewPanel::SceneViewPanel(Editor& editor) : ViewPanel(editor, "Scene") {}

	void SceneViewPanel::Update(float dt)
	{
		mCamera.Update(dt, mHovered && WasVisible());
	}

	void SceneViewPanel::OnGui()
	{
		BeginViewToolbar();
		DrawToolbar();
		EditorGui::EndToolbar();
		DrawView(0.0f);

		RenderCamera cam = mCamera.GetRenderCamera(GetAspect());
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				HandleDrop(static_cast<const char*>(payload->Data), cam.ScreenPointToRay(ToViewUv(ImGui::GetMousePos())));
			}
			ImGui::EndDragDropTarget();
		}

		bool usingGizmo = false;
		Actor* selected = mEditor.GetSelected();
		if (selected) { DrawGizmo(selected, cam, usingGizmo); }

		if (mHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !usingGizmo && !Input::GetKeyRaw(Key::Alt))
		{
			mEditor.Select(Pick(cam.ScreenPointToRay(ToViewUv(ImGui::GetMousePos()))));
		}

		uint32_t loading = AssetManager::Get().GetLoadingCount();
		if (loading > 0)
		{
			std::string text = std::format("Loading {} assets...", loading);
			ImGui::GetWindowDrawList()->AddText(ImVec2(mImagePos.x + 10.0f, mImagePos.y + 8.0f), ImGui::ColorConvertFloat4ToU32(EditorStyle::kWarning), text.c_str());
		}
	}

	void SceneViewPanel::DrawToolbar()
	{
		ImGui::Checkbox("Grid", &showGrid);
		ImGui::SameLine();
		ImGui::Checkbox("Gizmos", &showGizmos);
		ImGui::SameLine();
		ImGui::Checkbox("Physics", &showPhysics);
		EditorGui::ToolbarSeparator();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("Speed");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
		ImGui::DragFloat("##speed", &mCamera.moveSpeed, 0.1f, 0.5f, 500.0f, "%.1f");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Camera speed (mouse wheel while flying)"); }
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("FOV");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 3.5f);
		ImGui::DragFloat("##fov", &mCamera.fov, 0.2f, 10.0f, 120.0f, "%.0f");
	}

	// Ctrl を押している間は決まった間隔にスナップする。ギズモを離したときに Undo の履歴へ積む
	void SceneViewPanel::DrawGizmo(Actor* selected, const RenderCamera& cam, bool& usingGizmo)
	{
		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(mImagePos.x, mImagePos.y, mImageSize.x, mImageSize.y);
		Matrix4 world = selected->GetTransform().GetWorldMatrix();
		ImGuizmo::OPERATION op = mGizmoOp == GizmoOp::Move ? ImGuizmo::TRANSLATE : (mGizmoOp == GizmoOp::Rotate ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
		ImGuizmo::MODE mode = (mGizmoLocal || mGizmoOp == GizmoOp::Scale) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
		float snap = mGizmoOp == GizmoOp::Rotate ? 15.0f : (mGizmoOp == GizmoOp::Scale ? 0.1f : 0.5f);
		float snapValues[3] = { snap, snap, snap };
		bool useSnap = Input::GetKeyRaw(Key::Control);
		if (ImGuizmo::Manipulate(cam.view.Data(), cam.proj.Data(), op, mode, world.Data(), nullptr, useSnap ? snapValues : nullptr))
		{
			selected->GetTransform().SetWorldMatrix(world);
		}
		usingGizmo = ImGuizmo::IsUsing();
		if (mGizmoWasUsing && !usingGizmo) { mEditor.CommitActorEdit("Transform"); }
		mGizmoWasUsing = usingGizmo;
	}

	// メッシュは三角形と、メッシュのないもの（ライト・カメラなど）は小さな球と、コライダーはその形と当てて一番近いものを選ぶ
	Actor* SceneViewPanel::Pick(const Ray& ray) const
	{
		Scene& scene = mEditor.GetScene();
		Actor* best = nullptr;
		float bestDist = math::kInfinity;
		scene.ForEachActor([&](Actor* actor)
			{
				if (!actor->IsActive()) { return; }
				float d;
				auto mr = actor->GetComponent<MeshRenderer>();
				if (mr)
				{
					if (mr->Raycast(ray, d) && d < bestDist) { bestDist = d; best = actor; }
					return;
				}
				WorldShape s;
				s.type = ShapeType::Sphere;
				s.center = actor->GetTransform().GetPosition();
				s.radius = 0.35f;
				Vector3 n;
				if (RaycastShape(s, ray, bestDist, d, n) && d < bestDist) { bestDist = d; best = actor; }
			});
		RaycastHit hit;
		if (scene.GetSystem<PhysicsWorld>().Raycast(ray, bestDist, hit, ~0u, true) && hit.distance < bestDist)
		{
			best = hit.collider->GetActor();
		}
		return best;
	}

	// 置く位置は、何かに当たればそこ、なければ地面（y=0）、地面も見えなければ前方
	void SceneViewPanel::HandleDrop(const std::string& path, const Ray& ray)
	{
		std::string ext = GetExtension(path);
		if (ext == ".scene")
		{
			mEditor.OpenScene(path);
			return;
		}
		Scene& scene = mEditor.GetScene();
		Vector3 place = ray.GetPoint(10.0f);
		RaycastHit hit;
		if (scene.GetSystem<PhysicsWorld>().Raycast(ray, 500.0f, hit)) { place = hit.point; }
		else if (ray.direction.y < -0.01f) { place = ray.GetPoint(-ray.origin.y / ray.direction.y); }

		Actor* actor = nullptr;
		if (ext == ".prefab")
		{
			actor = scene.Instantiate(path);
		}
		else if (IsModelFile(ext))
		{
			actor = scene.CreateActor(GetStem(path));
			actor->AddComponent<MeshRenderer>()->SetModel(path);
		}
		else
		{
			Log::Warning(LogCat::Editor, "Cannot drop '{}' into the scene view", path);
			return;
		}
		if (actor)
		{
			actor->GetTransform().SetPosition(place);
			mEditor.Select(actor);
			mEditor.PushCommand(ActorLifetimeCommand::Created("Drop " + GetFileName(path), scene, { actor }));
		}
	}
}
