#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/CommandHistory.h"
#include "Editor/Core/ImGuiLayer.h"
#include "Runtime/FrameOverlay.h"
#include "Scene/Actor/Actor.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class AIAssistant;
	class ConsolePanel;
	class EditorPanel;
	class Engine;
	class HierarchyPanel;
	class InspectorPanel;
	class LightingPanel;
	class ProfilerPanel;
	class ProjectPanel;
	class SceneViewPanel;
	class Scene;
	class StatsPanel;

	// ImGui のエディタ（Unity 風: Hierarchy / Inspector / Project / Console / Scene・Game ビュー）
	//  パネルは EditorPanel の派生として別のクラスにし、ここは選択・Undo・シーンとアクターの操作・メニューを受け持つ。
	//  エンジンは作るときに渡される（グローバルの Engine::Get() には頼らない）。
	//  実装は Editor.cpp（全体）/ EditorActions.cpp（アクターとシーンの操作）/ EditorMenu.cpp（メニュー・ツールバー・ショートカット）
	class Editor final : public FrameOverlay
	{
	public:
		explicit Editor(Engine& engine);
		~Editor();

		void Initialize();
		void Shutdown();
		bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void BeginFrame();
		void Update(float dt);
		// シーンが差し替わった（keepSelection ならプレイを止めて戻ったので選択と履歴を残す）
		void OnSceneChanged(bool keepSelection = false);

		void GetViewportSize(uint32_t& width, uint32_t& height) const override;
		bool OverrideCamera(float aspect, RenderCamera& camera) const override;
		void DrawGizmos(DebugDraw& draw) override;
		void RenderUI() override;

		Engine& GetEngine() const { return mEngine; }
		Scene& GetScene() const;
		bool IsPlaying() const;

		Actor* GetSelected() const;
		ActorId GetSelectedId() const { return mSelectedId; }
		void Select(Actor* actor);

		// 操作した後に呼ぶ: 選んでいるアクター / シーンの設定の変更を確定して Undo の履歴に積む
		void CommitActorEdit(const std::string& label);
		void CommitSettingsEdit(const std::string& label);
		void PushCommand(std::unique_ptr<EditorCommand> command);
		void Undo();
		void Redo();
		const CommandHistory& GetHistory() const { return mHistory; }

		// パネルを描き終えてから実行する（描いている途中にシーンを変えると辿っている途中の一覧が壊れるため）
		void Defer(std::function<void()> action) { mDeferred.push_back(std::move(action)); }

		// kind は "Empty" / "Cube" / "Sphere" / "Plane" / "Capsule" / "Directional Light" / "Point Light" /
		// "Spot Light" / "Camera" / "Particle System"。parent を渡すとその子として親の原点に、なければカメラの前に作る
		Actor* CreateActor(const std::string& kind, Actor* parent = nullptr);
		// 作成のメニューの項目を並べる（選ばれたら後で作る）
		void DrawCreateMenu(Actor* parent);
		void DeleteSelected();
		void DuplicateSelected();
		void CopySelected();
		void PasteClipboard();
		bool HasClipboard() const { return !mClipboard.is_null(); }
		void RenameSelected();
		void FocusOn(Actor* actor);
		// アクター（と子）を dir にプレハブとして保存する（同じ名前があれば番号を付ける）。保存したパスを返す
		std::string CreatePrefab(Actor* actor, const std::string& dir);
		Actor* InstantiatePrefab(const std::string& path, Actor* parent, int siblingIndex);

		void NewScene();
		void OpenScene();
		void OpenScene(const std::string& path);
		bool SaveScene(bool saveAs);
		void RequestQuit();

		void TogglePlay();
		void TogglePause();
		void StepFrame();

		SceneViewPanel& GetSceneView() { return *mSceneView; }
		HierarchyPanel& GetHierarchy() { return *mHierarchy; }
		ProjectPanel& GetProject() { return *mProject; }

	private:
		template <class T>
		T* AddPanel(std::unique_ptr<T> panel)
		{
			T* raw = panel.get();
			mPanels.push_back(std::move(panel));
			return raw;
		}
		Actor* CreatePrimitive(const std::string& name, const std::string& model);
		void RefreshSnapshots();
		void UpdateTitle();
		void RunDeferred();
		// 保存していない変更があれば「保存しますか？」を出してから action を実行する
		void RunAfterSaveCheck(std::function<void()> action);

		void SetupDockLayout(unsigned int dockId);
		void SelectDefaultTabs();
		void DrawMenuBar();
		void DrawMainToolbar();
		void DrawDialogs();
		void HandleShortcuts();

		Engine& mEngine;
		ImGuiLayer mImGui;
		CommandHistory mHistory;
		ActorId mSelectedId = 0;
		// 変更前の状態（確定したときに変更後と比べてコマンドを作る）
		ActorId mSnapshotActor = 0;
		Json mActorSnapshot;
		Json mSettingsSnapshot;
		Json mClipboard;
		std::vector<std::function<void()>> mDeferred;

		std::vector<std::unique_ptr<EditorPanel>> mPanels;
		SceneViewPanel* mSceneView = nullptr;
		HierarchyPanel* mHierarchy = nullptr;
		InspectorPanel* mInspector = nullptr;
		ProjectPanel* mProject = nullptr;
		ConsolePanel* mConsole = nullptr;
		AIAssistant* mAssistant = nullptr;
		ProfilerPanel* mProfiler = nullptr;

		bool mFirstLayout = true;
		bool mSelectDefaultTabs = false;
		bool mFocusViewport = true;		bool mShowDemo = false;
		bool mAllowClose = false;
		std::function<void()> mPendingAfterSave;
		bool mOpenSavePrompt = false;
	};
}
