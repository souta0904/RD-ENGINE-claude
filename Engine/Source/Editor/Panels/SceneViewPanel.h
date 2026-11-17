#pragma once
#include "Editor/Core/EditorCamera.h"
#include "Editor/Panels/ViewPanel.h"
#include "Math/Geometry/Ray.h"
#include <string>

namespace rd
{
	class Actor;

	// シーンビュー（Unity の Scene）
	//  エディタのカメラ、ギズモでの移動・回転・拡縮、クリックで選択、アセットのドロップ
	class SceneViewPanel final : public ViewPanel
	{
	public:
		enum class GizmoOp { Move, Rotate, Scale };

		explicit SceneViewPanel(Editor& editor);

		void Update(float dt) override;
		RenderCamera GetViewCamera(float aspect) const override { return mCamera.GetRenderCamera(aspect); }
		EditorCamera& GetCamera() { return mCamera; }

		GizmoOp GetGizmoOp() const { return mGizmoOp; }
		void SetGizmoOp(GizmoOp op) { mGizmoOp = op; }
		bool IsGizmoLocal() const { return mGizmoLocal; }
		void SetGizmoLocal(bool local) { mGizmoLocal = local; }

		bool showGrid = true;
		bool showGizmos = true;
		bool showPhysics = false;

	protected:
		void OnGui() override;

	private:
		void DrawToolbar();
		void DrawGizmo(Actor* selected, const RenderCamera& cam, bool& usingGizmo);
		void HandleDrop(const std::string& path, const Ray& ray);
		Actor* Pick(const Ray& ray) const;

		EditorCamera mCamera;
		GizmoOp mGizmoOp = GizmoOp::Move;
		bool mGizmoLocal = false;
		bool mGizmoWasUsing = false;
	};
}
