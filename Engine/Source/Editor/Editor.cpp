#include "RdPch.h"
#include "Editor/Editor.h"
#include "Editor/Assistant/AIAssistant.h"
#include "Editor/Commands/SceneSettingsCommand.h"
#include "Editor/Commands/ActorStateCommand.h"
#include "Editor/Panels/ConsolePanel.h"
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

		bool LayoutHasWindow(const char* name)
		{
			std::ifstream file(kLayoutFile);
			std::string text((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
			return text.find(std::string("[Window][") + name + "]") != std::string::npos;
		}
	}

	Editor::Editor(Engine& engine) : mEngine(engine)
	{
		mSceneView = AddPanel(std::make_unique<SceneViewPanel>(*this));
		mHierarchy = AddPanel(std::make_unique<HierarchyPanel>(*this));
		mProperties = AddPanel(std::make_unique<PropertiesPanel>(*this));
		mProject = AddPanel(std::make_unique<ProjectPanel>(*this));
		AddPanel(std::make_unique<PreviewPanel>(*this));
		mConsole = AddPanel(std::make_unique<ConsolePanel>(*this));
		AddPanel(std::make_unique<LightingPanel>(*this));
		AddPanel(std::make_unique<StatsPanel>(*this));
		mProfiler = AddPanel(std::make_unique<ProfilerPanel>(*this));
		mAssistant = AddPanel(std::make_unique<AIAssistant>(*this));
	}

	Editor::~Editor() = default;

	void Editor::Initialize()
	{
		std::filesystem::create_directories("Logs");
		mImGui.Initialize(mEngine.GetWindow().GetHandle());
		ImGui::GetIO().IniFilename = kLayoutFile;
		// 前回の配置が保存されていればそれを使う（後から足したパネルが入っていない古い配置なら作り直す）
		mFirstLayout = !std::filesystem::exists(kLayoutFile) || !LayoutHasWindow("Preview");
	}

	void Editor::Shutdown()
	{
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

	void Editor::GetViewportSize(uint32_t& width, uint32_t& height) const
	{
		mSceneView->GetRenderSize(width, height);
	}

	bool Editor::OverrideCamera(float aspect, RenderCamera& camera) const
	{
		if (!mSceneView->UsesEditorCamera()) { return false; }
		camera = mSceneView->GetSceneCamera(aspect);
		return true;
	}

	void Editor::DrawGizmos(DebugDraw& draw)
	{
		if (!mSceneView->UsesEditorCamera()) { return; }
		Scene& scene = mEngine.GetScene();
		if (mSceneView->showGrid) { draw.Grid(50.0f, 1.0f, Color(0.5f, 0.5f, 0.5f, 0.25f)); }
		if (mSceneView->showGizmos) { scene.DrawGizmos(draw, GetSelected()); }
		else if (Actor* sel = GetSelected()) { draw.Axes(sel->GetTransform().GetWorldMatrix(), 1.0f); }
		if (mSceneView->showPhysics)
		{
			for (auto* c : scene.FindComponents<Collider>()) { c->OnDrawGizmos(draw, false); }
		}
	}

	void Editor::RenderUI()
	{
		mImGui.Render();
	}

	// メニュー → ツールバー → ドッキング → パネル → 確認の画面 → ショートカット → 後回しにした操作 の順
	void Editor::Update(float dt)
	{
		EditorStyle::SetPlayMode(IsPlaying(), mEngine.IsPaused());
		DrawMenuBar();
		DrawMainToolbar();
		ImGuiID dockId = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport(), ImGuiDockNodeFlags_PassthruCentralNode);
		if (mFirstLayout)
		{
			mFirstLayout = false;
			SetupDockLayout(dockId);
		}

		for (auto& panel : mPanels) { panel->Draw(); }
		if (mSelectDefaultTabs) { SelectDefaultTabs(); }
		// 最後に開いたウィンドウ（Stats）がフォーカスを持つと、そのタブが選ばれ続けるので、起動時はビューにフォーカスを移す
		if (mFocusViewport)
		{
			ImGui::SetWindowFocus("Viewport");
			mFocusViewport = false;
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
		ImGui::DockBuilderDockWindow("Hierarchy", left);
		ImGui::DockBuilderDockWindow("Properties", right);
		ImGui::DockBuilderDockWindow("Lighting", right);
		ImGui::DockBuilderDockWindow("Stats", right);
		ImGui::DockBuilderDockWindow("AI Assistant", right);
		ImGui::DockBuilderDockWindow("Preview", rightBottom);
		ImGui::DockBuilderDockWindow("Project", bottom);
		ImGui::DockBuilderDockWindow("Console", bottom);
		ImGui::DockBuilderDockWindow("Profiler", bottom);
		ImGui::DockBuilderDockWindow("Viewport", center);
		ImGui::DockBuilderFinish(dockId);
		mSelectDefaultTabs = true;
	}

	// 配置を作り直すと最後に入れたタブが前に出るので、タブができてから Properties と Project を選び直す
	void Editor::SelectDefaultTabs()
	{
		for (const char* name : { "Properties", "Project" })
		{
			ImGuiWindow* window = ImGui::FindWindowByName(name);
			if (!window || !window->DockNode || !window->DockNode->TabBar) { return; }
			if (!ImGui::TabBarFindTabByID(window->DockNode->TabBar, window->TabId)) { return; }
		}
		for (const char* name : { "Properties", "Project" })
		{
			ImGuiWindow* window = ImGui::FindWindowByName(name);
			window->DockNode->TabBar->NextSelectedTabId = window->TabId;
			window->DockNode->SelectedTabId = window->TabId;
		}
		mFocusViewport = true;
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
