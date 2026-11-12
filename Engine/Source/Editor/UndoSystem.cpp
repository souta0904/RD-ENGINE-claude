#include "RdPch.h"
#include "Editor/UndoSystem.h"
#include "Scene/Scene.h"

namespace rd
{
	void UndoSystem::Reset(const Scene& scene)
	{
		mEntries.clear();
		mCursor = 0;
		mStable = scene.Serialize().dump();
		mDirty = false;
	}

	void UndoSystem::Commit(const Scene& scene, const std::string& label, uint64_t selection)
	{
		std::string after = scene.Serialize().dump();
		if (after == mStable) { return; }
		mEntries.resize(mCursor);// Redo 側を捨てる
		mEntries.push_back({ label, mStable, after, selection });
		if (mEntries.size() > kMaxEntries) { mEntries.erase(mEntries.begin()); }
		mCursor = mEntries.size();
		mStable = std::move(after);
		mDirty = true;
	}

	uint64_t UndoSystem::Undo(Scene& scene)
	{
		if (!CanUndo()) { return 0; }
		const Entry& e = mEntries[--mCursor];
		std::string path = scene.GetPath();
		scene.Deserialize(Json::parse(e.before));
		scene.SetPath(path);
		mStable = e.before;
		mDirty = true;
		Log::Info("Undo: {}", e.label);
		return e.selection;
	}

	uint64_t UndoSystem::Redo(Scene& scene)
	{
		if (!CanRedo()) { return 0; }
		const Entry& e = mEntries[mCursor++];
		std::string path = scene.GetPath();
		scene.Deserialize(Json::parse(e.after));
		scene.SetPath(path);
		mStable = e.after;
		mDirty = true;
		Log::Info("Redo: {}", e.label);
		return e.selection;
	}

	const std::string& UndoSystem::GetUndoLabel() const
	{
		static const std::string kEmpty;
		return CanUndo() ? mEntries[mCursor - 1].label : kEmpty;
	}

	const std::string& UndoSystem::GetRedoLabel() const
	{
		static const std::string kEmpty;
		return CanRedo() ? mEntries[mCursor].label : kEmpty;
	}
}
