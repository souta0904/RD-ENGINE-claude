#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace rd
{
	class Scene;

	// シーン全体のスナップショットによる Undo/Redo
	//  編集が確定するたびに Commit を呼ぶ。直前の安定状態との差分ではなく全体を保存するので
	//  作成・削除・親子付け替え・プロパティ変更のすべてを同じ仕組みで元に戻せる。
	class UndoSystem
	{
	public:
		// シーンの読み込みやプレイ停止の後に呼ぶ（履歴をリセット）
		void Reset(const Scene& scene);
		// 編集を確定して履歴に積む
		void Commit(const Scene& scene, const std::string& label, uint64_t selection);

		bool CanUndo() const { return mCursor > 0; }
		bool CanRedo() const { return mCursor < mEntries.size(); }
		// シーンを書き換え、復元すべき選択アクター ID を返す
		uint64_t Undo(Scene& scene);
		uint64_t Redo(Scene& scene);

		const std::string& GetUndoLabel() const;
		const std::string& GetRedoLabel() const;
		bool IsDirty() const { return mDirty; }
		void ClearDirty() { mDirty = false; }

	private:
		struct Entry
		{
			std::string label;
			std::string before;
			std::string after;
			uint64_t selection = 0;
		};
		static constexpr size_t kMaxEntries = 100;
		std::vector<Entry> mEntries;
		size_t mCursor = 0;
		std::string mStable;// 最後に確定した状態
		bool mDirty = false;
	};
}
