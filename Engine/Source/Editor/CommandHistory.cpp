#include "RdPch.h"
#include "Editor/CommandHistory.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		constexpr size_t kUnreachable = size_t(-1);
	}

	void CommandHistory::Push(std::unique_ptr<EditorCommand> command)
	{
		if (!command) { return; }
		// Redo 側を捨てる（保存した時点がそこにあったら、もう戻れない）
		if (mCursor < mCommands.size())
		{
			mCommands.resize(mCursor);
			if (mSavedCursor > mCursor) { mSavedCursor = kUnreachable; }
		}
		// 保存した直後でなければ、直前のコマンドにまとめられる
		if (mCursor > 0 && mSavedCursor != mCursor && mCommands[mCursor - 1]->MergeWith(*command))
		{
			Log::Debug(LogCat::Editor, "Merged: {}", command->GetLabel());
			return;
		}
		Log::Debug(LogCat::Editor, "Command: {}", command->GetLabel());
		mCommands.push_back(std::move(command));
		if (mCommands.size() > kMaxCommands)
		{
			mCommands.erase(mCommands.begin());
			mSavedCursor = mSavedCursor == 0 || mSavedCursor == kUnreachable ? kUnreachable : mSavedCursor - 1;
		}
		mCursor = mCommands.size();
	}

	void CommandHistory::Clear()
	{
		mCommands.clear();
		mCursor = 0;
		mSavedCursor = 0;
	}

	ActorId CommandHistory::Undo(Scene& scene)
	{
		if (!CanUndo()) { return 0; }
		EditorCommand& c = *mCommands[--mCursor];
		c.Undo(scene);
		Log::Info(LogCat::Editor, "Undo: {}", c.GetLabel());
		return c.GetSelection();
	}

	ActorId CommandHistory::Redo(Scene& scene)
	{
		if (!CanRedo()) { return 0; }
		EditorCommand& c = *mCommands[mCursor++];
		c.Redo(scene);
		Log::Info(LogCat::Editor, "Redo: {}", c.GetLabel());
		return c.GetSelection();
	}

	const std::string& CommandHistory::GetUndoLabel() const
	{
		static const std::string kEmpty;
		return CanUndo() ? mCommands[mCursor - 1]->GetLabel() : kEmpty;
	}

	const std::string& CommandHistory::GetRedoLabel() const
	{
		static const std::string kEmpty;
		return CanRedo() ? mCommands[mCursor]->GetLabel() : kEmpty;
	}
}
