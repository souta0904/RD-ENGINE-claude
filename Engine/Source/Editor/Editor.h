#pragma once
#include "Editor/EditorCamera.h"
#include "Editor/ImGuiLayer.h"
#include "Editor/UndoSystem.h"
#include "Scene/Actor.h"
#include <string>
#include <vector>

namespace rd
{
	class DebugDraw;
	class Scene;

	// ImGui ベースのエディタ
	class Editor
	{
	public:
		void Initialize();
		void Shutdown();
		bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		void BeginFrame();
		void Update(float dt);
		void DrawGizmos(DebugDraw& draw);
		void RenderUI();
		// シーンが差し替わった（keepSelection=true なら選択を ID で復元）
		void OnSceneChanged(bool keepSelection = false);

		void GetViewportSize(uint32_t& width, uint32_t& height) const;
		bool UseEditorCamera() const;
		RenderCamera GetEditorCamera(float aspect) const { return mCamera.GetRenderCamera(aspect); }

		// 選択
		Actor* GetSelected() const;
		void Select(Actor* actor) { mSelectedId = actor ? actor->GetId() : 0; }
		// 編集を確定して Undo 履歴に積む
		void Commit(const std::string& label);

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

		ImGuiLayer mImGui;
		EditorCamera mCamera;
		UndoSystem mUndo;
		ActorId mSelectedId = 0;

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
		bool mConsoleShowInfo = true;
		bool mConsoleShowWarn = true;
		bool mConsoleShowError = true;
		uint64_t mConsoleVersion = 0;
		// 統計
		std::vector<float> mFrameTimes;
		std::string mRenameBuffer;
		ActorId mRenamingId = 0;
		ActorId mPendingDelete = 0;
		std::string mAddComponentFilter;
	};
}
