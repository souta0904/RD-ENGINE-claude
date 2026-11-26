#pragma once
#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "Editor/Panels/EditorPanel.h"
#include <string>
#include <unordered_map>
#include <vector>

namespace ax::NodeEditor { struct EditorContext; }

namespace rd
{
	// 行動ツリー（.rdbt）をノードで組むパネル（thedmd/imgui-node-editor を使う）
	//  上から下へ流れる木: 親の下の出口から子の上の入口へつなぐ。Composite（Sequence・Selector）の子は
	//  左にあるものから順に動く（Unreal の Behavior Tree と同じ）。
	//  プレイ中は、選んでいるアクター（いなければ最初に見つかったもの）のツリーの動いているノードを光らせる
	class RD_API BehaviorTreePanel final : public EditorPanel
	{
	public:
		explicit BehaviorTreePanel(Editor& editor);
		~BehaviorTreePanel() override;

		// ファイルを開く（今のファイルに保存していない変更があれば先に聞く）
		void Open(const std::string& path);
		const std::string& GetPath() const { return mPath; }
		bool IsDirty() const { return mDirty; }
		bool HandleShortcuts() override;

	protected:
		void OnGui() override;
		bool HasPadding() const override { return false; }
		ImVec2 GetDefaultSize() const override { return ImVec2(ImGui::GetFontSize() * 70.0f, ImGui::GetFontSize() * 40.0f); }
		// 初めて開いたときは、シーンビューと同じ所にタブとして入れる（広く使えるように）
		const char* GetDefaultDockWindow() const override { return "Scene"; }

	private:
		struct NodeFlash
		{
			uint64_t ticks = 0;
			double time = -100.0;
			bool wasRunning = false;
		};

		void LoadFile(const std::string& path);
		bool Save();
		void DrawToolbar();
		void DrawCanvas(float width, float height);
		void DrawNode(bt::NodeData& node, const bt::TreeInstance* debug, double now);
		void DrawLinks(const bt::TreeInstance* debug);
		void HandleCreateAndDelete();
		void DrawContextMenus(const ImVec2& mouseCanvas);
		void DrawCreateMenuItems();
		void DrawSidebar(float width, float height);
		bool DrawParam(const bt::ParamDef& def, Json& value);
		void DrawSavePrompt();

		// つなぐ・外す
		bool CanConnect(uint32_t parent, uint32_t child) const;
		void Connect(uint32_t parent, int slot, uint32_t child);
		void Detach(uint32_t child);
		uint32_t CreateNode(const std::string& type, const Vector2& position);
		void DuplicateSelection();
		void DeleteSelection();

		// 変更の確定と Undo（ツリー全体の JSON を積む）
		void MarkChanged() { mChanged = true; }
		void CommitIfSettled();
		void ApplySnapshot(const std::string& json);
		void UndoEdit();
		void RedoEdit();

		const bt::TreeInstance* FindDebugInstance() const;
		std::vector<uint32_t> GetSelectedIds() const;

		ax::NodeEditor::EditorContext* mContext = nullptr;
		bt::TreeAsset mAsset;
		std::string mPath;
		bool mLoaded = false;
		bool mDirty = false;
		bool mChanged = false;
		std::string mCommitted;
		std::vector<std::string> mUndo;
		std::vector<std::string> mRedo;

		// 次のフレームでノードの位置をエディタへ入れ直す（読み込み・Undo・整列の後）
		bool mPushPositions = false;
		bool mLayoutPending = false;
		int mNavigateFrames = 0;
		bool mFocused = false;

		// 作成のメニュー
		ImVec2 mCreatePos{ 0.0f, 0.0f };
		uint64_t mCreateFromPin = 0;
		std::string mCreateFilter;
		bool mFocusCreateFilter = false;
		uint32_t mContextNode = 0;
		uint32_t mContextLink = 0;

		// 保存していない変更があるときに別のファイルを開こうとした
		std::string mPendingOpen;
		bool mOpenSavePrompt = false;

		std::unordered_map<uint32_t, NodeFlash> mFlash;
		std::string mDebugOwner;
	};
}
