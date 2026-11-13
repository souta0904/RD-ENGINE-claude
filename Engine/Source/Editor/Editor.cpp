#include "RdPch.h"
#include "Editor/Editor.h"
#include "Asset/AssetManager.h"
#include "Asset/Model.h"
#include "Components/CameraComponent.h"
#include "Components/Lights.h"
#include "Components/MeshRenderer.h"
#include "Components/ParticleSystem.h"
#include "Core/StringUtil.h"
#include "Engine.h"
#include "Physics/Collider.h"
#include "Physics/PhysicsWorld.h"
#include "Platform/Input.h"
#include "Render/DebugDraw.h"
#include "Render/Renderer.h"
#include "Scene/Scene.h"
#include <commdlg.h>
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/ImGuizmo.h>

#pragma comment(lib, "comdlg32.lib")

namespace rd
{
	namespace
	{
		// Win32 のファイルダイアログ（カレントディレクトリからの相対パスを返す）
		std::string FileDialog(HWND owner, bool save, const wchar_t* filter, const wchar_t* defExt)
		{
			wchar_t buffer[MAX_PATH] = {};
			OPENFILENAMEW ofn = {};
			ofn.lStructSize = sizeof(ofn);
			ofn.hwndOwner = owner;
			ofn.lpstrFilter = filter;
			ofn.lpstrFile = buffer;
			ofn.nMaxFile = MAX_PATH;
			ofn.lpstrDefExt = defExt;
			std::wstring initial = std::filesystem::absolute(L"Assets").wstring();
			ofn.lpstrInitialDir = initial.c_str();
			ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
			BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
			if (!ok) { return {}; }
			std::error_code ec;
			auto rel = std::filesystem::relative(buffer, std::filesystem::current_path(), ec);
			return NormalizePath(ToUtf8(ec ? std::wstring(buffer) : rel.wstring()));
		}

		bool IsModelFile(const std::string& ext)
		{
			return ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx" || ext == ".dae";
		}
	}

	// ==================================================
	// 初期化
	// ==================================================
	void Editor::Initialize()
	{
		std::filesystem::create_directories("Logs");
		mImGui.Initialize(mEngine.GetWindow().GetHandle());
		// レイアウト保存ファイルがあればそれを使う
		mFirstLayout = !std::filesystem::exists("Logs/imgui.ini");
	}

	void Editor::Shutdown()
	{
		mImGui.Shutdown();
	}

	bool Editor::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		return mImGui.HandleMessage(hwnd, msg, wParam, lParam);
	}

	void Editor::BeginFrame()
	{
		mImGui.BeginFrame();
	}

	void Editor::OnSceneChanged(bool keepSelection)
	{
		// プレイの停止（keepSelection）では同じ ID のシーンに戻るので履歴は残す。別のシーンを開いたら捨てる
		if (!keepSelection)
		{
			mSelectedId = 0;
			mHistory.Clear();
		}
		RefreshSnapshots();
		UpdateTitle();
	}

	Actor* Editor::GetSelected() const
	{
		return mSelectedId ? mEngine.GetScene().FindActor(mSelectedId) : nullptr;
	}

	void Editor::Select(Actor* actor)
	{
		mSelectedId = actor ? actor->GetId() : 0;
		RefreshSnapshots();
	}

	// ==================================================
	// Undo / Redo
	// ==================================================
	void Editor::RefreshSnapshots()
	{
		Scene& scene = mEngine.GetScene();
		Actor* actor = GetSelected();
		mSnapshotActor = actor ? actor->GetId() : 0;
		mActorSnapshot = actor ? scene.SerializeActorState(actor) : Json();
		mSettingsSnapshot = scene.SerializeSettings();
	}

	void Editor::CommitActorEdit(const std::string& label)
	{
		// プレイ中の変更は停止時に元に戻るので履歴に残さない
		if (mEngine.IsPlaying()) { return; }
		Actor* actor = GetSelected();
		if (!actor || actor->GetId() != mSnapshotActor) { RefreshSnapshots(); return; }
		Json after = mEngine.GetScene().SerializeActorState(actor);
		if (after == mActorSnapshot) { return; }
		PushCommand(std::make_unique<ActorStateCommand>(label, actor->GetId(), std::move(mActorSnapshot), after));
	}

	void Editor::CommitSettingsEdit(const std::string& label)
	{
		if (mEngine.IsPlaying()) { return; }
		Json after = mEngine.GetScene().SerializeSettings();
		if (after == mSettingsSnapshot) { return; }
		PushCommand(std::make_unique<SceneSettingsCommand>(label, std::move(mSettingsSnapshot), after));
	}

	void Editor::PushCommand(std::unique_ptr<EditorCommand> command)
	{
		if (mEngine.IsPlaying()) { return; }
		mHistory.Push(std::move(command));
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::Undo()
	{
		if (mEngine.IsPlaying() || !mHistory.CanUndo()) { return; }
		mSelectedId = mHistory.Undo(mEngine.GetScene());
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::Redo()
	{
		if (mEngine.IsPlaying() || !mHistory.CanRedo()) { return; }
		mSelectedId = mHistory.Redo(mEngine.GetScene());
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::UpdateTitle()
	{
		mEngine.SetWindowTitleSuffix(mHistory.IsDirty() ? " *" : "");
	}

	void Editor::GetViewportSize(uint32_t& width, uint32_t& height) const
	{
		width = std::max(1u, mViewportWidth);
		height = std::max(1u, mViewportHeight);
	}

	bool Editor::UseEditorCamera() const
	{
		return !mEngine.IsPlaying() || mSceneCameraInPlay;
	}

	bool Editor::OverrideCamera(float aspect, RenderCamera& camera) const
	{
		if (!UseEditorCamera()) { return false; }
		camera = mCamera.GetRenderCamera(aspect);
		return true;
	}

	Scene& Editor::GetScene() const
	{
		return mEngine.GetScene();
	}

	// ==================================================
	// 毎フレーム
	// ==================================================
	void Editor::Update(float dt)
	{
		// 統計用
		mFrameTimes.push_back(dt * 1000.0f);
		if (mFrameTimes.size() > 240) { mFrameTimes.erase(mFrameTimes.begin()); }

		// 全画面のドッキング領域
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGuiID dockId = ImGui::DockSpaceOverViewport(vp, ImGuiDockNodeFlags_PassthruCentralNode);
		if (mFirstLayout)
		{
			mFirstLayout = false;
			ImGui::DockBuilderRemoveNode(dockId);
			ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
			ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
			ImGuiID center = dockId;
			ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
			ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.26f, nullptr, &center);
			ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
			ImGui::DockBuilderDockWindow("Hierarchy", left);
			ImGui::DockBuilderDockWindow("Inspector", right);
			ImGui::DockBuilderDockWindow("Render Settings", right);
			ImGui::DockBuilderDockWindow("Stats", right);
			ImGui::DockBuilderDockWindow("Assets", bottom);
			ImGui::DockBuilderDockWindow("Console", bottom);
			ImGui::DockBuilderDockWindow("Viewport", center);
			ImGui::DockBuilderFinish(dockId);
		}

		DrawMenuBar();
		DrawViewport();
		if (mShowHierarchy) { DrawHierarchy(); }
		if (mShowInspector) { DrawInspector(); }
		if (mShowAssets) { DrawAssets(); }
		if (mShowConsole) { DrawConsole(); }
		if (mShowStats) { DrawStats(); }
		if (mShowRenderSettings) { DrawRenderSettings(); }
		if (mShowDemo) { ImGui::ShowDemoWindow(&mShowDemo); }
		HandleShortcuts();

		// エディタカメラ
		if (UseEditorCamera()) { mCamera.Update(dt, mViewportHovered, mViewportFocused); }

		// ゲームへの入力はビューポートにフォーカスがあるときだけ
		ImGuiIO& io = ImGui::GetIO();
		bool gameHasFocus = mEngine.IsPlaying() && mViewportFocused && !mCamera.IsControlling();
		Input::SetBlocked(!gameHasFocus || io.WantTextInput, !gameHasFocus);
	}

	void Editor::DrawGizmos(DebugDraw& draw)
	{
		bool playView = !UseEditorCamera();
		if (mShowGrid && !playView) { draw.Grid(50.0f, 1.0f, Color(0.5f, 0.5f, 0.5f, 0.25f)); }
		Scene& scene = mEngine.GetScene();
		if (mShowGizmos && !playView) { scene.DrawGizmos(draw, GetSelected()); }
		else if (Actor* sel = GetSelected(); sel && !playView)
		{
			draw.Axes(sel->GetTransform().GetWorldMatrix(), 1.0f);
		}
		if (mShowPhysics)
		{
			for (auto* c : scene.FindComponents<Collider>()) { c->OnDrawGizmos(draw, false); }
		}
	}

	void Editor::RenderUI()
	{
		mImGui.Render();
	}

	// ==================================================
	// メニュー
	// ==================================================
	void Editor::DrawMenuBar()
	{
		Engine& engine = mEngine;
		if (!ImGui::BeginMainMenuBar()) { return; }
		if (ImGui::BeginMenu("File"))
		{
			if (ImGui::MenuItem("New Scene")) { engine.NewScene(); }
			if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) { OpenScene(); }
			if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { SaveScene(false); }
			if (ImGui::MenuItem("Save Scene As...")) { SaveScene(true); }
			ImGui::Separator();
			if (ImGui::MenuItem("Reload Shaders", "Ctrl+R")) { Renderer::Get().ReloadShaders(); }
			if (ImGui::MenuItem("Unload Unused Assets"))
			{
				Log::Info(LogCat::Editor, "Unloaded {} assets", AssetManager::Get().CollectGarbage());
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Exit")) { engine.Quit(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Edit"))
		{
			std::string undo = "Undo " + mHistory.GetUndoLabel();
			std::string redo = "Redo " + mHistory.GetRedoLabel();
			if (ImGui::MenuItem(undo.c_str(), "Ctrl+Z", false, mHistory.CanUndo())) { Undo(); }
			if (ImGui::MenuItem(redo.c_str(), "Ctrl+Y", false, mHistory.CanRedo())) { Redo(); }
			ImGui::Separator();
			if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, GetSelected() != nullptr)) { DuplicateSelected(); }
			if (ImGui::MenuItem("Delete", "Del", false, GetSelected() != nullptr)) { DeleteSelected(); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Actor"))
		{
			for (const char* kind : { "Empty", "Cube", "Sphere", "Plane", "Capsule", "Directional Light", "Point Light",
				"Spot Light", "Camera", "Particle System" })
			{
				if (ImGui::MenuItem(kind)) { CreateActorFromMenu(kind); }
			}
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("View"))
		{
			ImGui::MenuItem("Hierarchy", nullptr, &mShowHierarchy);
			ImGui::MenuItem("Inspector", nullptr, &mShowInspector);
			ImGui::MenuItem("Assets", nullptr, &mShowAssets);
			ImGui::MenuItem("Console", nullptr, &mShowConsole);
			ImGui::MenuItem("Stats", nullptr, &mShowStats);
			ImGui::MenuItem("Render Settings", nullptr, &mShowRenderSettings);
			ImGui::Separator();
			ImGui::MenuItem("Grid", nullptr, &mShowGrid);
			ImGui::MenuItem("Gizmos", nullptr, &mShowGizmos);
			ImGui::MenuItem("Physics Shapes", nullptr, &mShowPhysics);
			ImGui::Separator();
			{
				Window& window = engine.GetWindow();
				if (ImGui::MenuItem("Fullscreen", "F11", window.IsFullscreen())) { window.ToggleFullscreen(); }
			}
			if (ImGui::MenuItem("Reset Layout")) { mFirstLayout = true; }
			ImGui::MenuItem("ImGui Demo", nullptr, &mShowDemo);
			ImGui::EndMenu();
		}

		// 右寄せで FPS
		std::string fps = std::format("{:.0f} FPS  {:.2f} ms", engine.GetFps(), engine.GetFrameMs());
		ImGui::SameLine(ImGui::GetWindowWidth() - ImGui::CalcTextSize(fps.c_str()).x - 16.0f);
		ImGui::TextDisabled("%s", fps.c_str());
		ImGui::EndMainMenuBar();
	}

	void Editor::DrawToolbar()
	{
		Engine& engine = mEngine;
		bool playing = engine.IsPlaying();

		// プレイ操作
		ImGui::PushStyleColor(ImGuiCol_Button, playing ? ImVec4(0.55f, 0.25f, 0.2f, 1.0f) : ImVec4(0.2f, 0.45f, 0.25f, 1.0f));
		if (ImGui::Button(playing ? "Stop" : "Play", ImVec2(60, 0))) { playing ? engine.EndPlay() : engine.BeginPlay(); }
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::BeginDisabled(!playing);
		if (ImGui::Button(engine.IsPaused() ? "Resume" : "Pause", ImVec2(60, 0))) { engine.SetPaused(!engine.IsPaused()); }
		ImGui::SameLine();
		if (ImGui::Button("Step")) { engine.SetPaused(true); engine.StepFrame(); }
		ImGui::EndDisabled();

		// ギズモ
		ImGui::SameLine(0.0f, 20.0f);
		const char* ops[] = { "Move (W)", "Rotate (E)", "Scale (R)" };
		for (int i = 0; i < 3; ++i)
		{
			if (i > 0) { ImGui::SameLine(); }
			bool active = mGizmoOp == i;
			if (active) { ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive)); }
			if (ImGui::Button(ops[i])) { mGizmoOp = i; }
			if (active) { ImGui::PopStyleColor(); }
		}
		ImGui::SameLine();
		if (ImGui::Button(mGizmoLocal ? "Local" : "World")) { mGizmoLocal = !mGizmoLocal; }
		ImGui::SameLine(0.0f, 20.0f);
		ImGui::Checkbox("Grid", &mShowGrid);
		ImGui::SameLine();
		ImGui::Checkbox("Gizmos", &mShowGizmos);
		if (playing)
		{
			ImGui::SameLine();
			ImGui::Checkbox("Scene Camera", &mSceneCameraInPlay);
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80.0f);
		ImGui::DragFloat("Cam Speed", &mCamera.moveSpeed, 0.1f, 0.5f, 500.0f, "%.1f");
	}

	// ==================================================
	// ビューポート
	// ==================================================
	void Editor::DrawViewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
		ImGui::Begin("Viewport", nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
		ImGui::PopStyleVar();
		DrawToolbar();

		ImVec2 avail = ImGui::GetContentRegionAvail();
		mViewportWidth = uint32_t(std::max(1.0f, avail.x));
		mViewportHeight = uint32_t(std::max(1.0f, avail.y));
		ImVec2 pos = ImGui::GetCursorScreenPos();
		mViewportX = pos.x;
		mViewportY = pos.y;

		const auto& output = Renderer::Get().GetOutput();
		if (output.GetResource())
		{
			ImGui::Image(ImTextureID(output.GetGpuSrv().ptr), avail);
		}
		else
		{
			ImGui::Dummy(avail);
		}
		mViewportHovered = ImGui::IsItemHovered();
		mViewportFocused = ImGui::IsWindowFocused();
		// 右クリックでカメラを操作し始めたらフォーカスを移す
		if (mViewportHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
		{
			ImGui::SetWindowFocus();
		}

		float aspect = avail.x / std::max(avail.y, 1.0f);
		RenderCamera cam = UseEditorCamera() ? mCamera.GetRenderCamera(aspect) : RenderCamera();
		if (!UseEditorCamera())
		{
			if (auto main = CameraComponent::FindMain(mEngine.GetScene())) { cam = main->GetRenderCamera(aspect); }
		}

		// ドラッグ&ドロップ（モデル・プレハブ・シーン）
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				ImVec2 m = ImGui::GetMousePos();
				Vector2 uv((m.x - pos.x) / avail.x, (m.y - pos.y) / avail.y);
				HandleViewportDrop(static_cast<const char*>(payload->Data), cam.ScreenPointToRay(uv));
			}
			ImGui::EndDragDropTarget();
		}

		// ギズモ
		Actor* selected = GetSelected();
		bool usingGizmo = false;
		if (selected && UseEditorCamera())
		{
			ImGuizmo::SetOrthographic(false);
			ImGuizmo::SetDrawlist();
			ImGuizmo::SetRect(pos.x, pos.y, avail.x, avail.y);
			Matrix4 world = selected->GetTransform().GetWorldMatrix();
			ImGuizmo::OPERATION op = mGizmoOp == 0 ? ImGuizmo::TRANSLATE : (mGizmoOp == 1 ? ImGuizmo::ROTATE : ImGuizmo::SCALE);
			ImGuizmo::MODE mode = (mGizmoLocal || mGizmoOp == 2) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
			// Ctrl でスナップ
			float snapValues[3] = { 0.5f, 0.5f, 0.5f };
			if (mGizmoOp == 1) { snapValues[0] = snapValues[1] = snapValues[2] = 15.0f; }
			if (mGizmoOp == 2) { snapValues[0] = snapValues[1] = snapValues[2] = 0.1f; }
			bool snap = Input::GetKeyRaw(Key::Control);
			if (ImGuizmo::Manipulate(cam.view.Data(), cam.proj.Data(), op, mode, world.Data(), nullptr, snap ? snapValues : nullptr))
			{
				selected->GetTransform().SetWorldMatrix(world);
			}
			usingGizmo = ImGuizmo::IsUsing();
			if (mGizmoWasUsing && !usingGizmo) { CommitActorEdit("Transform"); }
			mGizmoWasUsing = usingGizmo;
		}

		// クリックで選択
		if (mViewportHovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGuizmo::IsOver() && !usingGizmo &&
			!Input::GetKeyRaw(Key::Alt) && UseEditorCamera())
		{
			ImVec2 m = ImGui::GetMousePos();
			Vector2 uv((m.x - pos.x) / avail.x, (m.y - pos.y) / avail.y);
			Select(Pick(cam.ScreenPointToRay(uv)));
		}

		// 読み込み中の表示
		uint32_t loading = AssetManager::Get().GetLoadingCount();
		if (loading > 0)
		{
			ImGui::SetCursorScreenPos(ImVec2(pos.x + 10, pos.y + 10));
			ImGui::TextColored(ImVec4(1, 0.8f, 0.3f, 1), "Loading %u assets...", loading);
		}
		if (mEngine.IsPlaying())
		{
			ImGui::GetWindowDrawList()->AddRect(pos, ImVec2(pos.x + avail.x, pos.y + avail.y),
				mEngine.IsPaused() ? IM_COL32(230, 180, 60, 255) : IM_COL32(60, 200, 90, 255), 0.0f, 0, 2.0f);
		}
		ImGui::End();
	}

	Actor* Editor::Pick(const Ray& ray)
	{
		Scene& scene = mEngine.GetScene();
		Actor* best = nullptr;
		float bestDist = math::kInfinity;
		scene.ForEachActor([&](Actor* actor)
			{
				if (!actor->IsActive()) { return; }
				float d;
				if (auto mr = actor->GetComponent<MeshRenderer>(); mr && mr->Raycast(ray, d) && d < bestDist)
				{
					bestDist = d;
					best = actor;
				}
				// メッシュを持たないもの（ライト・カメラなど）は小さな球で判定
				if (!actor->GetComponent<MeshRenderer>())
				{
					WorldShape s;
					s.type = ShapeType::Sphere;
					s.center = actor->GetTransform().GetPosition();
					s.radius = 0.35f;
					Vector3 n;
					if (RaycastShape(s, ray, bestDist, d, n) && d < bestDist)
					{
						bestDist = d;
						best = actor;
					}
				}
			});
		// コライダーにも当てる
		RaycastHit hit;
		if (scene.GetSystem<PhysicsWorld>().Raycast(ray, bestDist, hit, ~0u, true) && hit.distance < bestDist)
		{
			best = hit.collider->GetActor();
		}
		return best;
	}

	void Editor::HandleViewportDrop(const std::string& path, const Ray& ray)
	{
		Engine& engine = mEngine;
		Scene& scene = engine.GetScene();
		std::string ext = GetExtension(path);
		if (ext == ".scene")
		{
			engine.LoadScene(path);
			return;
		}
		// 置く位置: 何かに当たればそこ、なければ地面（y=0）か前方
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
			Log::Warning(LogCat::Editor, "Cannot drop '{}' into the viewport", path);
			return;
		}
		if (actor)
		{
			actor->GetTransform().SetPosition(place);
			Select(actor);
			PushCommand(ActorLifetimeCommand::Created("Drop " + GetFileName(path), scene, { actor }));
		}
	}

	// ==================================================
	// ショートカット
	// ==================================================
	void Editor::HandleShortcuts()
	{
		ImGuiIO& io = ImGui::GetIO();
		if (io.WantTextInput) { return; }
		bool ctrl = io.KeyCtrl;
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) { Undo(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) { Redo(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S, false)) { SaveScene(false); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O, false)) { OpenScene(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_D, false)) { DuplicateSelected(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_R, false)) { Renderer::Get().ReloadShaders(); }
		if (ctrl && ImGui::IsKeyPressed(ImGuiKey_P, false))
		{
			mEngine.IsPlaying() ? mEngine.EndPlay() : mEngine.BeginPlay();
		}
		if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && (mViewportFocused || ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow)))
		{
			DeleteSelected();
		}
		// ギズモ切り替え・フォーカス（カメラ操作中は WASD と衝突するので無効）
		if (!mCamera.IsControlling() && !ctrl && (mViewportFocused || mViewportHovered))
		{
			if (ImGui::IsKeyPressed(ImGuiKey_W, false)) { mGizmoOp = 0; }
			if (ImGui::IsKeyPressed(ImGuiKey_E, false)) { mGizmoOp = 1; }
			if (ImGui::IsKeyPressed(ImGuiKey_R, false)) { mGizmoOp = 2; }
			if (ImGui::IsKeyPressed(ImGuiKey_F, false))
			{
				if (Actor* sel = GetSelected())
				{
					auto mr = sel->GetComponent<MeshRenderer>();
					mCamera.Focus(mr ? mr->GetWorldBounds() : AABB::FromCenterExtents(sel->GetTransform().GetPosition(), Vector3(1, 1, 1)));
				}
			}
		}
	}

	// ==================================================
	// 操作
	// ==================================================
	Actor* Editor::CreatePrimitive(const std::string& name, const std::string& model)
	{
		Scene& scene = mEngine.GetScene();
		Actor* actor = scene.CreateActor(name);
		actor->AddComponent<MeshRenderer>()->SetModel(model);
		return actor;
	}

	Actor* Editor::CreateActorFromMenu(const std::string& kind)
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

		// カメラの前に置く
		RenderCamera cam = mCamera.GetRenderCamera(1.0f);
		if (kind != "Directional Light") { actor->GetTransform().SetPosition(cam.position + cam.GetForward() * 6.0f); }
		Select(actor);
		PushCommand(ActorLifetimeCommand::Created("Create " + kind, scene, { actor }));
		return actor;
	}

	void Editor::DeleteSelected()
	{
		if (Actor* sel = GetSelected())
		{
			Scene& scene = mEngine.GetScene();
			// 消す前に中身を覚えておく
			auto command = ActorLifetimeCommand::Deleting("Delete " + sel->GetName(), scene, { sel });
			sel->Destroy();
			scene.FlushPendingDestroy();
			Select(nullptr);
			PushCommand(std::move(command));
		}
	}

	void Editor::DuplicateSelected()
	{
		if (Actor* sel = GetSelected())
		{
			Scene& scene = mEngine.GetScene();
			if (Actor* copy = scene.Duplicate(sel))
			{
				Select(copy);
				PushCommand(ActorLifetimeCommand::Created("Duplicate " + sel->GetName(), scene, { copy }));
			}
		}
	}

	void Editor::SaveScene(bool saveAs)
	{
		if (mEngine.IsPlaying())
		{
			Log::Warning(LogCat::Editor, "Stop play mode before saving");
			return;
		}
		Scene& scene = mEngine.GetScene();
		std::string path = scene.GetPath();
		if (saveAs || path.empty())
		{
			path = FileDialog(mEngine.GetWindow().GetHandle(), true, L"Scene (*.scene)\0*.scene\0", L"scene");
			if (path.empty()) { return; }
		}
		if (scene.SaveToFile(path))
		{
			mHistory.MarkSaved();
			UpdateTitle();
		}
	}

	void Editor::OpenScene()
	{
		std::string path = FileDialog(mEngine.GetWindow().GetHandle(), false, L"Scene (*.scene)\0*.scene\0", L"scene");
		if (!path.empty())
		{
			if (mEngine.IsPlaying()) { mEngine.EndPlay(); }
			mEngine.LoadScene(path);
		}
	}
}
