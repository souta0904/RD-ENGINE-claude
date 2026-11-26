#pragma once
#include "Core/Serialization/Json.h"
#include <string>

namespace rd
{
	class Actor;
	class Editor;
	class MeshRenderer;

	// エディタでのアクターの操作（作る・消す・複製・コピーと貼り付け・プレハブ・マテリアルを入れる など）
	//  どの操作も選択を合わせ、Undo に積む。メニュー・ヒエラルキー・シーンビューから同じものを呼ぶ
	class RD_API ActorActions
	{
	public:
		explicit ActorActions(Editor& editor) : mEditor(editor) {}

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
		// シーンビューのカメラを actor に向ける
		void FocusOn(Actor* actor);

		// アクター（と子）を dir にプレハブとして保存する（同じ名前があれば番号を付ける）。保存したパスを返す
		std::string CreatePrefab(Actor* actor, const std::string& dir);
		// プレハブかモデルを parent の siblingIndex 番目（-1 なら最後）に置いて選ぶ
		Actor* InstantiateAsset(const std::string& path, Actor* parent, int siblingIndex);
		// MeshRenderer の枠にマテリアルのアセットを入れる（入れられなければ false）
		bool AssignMaterial(MeshRenderer& renderer, size_t slot, const std::string& path);

	private:
		Actor* CreatePrimitive(const std::string& name, const std::string& model);

		Editor& mEditor;
		Json mClipboard;
	};
}
