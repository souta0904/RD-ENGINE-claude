#include "RdPch.h"
#include "Editor/Commands/CommandHistory.h"
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
		// 保存した時点が捨てる Redo の側にあったら、もうその時点には戻れない
		if (mCursor < mCommands.size())
		{
			mCommands.resize(mCursor);
			if (mSavedCursor > mCursor) { mSavedCursor = kUnreachable; }
		}
		// 保存した直後は、保存した時点を残すためにまとめない
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

	// 保存した時点と今の位置の間にあるコマンドを見る（どちらが前でも、間のものを戻すかやり直すと保存した状態になる）
	bool CommandHistory::IsDirty() const
	{
		if (mSavedCursor == kUnreachable) { return true; }
		size_t from = std::min(mCursor, mSavedCursor);
		size_t to = std::min(std::max(mCursor, mSavedCursor), mCommands.size());
		for (size_t i = from; i < to; ++i)
		{
			if (mCommands[i]->ChangesScene()) { return true; }
		}
		return false;
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
