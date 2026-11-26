#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Core/ActorActions.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Core/EditorSession.h"
#include "Editor/Core/PlayMode.h"
#include "Editor/Core/SceneDocument.h"
#include "Editor/Core/ImGuiLayer.h"
#include "Render/RenderList/RenderList.h"
#include "Runtime/EngineLayer.h"
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class AIAssistant;
	class ActorActions;
	class BehaviorTreePanel;
	class EditorLayout;
	class EditorMainUI;
	class EditorPanel;
	class EditorSelection;
	class EditorUndo;
	class Engine;
	class GameViewPanel;
	class HierarchyPanel;
	class HotReload;
	class PlayMode;
	class ProfilerPanel;
	class ProjectPanel;
	class Scene;
	class SceneDocument;
	class SceneViewPanel;
	class ViewPanel;

	// ImGui のエディタ（Unity 風: Hierarchy / Properties / Preview / Project / Log / Scene / Game）
	//  エンジンに EngineLayer として差し込まれる（エンジンはエディタを知らない）。
	//  Editor はパネルと部品を持って組み立て、フレームの流れを回すだけで、中身は部品に任せる:
	//   EditorSelection（選んでいるもの）/ EditorUndo（Undo・Redo）/ ActorActions（アクターの操作）/
	//   SceneDocument（シーンのファイル）/ PlayMode（Play・Stop）/ HotReload（ゲームのコードの読み直し）/
	//   EditorLayout（パネルの配置）/ EditorMainUI（メニュー・ツールバー・ステータスバー・ショートカット）
	//  パネルは EditorPanel の派生で、作るときに Editor& を受け取り、ここから部品を使う。
	//  エンジンは作るときに渡される（グローバルの Engine::Get() には頼らない）。
	class RD_API Editor final : public EngineLayer
	{
	public:
		explicit Editor(Engine& engine);
		~Editor() override;

		// EngineLayer
		void OnConfigure(EngineConfig& config, WindowDesc& window) override;
		void OnInitialize() override;
		void OnShutdown() override;
		bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) override;
		void OnBeginFrame() override;
		void OnUpdate(float dt) override;
		void OnSceneChanged() override { SceneChanged(false); }

		// FrameOverlay
		void GetViewportSize(uint32_t& width, uint32_t& height) const override;
		bool OverrideCamera(float aspect, RenderCamera& camera) const override;
		void DrawGizmos(DebugDraw& draw) override;
		void RenderExtraViews(Scene& scene, float time) override;
		void RenderUI() override;

		// シーンが差し替わった（keepSelection: プレイを止めて同じシーンに戻ったので選択と履歴を残す）
		void SceneChanged(bool keepSelection);
		// ゲームのコードを読み直して、シーンを作り直した（選択と履歴は残す）
		void OnGameCodeReloaded();
		// タイトルを「エンジン名 - シーンのパス」にし、保存していない変更があれば * を付ける
		void UpdateTitle();

		Engine& GetEngine() const { return mEngine; }
		Scene& GetScene() const;
		bool IsPlaying() const;

		// 部品
		EditorSelection& GetSelection() const { return *mSelection; }
		EditorUndo& GetUndo() const { return *mUndo; }
		ActorActions& GetActions() const { return *mActions; }
		SceneDocument& GetDocument() const { return *mDocument; }
		PlayMode& GetPlayMode() const { return *mPlayMode; }
		HotReload& GetHotReload() const { return *mHotReload; }
		EditorLayout& GetLayout() const { return *mLayout; }

		// パネル
		const std::vector<std::unique_ptr<EditorPanel>>& GetPanels() const { return mPanels; }
		SceneViewPanel& GetSceneView() const { return *mSceneView; }
		HierarchyPanel& GetHierarchy() const { return *mHierarchy; }
		ProjectPanel& GetProject() const { return *mProject; }
		ProfilerPanel& GetProfiler() const { return *mProfiler; }
		AIAssistant& GetAssistant() const { return *mAssistant; }
		// 行動ツリー（.rdbt）をパネルで開く
		void OpenBehaviorTree(const std::string& path);

		// パネルを描き終えてから実行する（描いている途中にシーンを変えると辿っている途中の一覧が壊れるため）
		void Defer(std::function<void()> action) { mDeferred.push_back(std::move(action)); }
		// 次のフレームで前に出すビューのウィンドウ（"Scene" / "Game"）
		void FocusView(const char* name) { mFocusView = name; }

	private:
		template <class T>
		T* AddPanel(std::unique_ptr<T> panel)
		{
			T* raw = panel.get();
			mPanels.push_back(std::move(panel));
			return raw;
		}
		void CreatePanels();
		void RestoreSession(const EditorSession& session);
		void SaveSession();
		void RunDeferred();

		ViewPanel& GetPrimaryView() const;
		void UpdatePrimaryView();
		// シーンビューに重ねる線（グリッド・ギズモ・コライダー・ナビメッシュ）
		void DrawSceneGizmos(DebugDraw& draw);
		void DrawNavMeshGizmos(DebugDraw& draw);

		Engine& mEngine;
		ImGuiLayer mImGui;
		bool mImGuiReady = false;

		std::unique_ptr<EditorSelection> mSelection;
		std::unique_ptr<EditorUndo> mUndo;
		std::unique_ptr<ActorActions> mActions;
		std::unique_ptr<SceneDocument> mDocument;
		std::unique_ptr<PlayMode> mPlayMode;
		std::unique_ptr<HotReload> mHotReload;
		std::unique_ptr<EditorLayout> mLayout;
		std::unique_ptr<EditorMainUI> mMainUI;

		std::vector<std::unique_ptr<EditorPanel>> mPanels;
		SceneViewPanel* mSceneView = nullptr;
		GameViewPanel* mGameView = nullptr;
		HierarchyPanel* mHierarchy = nullptr;
		ProjectPanel* mProject = nullptr;
		ProfilerPanel* mProfiler = nullptr;
		AIAssistant* mAssistant = nullptr;
		BehaviorTreePanel* mBehaviorTree = nullptr;

		// primary でないビューに描くときの描画リスト（使い回す）
		RenderList mExtraList;
		std::vector<std::function<void()>> mDeferred;

		// 前回の状態（OnConfigure で読み、OnInitialize で戻す）。撮影や計測の実行では毎回同じ条件にしたいので使わない
		EditorSession mSession;
		bool mUseSession = false;
		bool mHasSession = false;
		bool mAutoPlay = false;

		bool mWasPlaying = false;
		bool mPrimaryGame = false;
		// 次に前に出すビューのウィンドウ名（なければ nullptr）
		const char* mFocusView = "Scene";
	};
}
