#include "RdPch.h"
#include "Editor/Editor.h"
#include "AI/Navigation/NavigationSystem.h"
#include "Components/Navigation/NavMeshSurface.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Core/ActorActions.h"
#include "Editor/Core/EditorLayout.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Core/HotReload.h"
#include "Editor/Core/PlayMode.h"
#include "Editor/Core/SceneDocument.h"
#include "Editor/EditorMainUI.h"
#include "Editor/Inspector/ComponentEditors.h"
#include "Editor/Panels/BehaviorTreePanel.h"
#include "Editor/Panels/GameViewPanel.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Panels/LightingPanel.h"
#include "Editor/Panels/LogPanel.h"
#include "Editor/Panels/PreviewPanel.h"
#include "Editor/Panels/ProfilerPanel.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Panels/PropertiesPanel.h"
#include "Editor/Panels/RenderGraphPanel.h"
#include "Editor/Panels/SceneViewPanel.h"
#include "Editor/Panels/StatsPanel.h"
#include "Editor/Style/EditorStyle.h"
#include "Engine.h"
#include "Physics/Colliders/Collider.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/Renderer/Renderer.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>

namespace rd
{
	// 部品はここで作る。パネルは ImGui と GPU の準備ができてから（OnInitialize）作る
	Editor::Editor(Engine& engine) : mEngine(engine)
	{
		mSelection = std::make_unique<EditorSelection>(*this);
		mUndo = std::make_unique<EditorUndo>(*this);
		mActions = std::make_unique<ActorActions>(*this);
		mDocument = std::make_unique<SceneDocument>(*this);
		mPlayMode = std::make_unique<PlayMode>(*this);
		mLayout = std::make_unique<EditorLayout>(*this);
		mMainUI = std::make_unique<EditorMainUI>(*this);
	}

	// GPU の完了はエンジンが待ってから消す
	Editor::~Editor()
	{
		mHotReload.reset();
		mPanels.clear();
		if (mImGuiReady)
		{
			EditorLocale::WriteMissing();
			mImGui.Shutdown();
		}
	}

	void Editor::CreatePanels()
	{
		mSceneView = AddPanel(std::make_unique<SceneViewPanel>(*this));
		mGameView = AddPanel(std::make_unique<GameViewPanel>(*this));
		mHierarchy = AddPanel(std::make_unique<HierarchyPanel>(*this));
		AddPanel(std::make_unique<PropertiesPanel>(*this));
		mProject = AddPanel(std::make_unique<ProjectPanel>(*this));
		AddPanel(std::make_unique<PreviewPanel>(*this));
		AddPanel(std::make_unique<LogPanel>(*this));
		AddPanel(std::make_unique<LightingPanel>(*this));
		AddPanel(std::make_unique<StatsPanel>(*this));
		mProfiler = AddPanel(std::make_unique<ProfilerPanel>(*this));
		mAssistant = AddPanel(std::make_unique<AIAssistant>(*this));
		mBehaviorTree = AddPanel(std::make_unique<BehaviorTreePanel>(*this));
		AddPanel(std::make_unique<RenderGraphPanel>(*this));
		mHotReload = std::make_unique<HotReload>(*this);
	}

	// 前回の状態を読む。窓とシーンは作る前に決める必要があるので、ここで設定へ反映する
	void Editor::OnConfigure(EngineConfig& config, WindowDesc& window)
	{
		mAutoPlay = config.autoPlay;
		mUseSession = config.captureFrame < 0 && config.profileFrames == 0;
		if (mUseSession) { mHasSession = mSession.Load(EditorSession::GetDefaultPath()); }
		if (!mHasSession) { return; }
		if (mSession.hasWindow)
		{
			window.hasPlacement = true;
			window.placement = mSession.window;
			window.maximized = mSession.window.maximized;
			if (!config.fullscreenFromCommandLine) { window.fullscreen = mSession.window.fullscreen; }
		}
		// シーンが消されていたら、いつもの開始シーンで起動する
		if (!config.sceneFromCommandLine && !mSession.scene.empty() && FileExists(mSession.scene)) { config.startScene = mSession.scene; }
	}

	void Editor::OnInitialize()
	{
		ComponentEditors::RegisterBuiltin();
		mImGui.Initialize(mEngine.GetWindow().GetHandle());
		mImGuiReady = true;
		EditorStyle::ApplyDarkTitleBar(mEngine.GetWindow().GetHandle());
		mEngine.GetWindow().EnableFileDrop();
		mLayout->Initialize();
		CreatePanels();

		SceneChanged(false);
		if (mHasSession) { RestoreSession(mSession); }
		if (mAutoPlay) { mPlayMode->Begin(); }
	}

	void Editor::OnShutdown()
	{
		SaveSession();
	}

	// ウィンドウを閉じようとしたとき、保存していない変更があれば先に聞く
	bool Editor::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		if (msg == WM_CLOSE && mDocument->InterceptClose()) { return true; }
		return mImGui.HandleMessage(hwnd, msg, wParam, lParam);
	}

	void Editor::OnBeginFrame()
	{
		mImGui.BeginFrame();
	}

	void Editor::OpenBehaviorTree(const std::string& path)
	{
		mBehaviorTree->Open(path);
	}

	// プレイを止めて戻ったとき（keepSelection）は同じ ID のシーンに戻るので履歴を残し、別のシーンを開いたら捨てる
	void Editor::SceneChanged(bool keepSelection)
	{
		if (!keepSelection)
		{
			mSelection->SetId(0);
			mUndo->Clear();
		}
		mUndo->RefreshSnapshots();
		UpdateTitle();
	}

	void Editor::OnGameCodeReloaded()
	{
		SceneChanged(true);
		for (auto& panel : mPanels) { panel->OnGameCodeReloaded(); }
	}

	void Editor::UpdateTitle()
	{
		const std::string& path = GetScene().GetPath();
		std::string scene = path.empty() ? std::string("Untitled") : path;
		mEngine.GetWindow().SetTitle(std::format("{} {} - {}{}", Engine::kName, Engine::kVersion, scene, mUndo->IsDirty() ? " *" : ""));
	}

	Scene& Editor::GetScene() const
	{
		return mEngine.GetScene();
	}

	bool Editor::IsPlaying() const
	{
		return mEngine.IsPlaying();
	}

	// 次の起動で今の状態に戻せるように保存する。
	//  プレイ中に閉じたら、ゲームが切り替えた場面ではなく、編集していたシーンを次に開く
	void Editor::SaveSession()
	{
		if (!mUseSession || !mEngine.GetSceneManager().HasScene()) { return; }
		EditorSession session;
		session.hasWindow = true;
		session.window = mEngine.GetWindow().GetPlacement();
		session.scene = IsPlaying() ? mPlayMode->GetStartScenePath() : GetScene().GetPath();

		// プレイ中にゲームが別の場面へ切り替えていたら、選択は編集中のシーンのものではないので残さない
		session.selectedActor = GetScene().GetPath() == session.scene ? mSelection->GetId() : 0;

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
		session.behaviorTree = mBehaviorTree->GetPath();
		session.language = EditorLocale::GetLanguageCode(EditorLocale::GetLanguage());
		if (!session.Save(EditorSession::GetDefaultPath())) { Logger::Warning(LogCat::Editor, "Failed to save the editor session"); }
	}

	// 前回の状態を戻す。選択とカメラは、前回と同じシーンを開いたときだけ戻す
	void Editor::RestoreSession(const EditorSession& session)
	{
		EditorLocale::SetLanguage(EditorLocale::FromCode(session.language));
		for (auto& panel : mPanels)
		{
			if (auto it = session.panels.find(panel->GetTitle()); it != session.panels.end()) { panel->SetOpen(it->second); }
		}
		std::error_code treeError;
		if (!session.behaviorTree.empty() && std::filesystem::exists(ToWide(session.behaviorTree), treeError))
		{
			bool open = mBehaviorTree->IsOpen();
			mBehaviorTree->Open(session.behaviorTree);
			mBehaviorTree->SetOpen(open);
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
			if (Actor* actor = GetScene().FindActor(session.selectedActor)) { mSelection->Select(actor); }
		}

		// Select がプロジェクトの選択を消すので、その後で戻す
		mProject->RestoreState(session.projectFolder, session.projectSelected, session.projectIconScale);
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
		Scene& scene = GetScene();
		Actor* selected = mSelection->Get();
		if (mSceneView->showGrid) { draw.Grid(50.0f, 1.0f, Color(0.5f, 0.5f, 0.5f, 0.25f)); }
		if (mSceneView->showGizmos) { scene.DrawGizmos(draw, selected); }
		else if (selected) { draw.Axes(selected->GetTransform().GetWorldMatrix(), 1.0f); }
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
		Scene& scene = GetScene();
		Actor* selected = mSelection->Get();
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
	void Editor::OnUpdate(float dt)
	{
		EditorStyle::SetPlayMode(IsPlaying(), mEngine.IsPaused());
		mHotReload->Update();
		mMainUI->DrawBars();
		mLayout->BeginDockSpace();

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
		mLayout->AfterPanels();
		// 最後に開いたウィンドウ（Stats）がフォーカスを持つと、そのタブが選ばれ続けるので、起動時はシーンビューにフォーカスを移す
		if (mFocusView)
		{
			ImGui::SetWindowFocus(TrId(mFocusView));
			mFocusView = nullptr;
		}
		for (auto& panel : mPanels) { panel->Update(dt); }
		mMainUI->DrawDemoWindow();
		mDocument->DrawSavePrompt();
		mMainUI->HandleShortcuts();
		RunDeferred();
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
