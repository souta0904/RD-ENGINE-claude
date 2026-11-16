#pragma once
#include "Editor/Panels/EditorPanel.h"
#include "Scene/Actor/Actor.h"
#include <ImGui/imgui_internal.h>
#include <string>
#include <vector>

namespace rd
{
	// Unity 風のヒエラルキー
	//  - 行の上 1/4 / 下 1/4 へのドロップで前後に並べ替え、真ん中へのドロップで子にする
	//  - シーンの行や空いている所へのドロップでルートの末尾へ。プレハブのドロップでその場に置く
	//  - ↑↓で選択、←→で開閉、F2 で名前の変更、ダブルクリックでフォーカス、右端の目で有効 / 無効
	//  - 検索欄に入力すると、一致したアクターだけを平らに並べる
	class HierarchyPanel final : public EditorPanel
	{
	public:
		explicit HierarchyPanel(Editor& editor);
		void BeginRename(Actor* actor);

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(280.0f, 500.0f); }

	private:
		enum class DropZone { Before, Inside, After };
		struct DropTarget
		{
			Actor* parent = nullptr;
			int index = -1;
		};
		// ツリーを描いている途中にすると辿っている一覧が壊れる操作は、描き終えてからまとめて行う
		//  moveIndex / instIndex は動かす前の並びでの挿入位置（-1 で末尾）
		struct PendingOps
		{
			Actor* move = nullptr;
			Actor* moveParent = nullptr;
			int moveIndex = -1;
			std::string instantiate;
			Actor* instParent = nullptr;
			int instIndex = -1;
			Actor* rename = nullptr;
			Actor* toggleActive = nullptr;
		};
		// 後ろの行の背景で隠れないよう、ドロップ位置の線はツリーを描き終えてから描く
		struct DropIndicator
		{
			bool active = false;
			ImRect row;
			DropZone zone = DropZone::Inside;
			float x = 0.0f;
		};
		struct Row
		{
			ActorId id = 0;
			ActorId parent = 0;
			bool open = false;
			bool hasChildren = false;
		};

		void DrawToolbar();
		void DrawSceneRow();
		void DrawTree();
		void DrawNode(Actor* actor, bool flat);
		void DrawNodeContextMenu(Actor* actor);
		void DrawNodeDropTarget(Actor* actor, const ImRect& row, bool flat, bool treeOpen, float rowX);
		void DrawNodeLabel(Actor* actor, const ImRect& row, bool flat, float rowX);
		void AcceptRootDrop();
		void DrawDropIndicator();
		void ApplyPendingOps();
		void HandleKeys();

		static DropZone GetDropZone(const ImRect& row, bool reorder);
		static DropTarget ResolveDrop(Actor* actor, DropZone zone, bool open);
		static bool CanMove(const Actor* dragged, const Actor* target, const DropTarget& t);

		std::string mFilter;
		std::vector<Row> mRows;
		int mRowCount = 0;
		ActorId mLastSelected = 0;
		bool mReveal = false;
		ActorId mToggleId = 0;
		bool mToggleOpen = false;
		ActorId mRenamingId = 0;
		bool mRenameFocus = false;
		int mRenameWait = 0;
		std::string mRenameBuffer;
		bool mFocused = false;
		PendingOps mOps;
		DropIndicator mDrop;
	};
}
