#include "RdPch.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Asset/AssetManager.h"
#include "Components/Rendering/CameraComponent.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Commands/ActorLifetimeCommand.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Engine.h"
#include "Physics/Collision/ShapeCollision.h"
#include "Physics/Colliders/Collider.h"
#include "Physics/World/PhysicsWorld.h"
#include "Platform/Input/Input.h"
#include "Render/Renderer/Renderer.h"
#include "Scene/Scene.h"
#include <ImGui/ImGuizmo.h>

namespace rd
{
	namespace
	{
		struct AspectOption
		{
			const char* label;
			float ratio;
		};
		constexpr AspectOption kAspects[] = { { "Free Aspect", 0.0f }, { "16:9", 16.0f / 9.0f }, { "16:10", 16.0f / 10.0f }, { "4:3", 4.0f / 3.0f }, { "1:1", 1.0f } };

		bool IsModelFile(const std::string& ext)
		{
			return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".dae";
		}
	}

	SceneViewPanel::SceneViewPanel(Editor& editor) : EditorPanel(editor, "Viewport") {}

	// プレイを始めたらゲームビュー、止めたらシーンビューへ切り替える（Unity と同じ）
	void SceneViewPanel::Update(float dt)
	{
		bool playing = mEditor.IsPlaying();
		if (playing != mWasPlaying)
		{
			mView = playing ? View::Game : View::Scene;
			mWasPlaying = playing;
		}
		if (UsesEditorCamera()) { mCamera.Update(dt, mHovered); }

		bool gameHasFocus = playing && mView == View::Game && mFocused;
		Input::SetBlocked(!gameHasFocus || ImGui::GetIO().WantTextInput, !gameHasFocus);
	}

	void SceneViewPanel::GetRenderSize(uint32_t& width, uint32_t& height) const
	{
		width = std::max(1u, mWidth);
		height = std::max(1u, mHeight);
	}

	Vector2 SceneViewPanel::ToViewUv(ImVec2 m) const
	{
		return Vector2((m.x - mImagePos.x) / mImageSize.x, (m.y - mImagePos.y) / mImageSize.y);
	}

	void SceneViewPanel::OnGui()
	{
		ImVec2 padding = ImGui::GetStyle().WindowPadding;
		ImGui::SetCursorPos(ImVec2(padding.x, ImGui::GetCursorPosY() + padding.y));
		EditorGui::BeginToolbar("viewToolbar");
		DrawViewSwitch();
		EditorGui::ToolbarSeparator();
		if (mView == View::Scene) { DrawSceneToolbar(); }
		else { DrawGameToolbar(); }
		EditorGui::EndToolbar();

		// ゲームビューで縦横比を選んでいたら、その比率で中央に置く（余りは黒い帯）
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 size(std::max(1.0f, avail.x), std::max(1.0f, avail.y));
		float ratio = mView == View::Game ? kAspects[mAspect].ratio : 0.0f;
		if (ratio > 0.0f)
		{
			if (size.x / size.y > ratio) { size.x = size.y * ratio; }
			else { size.y = size.x / ratio; }
		}
		ImVec2 pos(origin.x + (avail.x - size.x) * 0.5f, origin.y + (avail.y - size.y) * 0.5f);
		ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(0, 0, 0, 255));
		mWidth = uint32_t(size.x);
		mHeight = uint32_t(size.y);
		mImagePos = pos;
		mImageSize = size;

		ImGui::SetCursorScreenPos(pos);
		const auto& output = Renderer::Get().GetOutput();
		if (output.GetResource()) { ImGui::Image(ImTextureID(output.GetGpuSrv().ptr), size); }
		else { ImGui::Dummy(size); }
		mHovered = ImGui::IsItemHovered();
		mFocused = ImGui::IsWindowFocused();
		// 右・中ボタンでカメラを動かし始めたらフォーカスを移す
		if (mHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
		{
			ImGui::SetWindowFocus();
		}

		float aspect = size.x / std::max(size.y, 1.0f);
		RenderCamera cam = mCamera.GetRenderCamera(aspect);
		if (!UsesEditorCamera())
		{
			if (auto main = CameraComponent::FindMain(mEditor.GetScene())) { cam = main->GetRenderCamera(aspect); }
		}

		if (UsesEditorCamera() && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				HandleDrop(static_cast<const char*>(payload->Data), cam.ScreenPointToRay(ToViewUv(ImGui::GetMousePos())));
			}
			ImGui::EndDragDropTarget();
		}

		bool usingGizmo = false;
		Actor* selected = mEditor.GetSelected();
		if (selected && UsesEditorCamera()) { DrawGizmo(selected, cam, usingGizmo); }

		if (mHovered && UsesEditorCamera() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !usingGizmo &&
			!Input::GetKeyRaw(Key::Alt))
		{
			mEditor.Select(Pick(cam.ScreenPointToRay(ToViewUv(ImGui::GetMousePos()))));
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		uint32_t loading = AssetManager::Get().GetLoadingCount();
		if (loading > 0)
		{
			std::string text = std::format("Loading {} assets...", loading);
			dl->AddText(ImVec2(pos.x + 10.0f, pos.y + 8.0f), ImGui::ColorConvertFloat4ToU32(EditorStyle::kWarning), text.c_str());
		}
		if (mView == View::Game && !CameraComponent::FindMain(mEditor.GetScene()))
		{
			dl->AddText(ImVec2(pos.x + 10.0f, pos.y + 8.0f), ImGui::ColorConvertFloat4ToU32(EditorStyle::kTextDim), "No camera in the scene");
		}
		if (mEditor.IsPlaying())
		{
			ImVec4 border = mEditor.GetEngine().IsPaused() ? EditorStyle::kWarning : EditorStyle::kSuccess;
			dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(border), 0.0f, 0, 2.0f);
		}
	}

	void SceneViewPanel::DrawViewSwitch()
	{
		const char* names[] = { "Scene", "Game" };
		for (int i = 0; i < 2; ++i)
		{
			if (i > 0) { ImGui::SameLine(0.0f, 2.0f); }
			if (EditorGui::ToggleButton(names[i], mView == View(i))) { mView = View(i); }
		}
	}

	void SceneViewPanel::DrawSceneToolbar()
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

	void SceneViewPanel::DrawGameToolbar()
	{
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
		if (ImGui::BeginCombo("##aspect", kAspects[mAspect].label))
		{
			for (int i = 0; i < int(std::size(kAspects)); ++i)
			{
				if (ImGui::Selectable(kAspects[i].label, i == mAspect)) { mAspect = i; }
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%u x %u", mWidth, mHeight);
		if (mEditor.IsPlaying())
		{
			ImGui::SameLine();
			ImGui::TextDisabled(mFocused ? "  Input: game" : "  Click to give input to the game");
		}
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
