#include "RdPch.h"
#include "Editor/Editor.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Commands/SceneSettingsCommand.h"
#include "Editor/Commands/ActorStateCommand.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Core/EditorSession.h"
#include "Editor/Core/HotReload.h"
#include "Editor/Panels/GameViewPanel.h"
#include "Editor/Panels/LogPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/PropertiesPanel.h"
#include "Editor/Panels/LightingPanel.h"
#include "Editor/Panels/PreviewPanel.h"
#include "Editor/Panels/ProfilerPanel.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Editor/Panels/StatsPanel.h"
#include "Editor/Style/EditorStyle.h"
#include "Engine.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Components/Navigation/NavMeshSurface.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Physics/Colliders/Collider.h"
#include "Render/Debug/DebugDraw.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <fstream>

namespace rd
{
	namespace
	{
		constexpr const char* kLayoutFile = "Logs/editor_layout.ini";
		// 既定の配置の版（パネルを足したり名前を変えたりしたら上げる。保存してある版と違えば配置を作り直す）
		constexpr const char* kLayoutVersionFile = "Logs/editor_layout.version";
		//  3: ウィンドウの ID を「訳###英語」にした（言語を変えても配置が保たれるように）
		constexpr int kLayoutVersion = 3;

		int ReadLayoutVersion()
		{
			std::ifstream file(kLayoutVersionFile);
			int version = 0;
			file >> version;
			return version;
		}

		void WriteLayoutVersion()
		{
			std::ofstream file(kLayoutVersionFile);
			file << kLayoutVersion;
		}
	}

	Editor::Editor(Engine& engine) : mEngine(engine)
	{
		mSceneView = AddPanel(std::make_unique<SceneViewPanel>(*this));
		mGameView = AddPanel(std::make_unique<GameViewPanel>(*this));
		mHierarchy = AddPanel(std::make_unique<HierarchyPanel>(*this));
		mProperties = AddPanel(std::make_unique<PropertiesPanel>(*this));
		mProject = AddPanel(std::make_unique<ProjectPanel>(*this));
		AddPanel(std::make_unique<PreviewPanel>(*this));
		mLog = AddPanel(std::make_unique<LogPanel>(*this));
		AddPanel(std::make_unique<LightingPanel>(*this));
		AddPanel(std::make_unique<StatsPanel>(*this));
		mProfiler = AddPanel(std::make_unique<ProfilerPanel>(*this));
		mAssistant = AddPanel(std::make_unique<AIAssistant>(*this));
		mHotReload = std::make_unique<HotReload>(*this);
	}

	Editor::~Editor() = default;

	void Editor::Initialize()
	{
		std::filesystem::create_directories("Logs");
		mImGui.Initialize(mEngine.GetWindow().GetHandle());
		EditorStyle::ApplyDarkTitleBar(mEngine.GetWindow().GetHandle());
		mEngine.GetWindow().EnableFileDrop();
		ImGui::GetIO().IniFilename = kLayoutFile;
		// 前回の配置が保存されていればそれを使う（既定の配置の版が古ければ作り直す）
		mFirstLayout = !std::filesystem::exists(kLayoutFile) || ReadLayoutVersion() != kLayoutVersion;
	}

	void Editor::Shutdown()
	{
		EditorLocale::WriteMissing();
		mImGui.Shutdown();
	}

	// ウィンドウを閉じようとしたとき、保存していない変更があれば先に聞く
	bool Editor::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CLOSE && !mAllowClose && mHistory.IsDirty())
		{
			RequestQuit();
			return true;
		}
		return mImGui.HandleMessage(hwnd, msg, wParam, lParam);
	}

	void Editor::BeginFrame()
	{
		mImGui.BeginFrame();
	}

	// プレイを止めて戻ったとき（keepSelection）は同じ ID のシーンに戻るので履歴を残し、別のシーンを開いたら捨てる
	void Editor::OnSceneChanged(bool keepSelection)
	{
		if (!keepSelection)
		{
			mSelectedId = 0;
			mHistory.Clear();
		}
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::OnGameCodeReloaded()
	{
		OnSceneChanged(true);
		for (auto& panel : mPanels) { panel->OnGameCodeReloaded(); }
	}

	void Editor::CaptureSession(EditorSession& session) const
	{
		// プレイ中にゲームが別の場面へ切り替えていたら、選択は編集中のシーンのものではないので残さない
		session.selectedActor = GetScene().GetPath() == session.scene ? mSelectedId : 0;

		const EditorCamera& camera = mSceneView->GetCamera();
		session.hasCamera = true;
		session.cameraPosition = camera.position;
		session.cameraYaw = camera.yaw;
		session.cameraPitch = camera.pitch;
		session.cameraFov = camera.fov;
		session.cameraMoveSpeed = camera.moveSpeed;

		session.gizmoOp = int(mSceneView->GetGizmoOp());
		session.gizmoLocal = mSceneView->IsGizmoLocal();
		session.showGrid = mSceneView->showGrid;
		session.showGizmos = mSceneView->showGizmos;
		session.showPhysics = mSceneView->showPhysics;
		session.showNavMesh = mSceneView->showNavMesh;
		// プレイ中は自動でゲームビューが前に出ているだけなので、覚えない
		session.gameViewActive = mPrimaryGame && !IsPlaying();

		session.projectFolder = mProject->GetCurrentFolder();
		session.projectSelected = mProject->GetSelectedFile();
		session.projectIconScale = mProject->GetIconScale();

		for (const auto& panel : mPanels) { session.panels[panel->GetTitle()] = panel->IsOpen(); }
		session.language = EditorLocale::GetLanguageCode(EditorLocale::GetLanguage());
	}

	void Editor::RestoreSession(const EditorSession& session)
	{
		EditorLocale::SetLanguage(EditorLocale::FromCode(session.language));
		for (auto& panel : mPanels)
		{
			if (auto it = session.panels.find(panel->GetTitle()); it != session.panels.end()) { panel->SetOpen(it->second); }
		}

		mSceneView->SetGizmoOp(SceneViewPanel::GizmoOp(session.gizmoOp));
		mSceneView->SetGizmoLocal(session.gizmoLocal);
		mSceneView->showGrid = session.showGrid;
		mSceneView->showGizmos = session.showGizmos;
		mSceneView->showPhysics = session.showPhysics;
		mSceneView->showNavMesh = session.showNavMesh;
		if (session.gameViewActive && mGameView->IsOpen())
		{
			mPrimaryGame = true;
			mFocusView = "Game";
		}

		// -scene で別のシーンを開いたときや、前回のシーンが消されていたときは、選択もカメラも意味がない
		if (!session.scene.empty() && GetScene().GetPath() == session.scene)
		{
			if (session.hasCamera)
			{
				EditorCamera& camera = mSceneView->GetCamera();
				camera.position = session.cameraPosition;
				camera.yaw = session.cameraYaw;
				camera.pitch = session.cameraPitch;
				camera.fov = session.cameraFov;
				camera.moveSpeed = session.cameraMoveSpeed;
			}
			if (Actor* actor = GetScene().FindActor(session.selectedActor)) { Select(actor); }
		}

		// Select がプロジェクトの選択を消すので、その後で戻す
		mProject->RestoreState(session.projectFolder, session.projectSelected, session.projectIconScale);
	}

	Scene& Editor::GetScene() const
	{
		return mEngine.GetScene();
	}

	bool Editor::IsPlaying() const
	{
		return mEngine.IsPlaying();
	}

	Actor* Editor::GetSelected() const
	{
		return mSelectedId ? mEngine.GetScene().FindActor(mSelectedId) : nullptr;
	}

	void Editor::Select(Actor* actor)
	{
		mSelectedId = actor ? actor->GetId() : 0;
		if (actor) { mProject->ClearSelection(); }
		RefreshSnapshots();
	}

	void Editor::RefreshSnapshots()
	{
		Scene& scene = mEngine.GetScene();
		Actor* actor = GetSelected();
		mSnapshotActor = actor ? actor->GetId() : 0;
		mActorSnapshot = actor ? scene.SerializeActorState(actor) : Json();
		mSettingsSnapshot = scene.SerializeSettings();
	}

	// プレイ中の変更は止めたときに元に戻るので、履歴に残さない
	void Editor::CommitActorEdit(const std::string& label)
	{
		if (IsPlaying()) { return; }
		Actor* actor = GetSelected();
		if (!actor || actor->GetId() != mSnapshotActor) { RefreshSnapshots(); return; }
		Json after = mEngine.GetScene().SerializeActorState(actor);
		if (after == mActorSnapshot) { return; }
		PushCommand(std::make_unique<ActorStateCommand>(label, actor->GetId(), std::move(mActorSnapshot), after));
	}

	void Editor::CommitSettingsEdit(const std::string& label)
	{
		if (IsPlaying()) { return; }
		Json after = mEngine.GetScene().SerializeSettings();
		if (after == mSettingsSnapshot) { return; }
		PushCommand(std::make_unique<SceneSettingsCommand>(label, std::move(mSettingsSnapshot), after));
	}

	// 入れる前後のアクターの状態を Undo に積む（アクターの値の変更と同じ扱い）
	bool Editor::AssignMaterial(MeshRenderer& renderer, size_t slot, const std::string& path)
	{
		Actor* actor = renderer.GetActor();
		Scene& scene = mEngine.GetScene();
		Json before = scene.SerializeActorState(actor);
		if (!renderer.SetSharedMaterial(slot, path))
		{
			Logger::Warning(LogCat::Editor, "{} has no material slot {} (is the model loaded?)", actor->GetName(), slot);
			return false;
		}
		Logger::Info(LogCat::Editor, "Assigned {} to {} (slot {})", path, actor->GetName(), slot);
		Json after = scene.SerializeActorState(actor);
		if (after != before)
		{
			PushCommand(std::make_unique<ActorStateCommand>(EditorLocale::Format("Assign Material {}", GetStem(path)), actor->GetId(), std::move(before), std::move(after)));
		}
		return true;
	}

	void Editor::PushCommand(std::unique_ptr<EditorCommand> command)
	{
		if (IsPlaying()) { return; }
		mHistory.Push(std::move(command));
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::Undo()
	{
		if (IsPlaying() || !mHistory.CanUndo()) { return; }
		mSelectedId = mHistory.Undo(mEngine.GetScene());
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::Redo()
	{
		if (IsPlaying() || !mHistory.CanRedo()) { return; }
		mSelectedId = mHistory.Redo(mEngine.GetScene());
		RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::UpdateTitle()
	{
		mEngine.SetWindowTitleSuffix(mHistory.IsDirty() ? " *" : "");
	}

	ViewPanel& Editor::GetPrimaryView() const
	{
		return mPrimaryGame ? static_cast<ViewPanel&>(*mGameView) : static_cast<ViewPanel&>(*mSceneView);
	}

	// 最後にフォーカスした方を primary にする。primary が隠れていて、もう片方が見えていればそちらへ移す
	//  パネルを描く前に決める（描くときにどちらの結果を出すかが決まっているように）ので、見えているかは前のフレームで見る
	void Editor::UpdatePrimaryView()
	{
		if (mSceneView->IsFocused() && mSceneView->WasVisible()) { mPrimaryGame = false; }
		else if (mGameView->IsFocused() && mGameView->WasVisible()) { mPrimaryGame = true; }
		if (mPrimaryGame && !mGameView->WasVisible() && mSceneView->WasVisible()) { mPrimaryGame = false; }
		else if (!mPrimaryGame && !mSceneView->WasVisible() && mGameView->WasVisible()) { mPrimaryGame = true; }
		mSceneView->SetPrimary(!mPrimaryGame);
		mGameView->SetPrimary(mPrimaryGame);
	}

	void Editor::GetViewportSize(uint32_t& width, uint32_t& height) const
	{
		GetPrimaryView().GetRenderSize(width, height);
	}

	// ゲームビューはゲームと同じカメラ（メインカメラ）なので差し替えない
	bool Editor::OverrideCamera(float aspect, RenderCamera& camera) const
	{
		if (mPrimaryGame) { return false; }
		camera = mSceneView->GetViewCamera(aspect);
		return true;
	}

	void Editor::DrawGizmos(DebugDraw& draw)
	{
		if (!mPrimaryGame) { DrawSceneGizmos(draw); }
	}

	// primary でない方も見えていれば、シーンを集め直して自分の描画先へ描く
	void Editor::RenderExtraViews(Scene& scene, float time)
	{
		ViewPanel& view = mPrimaryGame ? static_cast<ViewPanel&>(*mSceneView) : static_cast<ViewPanel&>(*mGameView);
		if (!view.IsVisible()) { return; }
		uint32_t width, height;
		view.GetRenderSize(width, height);
		mExtraList.uiScreenSize = mPrimaryGame ? Vector2() : Vector2(float(width), float(height));
		scene.CollectRender(mExtraList);
		DebugDraw debug;
		if (mPrimaryGame) { DrawSceneGizmos(debug); }
		Renderer::Get().RenderOffscreen(view.GetOffscreenTargets(), mExtraList, view.GetViewCamera(view.GetAspect()), scene.GetEnvironment(),
			width, height, time, &debug);
	}

	void Editor::DrawSceneGizmos(DebugDraw& draw)
	{
		Scene& scene = mEngine.GetScene();
		if (mSceneView->showGrid) { draw.Grid(50.0f, 1.0f, Color(0.5f, 0.5f, 0.5f, 0.25f)); }
		if (mSceneView->showGizmos) { scene.DrawGizmos(draw, GetSelected()); }
		else if (Actor* sel = GetSelected()) { draw.Axes(sel->GetTransform().GetWorldMatrix(), 1.0f); }
		if (mSceneView->showPhysics)
		{
			for (auto* c : scene.FindComponents<Collider>()) { c->OnDrawGizmos(draw, false); }
		}
		DrawNavMeshGizmos(draw);
	}

	// ナビメッシュは「NavMesh」が オン のとき、または NavMeshSurface を持つアクターを選んでいるときに描く。
	//  シーンのナビメッシュは 1 つ（NavigationSystem）なので、選んでいる Surface があればそれを、
	//  なければ最初に見つかった Surface を読み込んで描く
	void Editor::DrawNavMeshGizmos(DebugDraw& draw)
	{
		Scene& scene = mEngine.GetScene();
		Actor* selected = GetSelected();
		NavMeshSurface* surface = selected ? selected->GetComponent<NavMeshSurface>() : nullptr;
		if (!surface && !mSceneView->showNavMesh) { return; }
		if (!surface || surface->data.empty())
		{
			surface = nullptr;
			for (NavMeshSurface* s : scene.FindComponents<NavMeshSurface>())
			{
				if (!s->data.empty()) { surface = s; break; }
			}
		}
		if (!surface) { return; }
		NavigationSystem& nav = scene.GetSystem<NavigationSystem>();
		nav.LoadSurface(surface->data);
		nav.GetMesh().DrawGizmos(draw);
	}

	void Editor::RenderUI()
	{
		mImGui.Render();
	}

	// メニュー → ツールバー → ドッキング → パネル → 確認の画面 → ショートカット → 後回しにした操作 の順
	void Editor::Update(float dt)
	{
		EditorStyle::SetPlayMode(IsPlaying(), mEngine.IsPaused());
		mHotReload->Update();
		DrawMenuBar();
		DrawMainToolbar();
		DrawStatusBar();
		ImGuiID dockId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		if (mFirstLayout)
		{
			mFirstLayout = false;
			SetupDockLayout(dockId);
		}

		// Unity と同じく、プレイを始めたらゲームビュー、止めたらシーンビューを前に出す
		bool playing = IsPlaying();
		if (playing != mWasPlaying)
		{
			mWasPlaying = playing;
			mFocusView = playing ? "Game" : "Scene";
			mPrimaryGame = playing;
			mSceneView->SetPrimary(!playing);
			mGameView->SetPrimary(playing);
		}
		// 切り替えたフレームはまだ前のフォーカスが残っているので、フォーカスで決め直さない
		else { UpdatePrimaryView(); }

		for (auto& panel : mPanels) { panel->Draw(); }
		if (mSelectDefaultTabs) { SelectDefaultTabs(); }
		// 最後に開いたウィンドウ（Stats）がフォーカスを持つと、そのタブが選ばれ続けるので、起動時はシーンビューにフォーカスを移す
		if (mFocusView)
		{
			ImGui::SetWindowFocus(TrId(mFocusView));
			mFocusView = nullptr;
		}
		for (auto& panel : mPanels) { panel->Update(dt); }
		if (mShowDemo) { ImGui::ShowDemoWindow(&mShowDemo); }
		DrawDialogs();
		HandleShortcuts();
		RunDeferred();
	}

	void Editor::SetupDockLayout(unsigned int dockId)
	{
		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImGui::DockBuilderRemoveNode(dockId);
		ImGui::DockBuilderAddNode(dockId, ImGuiDockNodeFlags_DockSpace);
		ImGui::DockBuilderSetNodeSize(dockId, vp->WorkSize);
		ImGuiID center = dockId;
		ImGuiID left = ImGui::DockBuilderSplitNode(center, ImGuiDir_Left, 0.17f, nullptr, &center);
		ImGuiID right = ImGui::DockBuilderSplitNode(center, ImGuiDir_Right, 0.3f, nullptr, &center);
		ImGuiID rightBottom = ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.5f, nullptr, &right);
		ImGuiID bottom = ImGui::DockBuilderSplitNode(center, ImGuiDir_Down, 0.28f, nullptr, &center);
		// ウィンドウの ID は「訳###英語」なので、同じ形の名前で指す（ID は ### から後ろで決まる）
		ImGui::DockBuilderDockWindow(TrId("Hierarchy"), left);
		ImGui::DockBuilderDockWindow(TrId("Properties"), right);
		ImGui::DockBuilderDockWindow(TrId("Lighting"), right);
		ImGui::DockBuilderDockWindow(TrId("Stats"), right);
		ImGui::DockBuilderDockWindow(TrId("AI Assistant"), right);
		ImGui::DockBuilderDockWindow(TrId("Preview"), rightBottom);
		ImGui::DockBuilderDockWindow(TrId("Project"), bottom);
		ImGui::DockBuilderDockWindow(TrId("Log"), bottom);
		ImGui::DockBuilderDockWindow(TrId("Profiler"), bottom);
		ImGui::DockBuilderDockWindow(TrId("Scene"), center);
		ImGui::DockBuilderDockWindow(TrId("Game"), center);
		ImGui::DockBuilderFinish(dockId);
		mSelectDefaultTabs = true;
		WriteLayoutVersion();
	}

	// 配置を作り直すと最後に入れたタブが前に出るので、タブができてから Properties と Project を選び直す
	void Editor::SelectDefaultTabs()
	{
		for (const char* name : { "Properties", "Project" })
		{
			ImGuiWindow* window = ImGui::FindWindowByName(TrId(name));
			if (!window || !window->DockNode || !window->DockNode->TabBar) { return; }
			if (!ImGui::TabBarFindTabByID(window->DockNode->TabBar, window->TabId)) { return; }
		}
		for (const char* name : { "Properties", "Project" })
		{
			ImGuiWindow* window = ImGui::FindWindowByName(TrId(name));
			window->DockNode->TabBar->NextSelectedTabId = window->TabId;
			window->DockNode->SelectedTabId = window->TabId;
		}
		mFocusView = "Scene";
		ImGui::MarkIniSettingsDirty();
		mSelectDefaultTabs = false;
	}

	// 後回しにした操作の中でさらに Defer されることもあるので、入れ替えてから実行する
	void Editor::RunDeferred()
	{
		while (!mDeferred.empty())
		{
			std::vector<std::function<void()>> actions;
			actions.swap(mDeferred);
			for (auto& action : actions) { action(); }
		}
	}
}
