#include "RdPch.h"
#include "Editor/Editor.h"
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
#include "Editor/Core/FileDialog.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Engine.h"
#include "Physics/Colliders/BoxCollider.h"
#include "Physics/Colliders/CapsuleCollider.h"
#include "Physics/Colliders/SphereCollider.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"
#include <ImGui/imgui.h>

namespace rd
{
	namespace
	{
		constexpr const wchar_t* kSceneFilter = L"Scene (*.scene)\0*.scene\0";

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

	Actor* Editor::CreatePrimitive(const std::string& name, const std::string& model)
	{
		Actor* actor = mEngine.GetScene().CreateActor(name);
		actor->AddComponent<MeshRenderer>()->SetModel(model);
		return actor;
	}

	Actor* Editor::CreateActor(const std::string& kind, Actor* parent)
	{
		Scene& scene = mEngine.GetScene();
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
			RenderCamera cam = mSceneView->GetViewCamera(1.0f);
			actor->GetTransform().SetPosition(cam.position + cam.GetForward() * 6.0f);
		}
		Select(actor);
		PushCommand(ActorLifetimeCommand::Created(EditorLocale::Format("Create {}", std::string(Tr(kind))), scene, { actor }));
		return actor;
	}

	void Editor::DrawCreateMenu(Actor* parent)
	{
		ActorId parentId = parent ? parent->GetId() : 0;
		auto item = [&](const char* label)
			{
				if (ImGui::MenuItem(Tr(label)))
				{
					std::string kind = label;
					Defer([this, kind, parentId] { CreateActor(kind, parentId ? GetScene().FindActor(parentId) : nullptr); });
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
	void Editor::DeleteSelected()
	{
		Actor* sel = GetSelected();
		if (!sel) { return; }
		Scene& scene = mEngine.GetScene();
		auto command = ActorLifetimeCommand::Deleting(EditorLocale::Format("Delete {}", sel->GetName()), scene, { sel });
		sel->Destroy();
		scene.FlushPendingDestroy();
		Select(nullptr);
		PushCommand(std::move(command));
	}

	void Editor::DuplicateSelected()
	{
		Actor* sel = GetSelected();
		if (!sel) { return; }
		Scene& scene = mEngine.GetScene();
		if (Actor* copy = scene.Duplicate(sel))
		{
			Select(copy);
			PushCommand(ActorLifetimeCommand::Created(EditorLocale::Format("Duplicate {}", sel->GetName()), scene, { copy }));
		}
	}

	void Editor::CopySelected()
	{
		if (Actor* sel = GetSelected()) { mClipboard = mEngine.GetScene().SerializeActors({ sel }); }
	}

	// 選んでいるアクターの兄弟として（選んでいなければルートに）貼り付ける
	void Editor::PasteClipboard()
	{
		if (!HasClipboard()) { return; }
		Scene& scene = mEngine.GetScene();
		Actor* sel = GetSelected();
		Actor* parent = sel ? sel->GetParent() : nullptr;
		auto roots = scene.DeserializeActors(mClipboard, true, parent);
		if (roots.empty()) { return; }
		Actor* copy = roots.front();
		if (sel) { copy->SetParent(parent, false, sel->GetSiblingIndex() + 1); }
		Select(copy);
		PushCommand(ActorLifetimeCommand::Created(EditorLocale::Format("Paste {}", copy->GetName()), scene, roots));
	}

	void Editor::RenameSelected()
	{
		mHierarchy->BeginRename(GetSelected());
	}

	void Editor::FocusOn(Actor* actor)
	{
		if (!actor) { return; }
		auto mr = actor->GetComponent<MeshRenderer>();
		mSceneView->GetCamera().Focus(mr ? mr->GetWorldBounds() : AABB::FromCenterExtents(actor->GetTransform().GetPosition(), Vector3(1, 1, 1)));
		mFocusView = "Scene";
	}

	// 上書きしないよう、同じ名前があれば " 1", " 2" … を付ける
	std::string Editor::CreatePrefab(Actor* actor, const std::string& dir)
	{
		if (!actor) { return {}; }
		std::string stem = SafeFileName(actor->GetName());
		std::string path = dir + "/" + stem + ".prefab";
		for (int n = 1; FileExists(path); ++n) { path = std::format("{}/{} {}.prefab", dir, stem, n); }
		if (!Scene::SavePrefab(actor, path))
		{
			Log::Error(LogCat::Editor, "Failed to save prefab: {}", path);
			return {};
		}
		Log::Info(LogCat::Editor, "Saved prefab: {}", path);
		return path;
	}

	// ヒエラルキーへのドロップ。プレハブはその中身を、モデルは MeshRenderer を付けたアクターを作る
	//  位置は Unity と同じく、ルートなら原点、親の中なら親の位置（ローカルの原点）
	Actor* Editor::InstantiateAsset(const std::string& path, Actor* parent, int siblingIndex)
	{
		Scene& scene = mEngine.GetScene();
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
		Select(instance);
		PushCommand(ActorLifetimeCommand::Created(EditorLocale::Format("Instantiate {}", GetStem(path)), scene, { instance }));
		return instance;
	}

	void Editor::NewScene()
	{
		RunAfterSaveCheck([this]
			{
				if (IsPlaying()) { mEngine.EndPlay(); }
				mEngine.NewScene();
			});
	}

	void Editor::OpenScene()
	{
		RunAfterSaveCheck([this]
			{
				std::string path = FileDialog::Open(mEngine.GetWindow().GetHandle(), kSceneFilter, L"scene");
				if (path.empty()) { return; }
				if (IsPlaying()) { mEngine.EndPlay(); }
				mEngine.LoadScene(path);
			});
	}

	void Editor::OpenScene(const std::string& path)
	{
		RunAfterSaveCheck([this, path]
			{
				if (IsPlaying()) { mEngine.EndPlay(); }
				mEngine.LoadScene(path);
			});
	}

	bool Editor::SaveScene(bool saveAs)
	{
		if (IsPlaying())
		{
			Log::Warning(LogCat::Editor, "Stop play mode before saving");
			return false;
		}
		Scene& scene = mEngine.GetScene();
		std::string path = scene.GetPath();
		if (saveAs || path.empty())
		{
			path = FileDialog::Save(mEngine.GetWindow().GetHandle(), kSceneFilter, L"scene");
			if (path.empty()) { return false; }
		}
		if (!scene.SaveToFile(path)) { return false; }
		mHistory.MarkSaved();
		UpdateTitle();
		return true;
	}

	void Editor::RequestQuit()
	{
		RunAfterSaveCheck([this]
			{
				mAllowClose = true;
				mEngine.Quit();
			});
	}

	void Editor::RunAfterSaveCheck(std::function<void()> action)
	{
		if (!mHistory.IsDirty())
		{
			action();
			return;
		}
		mPendingAfterSave = std::move(action);
		mOpenSavePrompt = true;
	}

	void Editor::TogglePlay()
	{
		IsPlaying() ? mEngine.EndPlay() : mEngine.BeginPlay();
	}

	void Editor::TogglePause()
	{
		if (IsPlaying()) { mEngine.SetPaused(!mEngine.IsPaused()); }
	}

	void Editor::StepFrame()
	{
		if (!IsPlaying()) { return; }
		mEngine.SetPaused(true);
		mEngine.StepFrame();
	}
}
