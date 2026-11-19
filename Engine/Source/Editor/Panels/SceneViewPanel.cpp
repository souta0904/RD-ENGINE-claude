#include "RdPch.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
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
#include "Render/Material/MaterialAsset.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"
#include <ImGui/ImGuizmo.h>

namespace rd
{
	SceneViewPanel::SceneViewPanel(Editor& editor) : ViewPanel(editor, "Scene") {}

	// パネルは OnGui（見えているときだけ）の後に Update（毎フレーム）が呼ばれる。
	//  このフレームにシーンビューの上でドラッグしていなければ、仮のアクターを片付ける（外へ出た・やめた・隠れた）
	void SceneViewPanel::Update(float dt)
	{
		mCamera.Update(dt, mHovered && WasVisible());
		if (!mPreviewTouched) { ClearDragPreview(); }
		mPreviewTouched = false;
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
			// マテリアルを運んでいる間は、落とすと入るモデルの名前をマウスの横に出す
			const ImGuiPayload* peek = ImGui::GetDragDropPayload();
			if (peek && peek->IsDataType("ASSET_PATH") && MaterialAsset::IsMaterialPath(static_cast<const char*>(peek->Data)))
			{
				int sub = -1;
				MeshRenderer* target = PickRenderer(cam.ScreenPointToRay(ToViewUv(ImGui::GetMousePos())), sub);
				std::string text = target ? EditorLocale::Format("Assign to {}", target->GetActor()->GetName()) : std::string(Tr("Drop onto a model"));
				// ドラッグ中のファイル名（ImGui が右下に出す）と重ならないよう、マウスの右上に出す
				ImVec2 size = ImGui::CalcTextSize(text.c_str());
				ImVec2 pos(ImGui::GetMousePos().x + 16.0f, ImGui::GetMousePos().y - size.y - 12.0f);
				ImDrawList* dl = ImGui::GetWindowDrawList();
				dl->AddRectFilled(ImVec2(pos.x - 4.0f, pos.y - 2.0f), ImVec2(pos.x + size.x + 4.0f, pos.y + size.y + 2.0f), IM_COL32(0, 0, 0, 170), 3.0f);
				dl->AddText(pos, target ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 190, 120, 255), text.c_str());
			}
			// モデル・プレハブを運んでいる間は、置かれる位置に本物を出しておく
			Ray mouseRay = cam.ScreenPointToRay(ToViewUv(ImGui::GetMousePos()));
			if (peek && peek->IsDataType("ASSET_PATH") && CanPlace(static_cast<const char*>(peek->Data)))
			{
				UpdateDragPreview(static_cast<const char*>(peek->Data), mouseRay);
				mPreviewTouched = true;
			}
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				HandleDrop(static_cast<const char*>(payload->Data), mouseRay);
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
			std::string text = EditorLocale::Format("Loading {} assets...", loading);
			ImGui::GetWindowDrawList()->AddText(ImVec2(mImagePos.x + 10.0f, mImagePos.y + 8.0f), ImGui::ColorConvertFloat4ToU32(EditorStyle::kWarning), text.c_str());
		}
	}

	void SceneViewPanel::DrawToolbar()
	{
		ImGui::Checkbox(Tr("Grid"), &showGrid);
		ImGui::SameLine();
		ImGui::Checkbox(Tr("Gizmos"), &showGizmos);
		ImGui::SameLine();
		ImGui::Checkbox(Tr("Physics"), &showPhysics);
		ImGui::SameLine();
		ImGui::Checkbox(Tr("NavMesh"), &showNavMesh);
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", Tr("Always show the NavMesh (it is also shown while a NavMeshSurface is selected)")); }
		EditorGui::ToolbarSeparator();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", Tr("Speed"));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 4.0f);
		ImGui::DragFloat("##speed", &mCamera.moveSpeed, 0.1f, 0.5f, 500.0f, "%.1f");
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", Tr("Camera speed (mouse wheel while flying)")); }
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
		if (mGizmoWasUsing && !usingGizmo) { mEditor.CommitActorEdit(Tr("Transform")); }
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

	// マウスの下のモデルと、そのどの部分（サブメッシュ）かを探す
	MeshRenderer* SceneViewPanel::PickRenderer(const Ray& ray, int& outSubMesh) const
	{
		MeshRenderer* best = nullptr;
		float bestDist = math::kInfinity;
		outSubMesh = -1;
		mEditor.GetScene().ForEachActor([&](Actor* actor)
			{
				if (!actor->IsActive()) { return; }
				for (MeshRenderer* mr : actor->GetComponents<MeshRenderer>())
				{
					float d;
					int sub;
					if (mr->IsEnabled() && mr->Raycast(ray, d, &sub) && d < bestDist)
					{
						bestDist = d;
						best = mr;
						outSubMesh = sub;
					}
				}
			});
		return best;
	}

	// Unity と同じく、マウスの下の部分が使っている枠にだけ入れる（分からなければ最初の枠）
	void SceneViewPanel::DropMaterial(const std::string& path, const Ray& ray)
	{
		int sub = -1;
		MeshRenderer* target = PickRenderer(ray, sub);
		if (!target)
		{
			Log::Warning(LogCat::Editor, "Drop the material onto a model to assign it");
			return;
		}
		size_t slot = 0;
		const auto& model = target->GetModel();
		if (sub >= 0 && model && model->IsReady() && size_t(sub) < model->GetSubMeshes().size())
		{
			slot = std::min<size_t>(model->GetSubMeshes()[size_t(sub)].materialIndex, target->GetMaterialCount() - 1);
		}
		mEditor.AssignMaterial(*target, slot, path);
	}

	namespace
	{
		// a が root そのものか、root の子孫か
		bool IsInside(const Actor* a, const Actor* root)
		{
			for (; a; a = a->GetParent()) { if (a == root) { return true; } }
			return false;
		}
	}

	bool SceneViewPanel::CanPlace(const std::string& path)
	{
		std::string ext = GetExtension(path);
		return ext == ".prefab" || Model::IsSupportedExtension(ext);
	}

	Actor* SceneViewPanel::SpawnAsset(const std::string& path)
	{
		Scene& scene = mEditor.GetScene();
		if (GetExtension(path) == ".prefab") { return scene.Instantiate(path); }
		Actor* actor = scene.CreateActor(GetStem(path));
		actor->AddComponent<MeshRenderer>()->SetModel(path);
		return actor;
	}

	Vector3 SceneViewPanel::FindPlacement(const Ray& ray, const Actor* ignore) const
	{
		Scene& scene = mEditor.GetScene();
		float best = 500.0f;
		bool found = false;

		// 見えているモデルの三角形（コライダーのない置物の上にも置けるように）
		scene.ForEachActor([&](Actor* actor)
			{
				if (!actor->IsActive() || IsInside(actor, ignore)) { return; }
				for (MeshRenderer* mr : actor->GetComponents<MeshRenderer>())
				{
					float d;
					if (mr->IsEnabled() && mr->Raycast(ray, d) && d < best) { best = d; found = true; }
				}
			});

		// コライダー。自分（仮のアクター）に当たったら、その少し先から撃ち直す
		PhysicsWorld& physics = scene.GetSystem<PhysicsWorld>();
		Ray r = ray;
		float travelled = 0.0f;
		for (int i = 0; i < 8 && travelled < best; ++i)
		{
			RaycastHit hit;
			if (!physics.Raycast(r, best - travelled, hit)) { break; }
			if (!IsInside(hit.collider->GetActor(), ignore))
			{
				best = travelled + hit.distance;
				found = true;
				break;
			}
			float step = hit.distance + 0.01f;
			travelled += step;
			r.origin = r.GetPoint(step);
		}

		if (found) { return ray.GetPoint(best); }
		if (ray.direction.y < -0.01f) { return ray.GetPoint(-ray.origin.y / ray.direction.y); }
		return ray.GetPoint(10.0f);
	}

	Actor* SceneViewPanel::GetDragPreview() const
	{
		return mPreviewId ? mEditor.GetScene().FindActor(mPreviewId) : nullptr;
	}

	// 運んでいるものが変わったら作り直す。仮のアクターは Undo にも選択にも入れない（まだシーンの変更ではない）
	void SceneViewPanel::UpdateDragPreview(const std::string& path, const Ray& ray)
	{
		Actor* preview = GetDragPreview();
		if (preview && mPreviewPath != path)
		{
			ClearDragPreview();
			preview = nullptr;
		}
		if (!preview)
		{
			preview = SpawnAsset(path);
			if (!preview) { return; }
			mPreviewId = preview->GetId();
			mPreviewPath = path;
		}
		preview->GetTransform().SetPosition(FindPlacement(ray, preview));
	}

	void SceneViewPanel::ClearDragPreview()
	{
		if (Actor* preview = GetDragPreview())
		{
			Scene& scene = mEditor.GetScene();
			preview->Destroy();
			scene.FlushPendingDestroy();
		}
		mPreviewId = 0;
		mPreviewPath.clear();
	}

	// 運んでいる間に出していた仮のアクターがあれば、それをそのまま確定する（見えていた位置に置かれる）
	void SceneViewPanel::HandleDrop(const std::string& path, const Ray& ray)
	{
		std::string ext = GetExtension(path);
		if (ext == ".scene")
		{
			mEditor.OpenScene(path);
			return;
		}
		Scene& scene = mEditor.GetScene();
		if (MaterialAsset::IsMaterialPath(path))
		{
			DropMaterial(path, ray);
			return;
		}
		if (!CanPlace(path))
		{
			Log::Warning(LogCat::Editor, "Cannot drop '{}' into the scene view", path);
			return;
		}

		Actor* actor = mPreviewPath == path ? GetDragPreview() : nullptr;
		if (actor)
		{
			// 確定したので、Update で片付けられないよう仮のアクターの印を外す
			mPreviewId = 0;
			mPreviewPath.clear();
		}
		else
		{
			ClearDragPreview();
			actor = SpawnAsset(path);
			if (!actor) { return; }
			actor->GetTransform().SetPosition(FindPlacement(ray, actor));
		}
		mEditor.Select(actor);
		mEditor.PushCommand(ActorLifetimeCommand::Created(EditorLocale::Format("Drop {}", GetFileName(path)), scene, { actor }));
	}
}
