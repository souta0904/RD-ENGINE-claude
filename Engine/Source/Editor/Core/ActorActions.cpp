#include "RdPch.h"
#include "Editor/Core/ActorActions.h"
#include "Asset/Model/Model.h"
#include "Components/Effects/ParticleSystem.h"
#include "Components/Lighting/DirectionalLight.h"
#include "Components/Lighting/PointLight.h"
#include "Components/Lighting/SpotLight.h"
#include "Components/Rendering/CameraComponent.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Commands/ActorLifetimeCommand.h"
#include "Editor/Commands/ActorStateCommand.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Editor.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Physics/Colliders/BoxCollider.h"
#include "Physics/Colliders/CapsuleCollider.h"
#include "Physics/Colliders/SphereCollider.h"
#include "Scene/Scene.h"
#include "Scene/Serialization/SceneSerializer.h"
#include <ImGui/imgui.h>

namespace rd
{
	namespace
	{
		// ファイル名に使えない文字は _ にする
		std::string SafeFileName(std::string name)
		{
			for (char& ch : name)
			{
				if (std::string_view("\\/:*?\"<>|").find(ch) != std::string_view::npos || static_cast<unsigned char>(ch) < 0x20) { ch = '_'; }
			}
			return name.empty() ? std::string("Prefab") : name;
		}
	}

	Actor* ActorActions::CreatePrimitive(const std::string& name, const std::string& model)
	{
		Actor* actor = mEditor.GetScene().CreateActor(name);
		actor->AddComponent<MeshRenderer>()->SetModel(model);
		return actor;
	}

	Actor* ActorActions::CreateActor(const std::string& kind, Actor* parent)
	{
		Scene& scene = mEditor.GetScene();
		Actor* actor = nullptr;
		if (kind == "Empty") { actor = scene.CreateActor("Actor"); }
		else if (kind == "Cube") { actor = CreatePrimitive("Cube", "Engine/Cube"); actor->AddComponent<BoxCollider>(); }
		else if (kind == "Sphere") { actor = CreatePrimitive("Sphere", "Engine/Sphere"); actor->AddComponent<SphereCollider>(); }
		else if (kind == "Plane") { actor = CreatePrimitive("Plane", "Engine/Plane"); actor->AddComponent<BoxCollider>()->size = Vector3(10, 0.01f, 10); }
		else if (kind == "Capsule") { actor = CreatePrimitive("Capsule", "Engine/Capsule"); actor->AddComponent<CapsuleCollider>(); }
		else if (kind == "Directional Light")
		{
			actor = scene.CreateActor("Directional Light");
			actor->AddComponent<DirectionalLight>();
			actor->GetTransform().SetLocalRotation(Quaternion::FromEuler({ math::ToRadians(50.0f), math::ToRadians(-30.0f), 0.0f }));
		}
		else if (kind == "Point Light") { actor = scene.CreateActor("Point Light"); actor->AddComponent<PointLight>(); }
		else if (kind == "Spot Light") { actor = scene.CreateActor("Spot Light"); actor->AddComponent<SpotLight>(); }
		else if (kind == "Camera") { actor = scene.CreateActor("Camera"); actor->AddComponent<CameraComponent>(); }
		else if (kind == "Particle System") { actor = scene.CreateActor("Particles"); actor->AddComponent<ParticleSystem>(); }
		if (!actor) { return nullptr; }

		if (parent)
		{
			actor->SetParent(parent, false);
			actor->GetTransform().SetLocalPosition(Vector3(0, 0, 0));
		}
		else if (kind != "Directional Light")
		{
			RenderCamera cam = mEditor.GetSceneView().GetViewCamera(1.0f);
			actor->GetTransform().SetPosition(cam.position + cam.GetForward() * 6.0f);
		}
		mEditor.GetSelection().Select(actor);
		mEditor.GetUndo().Push(ActorLifetimeCommand::Created(EditorLocale::Format("Create {}", std::string(Tr(kind))), { actor }));
		return actor;
	}

	void ActorActions::DrawCreateMenu(Actor* parent)
	{
		ActorId parentId = parent ? parent->GetId() : 0;
		auto item = [&](const char* label)
			{
				if (ImGui::MenuItem(Tr(label)))
				{
					std::string kind = label;
					mEditor.Defer([this, kind, parentId] { CreateActor(kind, parentId ? mEditor.GetScene().FindActor(parentId) : nullptr); });
				}
			};
		item("Empty");
		ImGui::Separator();
		if (ImGui::BeginMenu(Tr("3D Object")))
		{
			for (const char* kind : { "Cube", "Sphere", "Plane", "Capsule" }) { item(kind); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Light")))
		{
			for (const char* kind : { "Directional Light", "Point Light", "Spot Light" }) { item(kind); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu(Tr("Effects")))
		{
			item("Particle System");
			ImGui::EndMenu();
		}
		item("Camera");
	}

	// 消す前に中身を覚えておく（Undo で戻せるように）
	void ActorActions::DeleteSelected()
	{
		Actor* sel = mEditor.GetSelection().Get();
		if (!sel) { return; }
		Scene& scene = mEditor.GetScene();
		auto command = ActorLifetimeCommand::Deleting(EditorLocale::Format("Delete {}", sel->GetName()), { sel });
		sel->Destroy();
		scene.FlushPendingDestroy();
		mEditor.GetSelection().Select(nullptr);
		mEditor.GetUndo().Push(std::move(command));
	}

	void ActorActions::DuplicateSelected()
	{
		Actor* sel = mEditor.GetSelection().Get();
		if (!sel) { return; }
		Scene& scene = mEditor.GetScene();
		if (Actor* copy = scene.Duplicate(sel))
		{
			mEditor.GetSelection().Select(copy);
			mEditor.GetUndo().Push(ActorLifetimeCommand::Created(EditorLocale::Format("Duplicate {}", sel->GetName()), { copy }));
		}
	}

	void ActorActions::CopySelected()
	{
		if (Actor* sel = mEditor.GetSelection().Get()) { mClipboard = SceneSerializer::SerializeActors({ sel }); }
	}

	// 選んでいるアクターの兄弟として（選んでいなければルートに）貼り付ける
	void ActorActions::PasteClipboard()
	{
		if (!HasClipboard()) { return; }
		Scene& scene = mEditor.GetScene();
		Actor* sel = mEditor.GetSelection().Get();
		Actor* parent = sel ? sel->GetParent() : nullptr;
		auto roots = SceneSerializer::DeserializeActors(scene, mClipboard, true, parent);
		if (roots.empty()) { return; }
		Actor* copy = roots.front();
		if (sel) { copy->SetParent(parent, false, sel->GetSiblingIndex() + 1); }
		mEditor.GetSelection().Select(copy);
		mEditor.GetUndo().Push(ActorLifetimeCommand::Created(EditorLocale::Format("Paste {}", copy->GetName()), roots));
	}

	void ActorActions::RenameSelected()
	{
		mEditor.GetHierarchy().BeginRename(mEditor.GetSelection().Get());
	}

	void ActorActions::FocusOn(Actor* actor)
	{
		if (!actor) { return; }
		auto mr = actor->GetComponent<MeshRenderer>();
		mEditor.GetSceneView().GetCamera().Focus(mr ? mr->GetWorldBounds() : AABB::FromCenterExtents(actor->GetTransform().GetPosition(), Vector3(1, 1, 1)));
		mEditor.FocusView("Scene");
	}

	// 上書きしないよう、同じ名前があれば " 1", " 2" … を付ける
	std::string ActorActions::CreatePrefab(Actor* actor, const std::string& dir)
	{
		if (!actor) { return {}; }
		std::string stem = SafeFileName(actor->GetName());
		std::string path = dir + "/" + stem + ".prefab";
		for (int n = 1; FileExists(path); ++n) { path = std::format("{}/{} {}.prefab", dir, stem, n); }
		if (!SceneSerializer::SavePrefab(actor, path))
		{
			Logger::Error(LogCat::Editor, "Failed to save prefab: {}", path);
			return {};
		}
		Logger::Info(LogCat::Editor, "Saved prefab: {}", path);
		return path;
	}

	// ヒエラルキーへのドロップ。プレハブはその中身を、モデルは MeshRenderer を付けたアクターを作る
	//  位置は Unity と同じく、ルートなら原点、親の中なら親の位置（ローカルの原点）
	Actor* ActorActions::InstantiateAsset(const std::string& path, Actor* parent, int siblingIndex)
	{
		Scene& scene = mEditor.GetScene();
		Actor* instance = nullptr;
		if (GetExtension(path) == ".prefab") { instance = scene.Instantiate(path, parent); }
		else if (Model::IsSupportedExtension(GetExtension(path)))
		{
			instance = scene.CreateActor(GetStem(path));
			instance->AddComponent<MeshRenderer>()->SetModel(path);
			if (parent) { instance->SetParent(parent, false); }
		}
		if (!instance) { return nullptr; }
		if (siblingIndex >= 0) { instance->SetParent(parent, false, siblingIndex); }
		mEditor.GetSelection().Select(instance);
		mEditor.GetUndo().Push(ActorLifetimeCommand::Created(EditorLocale::Format("Instantiate {}", GetStem(path)), { instance }));
		return instance;
	}

	// 入れる前後のアクターの状態を Undo に積む（アクターの値の変更と同じ扱い）
	bool ActorActions::AssignMaterial(MeshRenderer& renderer, size_t slot, const std::string& path)
	{
		Actor* actor = renderer.GetActor();
		Json before = SceneSerializer::SerializeActorState(actor);
		if (!renderer.SetSharedMaterial(slot, path))
		{
			Logger::Warning(LogCat::Editor, "{} has no material slot {} (is the model loaded?)", actor->GetName(), slot);
			return false;
		}
		Logger::Info(LogCat::Editor, "Assigned {} to {} (slot {})", path, actor->GetName(), slot);
		Json after = SceneSerializer::SerializeActorState(actor);
		if (after != before)
		{
			mEditor.GetUndo().Push(std::make_unique<ActorStateCommand>(EditorLocale::Format("Assign Material {}", GetStem(path)), actor->GetId(), std::move(before), std::move(after)));
		}
		return true;
	}
}
