#pragma once
#include "Editor/Core/EditorCamera.h"
#include "Editor/Panels/EditorPanel.h"
#include "Math/Geometry/Ray.h"
#include <string>

namespace rd
{
	class Actor;

	// シーンビューとゲームビュー（ツールバー左端のボタンで切り替える）
	//  1 フレームに描く画面は 1 つなので、見ている方のカメラで描く（プレイを始めるとゲームビューへ、止めるとシーンビューへ戻る）
	//  シーンビュー: エディタのカメラ、ギズモでの移動・回転・拡縮、クリックで選択、アセットのドロップ
	//  ゲームビュー: シーンのメインカメラ。縦横比を選べ、フォーカスしているときだけゲームに入力が届く
	class SceneViewPanel final : public EditorPanel
	{
	public:
		enum class View { Scene, Game };
		enum class GizmoOp { Move, Rotate, Scale };

		explicit SceneViewPanel(Editor& editor);

		void Update(float dt) override;

		View GetView() const { return mView; }
		void SetView(View view) { mView = view; }
		bool UsesEditorCamera() const { return mView == View::Scene; }
		RenderCamera GetSceneCamera(float aspect) const { return mCamera.GetRenderCamera(aspect); }
		EditorCamera& GetCamera() { return mCamera; }
		void GetRenderSize(uint32_t& width, uint32_t& height) const;
		bool IsHovered() const { return mHovered; }
		bool IsFocused() const { return mFocused; }

		GizmoOp GetGizmoOp() const { return mGizmoOp; }
		void SetGizmoOp(GizmoOp op) { mGizmoOp = op; }
		bool IsGizmoLocal() const { return mGizmoLocal; }
		void SetGizmoLocal(bool local) { mGizmoLocal = local; }

		bool showGrid = true;
		bool showGizmos = true;
		bool showPhysics = false;

	protected:
		void OnGui() override;
		ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
		bool HasPadding() const override { return false; }

	private:
		void DrawViewSwitch();
		void DrawSceneToolbar();
		void DrawGameToolbar();
		void DrawGizmo(Actor* selected, const RenderCamera& cam, bool& usingGizmo);
		void HandleDrop(const std::string& path, const Ray& ray);
		Actor* Pick(const Ray& ray) const;
		Vector2 ToViewUv(ImVec2 mouse) const;

		EditorCamera mCamera;
		View mView = View::Scene;
		bool mWasPlaying = false;
		GizmoOp mGizmoOp = GizmoOp::Move;
		bool mGizmoLocal = false;
		bool mGizmoWasUsing = false;
		int mAspect = 0;

		uint32_t mWidth = 1280;
		uint32_t mHeight = 720;
		ImVec2 mImagePos = ImVec2(0.0f, 0.0f);
		ImVec2 mImageSize = ImVec2(1.0f, 1.0f);
		bool mHovered = false;
		bool mFocused = false;
	};
}
