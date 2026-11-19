#pragma once
#include "Editor/Core/EditorCamera.h"
#include "Editor/Panels/ViewPanel.h"
#include "Math/Geometry/Ray.h"
#include <string>

namespace rd
{
	class Actor;
	class MeshRenderer;

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
		const EditorCamera& GetCamera() const { return mCamera; }

		GizmoOp GetGizmoOp() const { return mGizmoOp; }
		void SetGizmoOp(GizmoOp op) { mGizmoOp = op; }
		bool IsGizmoLocal() const { return mGizmoLocal; }
		void SetGizmoLocal(bool local) { mGizmoLocal = local; }

		bool showGrid = true;
		bool showGizmos = true;
		bool showPhysics = false;
		// ナビメッシュを常に表示する（オフでも NavMeshSurface を選んでいる間は表示する）
		bool showNavMesh = false;

	protected:
		void OnGui() override;

	private:
		void DrawToolbar();
		void DrawGizmo(Actor* selected, const RenderCamera& cam, bool& usingGizmo);
		void HandleDrop(const std::string& path, const Ray& ray);
		void DropMaterial(const std::string& path, const Ray& ray);
		Actor* Pick(const Ray& ray) const;
		MeshRenderer* PickRenderer(const Ray& ray, int& outSubMesh) const;

		// ドラッグ中のプレビュー（Unity と同じく、モデルやプレハブを運んでいる間は本物をマウスの下に置いて見せる）
		//  シーンビューの上にある間だけ仮のアクターを置いて動かし、離したらそのまま確定（Undo に積む）、
		//  シーンビューの外へ出たりドラッグをやめたりしたら消す
		static bool CanPlace(const std::string& path);
		Actor* SpawnAsset(const std::string& path);
		// 置く位置。モデルかコライダーに当たればそこ、なければ地面（y=0）、地面も見えなければ前方。ignore の下のものには当てない
		Vector3 FindPlacement(const Ray& ray, const Actor* ignore) const;
		void UpdateDragPreview(const std::string& path, const Ray& ray);
		void ClearDragPreview();
		Actor* GetDragPreview() const;

		EditorCamera mCamera;
		uint64_t mPreviewId = 0;     // 仮のアクター（ActorId。0 ならなし）
		std::string mPreviewPath;    // 仮のアクターを作ったアセット
		bool mPreviewTouched = false; // このフレームにシーンビューの上でドラッグしていたか
		GizmoOp mGizmoOp = GizmoOp::Move;
		bool mGizmoLocal = false;
		bool mGizmoWasUsing = false;
	};
}
