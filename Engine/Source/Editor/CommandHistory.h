#pragma once
#include "Editor/EditorCommands.h"
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class Scene;

	// Undo / Redo の履歴
	//  エディタは操作を実行した後、その操作を表すコマンドを Push する。
	//  各コマンドは変更した部分だけを持つので、シーン全体を保存し直すより軽い。
	class CommandHistory
	{
	public:
		// 実行済みのコマンドを積む（Redo 側は捨てる。直前のものとまとめられればまとめる）
		void Push(std::unique_ptr<EditorCommand> command);
		void Clear();

		bool CanUndo() const { return mCursor > 0; }
		bool CanRedo() const { return mCursor < mCommands.size(); }
		// シーンを書き換え、選択すべきアクター ID を返す
		ActorId Undo(Scene& scene);
		ActorId Redo(Scene& scene);

		const std::string& GetUndoLabel() const;
		const std::string& GetRedoLabel() const;

		// 保存した時点から変わっているか
		bool IsDirty() const { return mCursor != mSavedCursor; }
		void MarkSaved() { mSavedCursor = mCursor; }

	private:
		static constexpr size_t kMaxCommands = 200;
		std::vector<std::unique_ptr<EditorCommand>> mCommands;
		size_t mCursor = 0;// ここより前が Undo できるもの
		size_t mSavedCursor = 0;// 保存した時点の位置（保存後に履歴が捨てられたら届かない値になる）
	};
}
