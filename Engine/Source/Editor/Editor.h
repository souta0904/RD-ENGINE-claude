#pragma once
#include "Editor/EditorCamera.h"
#include "Editor/ImGuiLayer.h"
#include "Editor/CommandHistory.h"
#include "Runtime/FrameOverlay.h"
#include "Scene/Actor.h"
#include <string>
#include <vector>

namespace rd
{
	class DebugDraw;
	class Engine;
	class Scene;

	// ImGui ベースのエディタ
	//  エンジンは作るときに渡される（グローバルの Engine::Get() には頼らない）
	class Editor final : public FrameOverlay
	{
	public:
		explicit Editor(Engine& engine) : mEngine(engine) {}

		void Initialize();
		void Shutdown();
		bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void BeginFrame();
		void Update(float dt);
		// シーンが差し替わった（keepSelection=true なら選択を ID で復元）
		void OnSceneChanged(bool keepSelection = false);

		// FrameOverlay
		void GetViewportSize(uint32_t& width, uint32_t& height) const override;
		bool OverrideCamera(float aspect, RenderCamera& camera) const override;
		void DrawGizmos(DebugDraw& draw) override;
		void RenderUI() override;

		bool UseEditorCamera() const;
		RenderCamera GetEditorCamera(float aspect) const { return mCamera.GetRenderCamera(aspect); }
		Engine& GetEngine() const { return mEngine; }
		Scene& GetScene() const;

		// 選択
		Actor* GetSelected() const;
		void Select(Actor* actor);

		// --- Undo / Redo（操作した後に呼ぶ） ---
		// 選択中のアクターの変更（インスペクタ・ギズモ・コンポーネントの追加/削除）を確定する
		void CommitActorEdit(const std::string& label);
		// シーン設定（環境・重力）の変更を確定する
		void CommitSettingsEdit(const std::string& label);
		// 実行済みのコマンドを履歴に積む
		void PushCommand(std::unique_ptr<EditorCommand> command);

	private:
		// メイン
		void DrawMenuBar();
		void DrawToolbar();
		void DrawViewport();
		void HandleShortcuts();
		void HandleViewportDrop(const std::string& path, const Ray& ray);
		Actor* Pick(const Ray& ray);
		// パネル（EditorPanels.cpp）
		void DrawHierarchy();
		void DrawHierarchyNode(Actor* actor);
		void DrawInspector();
		void DrawAssets();
		void DrawConsole();
		void DrawStats();
		void DrawRenderSettings();
		// 操作
		Actor* CreatePrimitive(const std::string& name, const std::string& model);
		Actor* CreateActorFromMenu(const std::string& kind);
		void DeleteSelected();
		void DuplicateSelected();
		void SaveScene(bool saveAs);
		void OpenScene();
		void Undo();
		void Redo();
		// 「変更前」の状態を取り直す（選択の変更・確定・Undo の後）
		void RefreshSnapshots();
		void UpdateTitle();

		Engine& mEngine;
		ImGuiLayer mImGui;
		EditorCamera mCamera;
		CommandHistory mHistory;
		ActorId mSelectedId = 0;
		// 変更前の状態（確定したときに変更後と比べてコマンドを作る）
		ActorId mSnapshotActor = 0;
		Json mActorSnapshot;
		Json mSettingsSnapshot;

		// ビューポート
		uint32_t mViewportWidth = 1280;
		uint32_t mViewportHeight = 720;
		float mViewportX = 0.0f, mViewportY = 0.0f;
		bool mViewportHovered = false;
		bool mViewportFocused = false;
		bool mSceneCameraInPlay = false;// プレイ中もエディタカメラで見る
		bool mShowGrid = true;
		bool mShowGizmos = true;
		bool mShowPhysics = false;
		int mGizmoOp = 0;// 0:移動 1:回転 2:拡縮
		bool mGizmoLocal = false;
		bool mGizmoWasUsing = false;

		// パネルの表示
		bool mShowHierarchy = true;
		bool mShowInspector = true;
		bool mShowAssets = true;
		bool mShowConsole = true;
		bool mShowStats = true;
		bool mShowRenderSettings = true;
		bool mShowDemo = false;
		bool mFirstLayout = true;

		// アセットパネル
		std::string mAssetDir = "Assets";
		std::string mAssetFilter;
		// コンソール
		std::string mConsoleFilter;
		bool mConsoleShowDebug = false;// Trace / Debug
		bool mConsoleShowInfo = true;
		bool mConsoleShowWarn = true;
		bool mConsoleShowError = true;
		uint64_t mConsoleVersion = 0;
		// 統計
		std::vector<float> mFrameTimes;
		std::string mAddComponentFilter;
	};
}
