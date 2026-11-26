#include "RdPch.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Commands/ActorStateCommand.h"
#include "Editor/Commands/SceneSettingsCommand.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Editor.h"
#include "Scene/Scene.h"

namespace rd
{
	void EditorUndo::RefreshSnapshots()
	{
		Scene& scene = mEditor.GetScene();
		Actor* actor = mEditor.GetSelection().Get();
		mSnapshotActor = actor ? actor->GetId() : 0;
		mActorSnapshot = actor ? scene.SerializeActorState(actor) : Json();
		mSettingsSnapshot = scene.SerializeSettings();
	}

	void EditorUndo::CommitActorEdit(const std::string& label)
	{
		if (mEditor.IsPlaying()) { return; }
		Actor* actor = mEditor.GetSelection().Get();
		if (!actor || actor->GetId() != mSnapshotActor) { RefreshSnapshots(); return; }
		Json after = mEditor.GetScene().SerializeActorState(actor);
		if (after == mActorSnapshot) { return; }
		Push(std::make_unique<ActorStateCommand>(label, actor->GetId(), std::move(mActorSnapshot), after));
	}

	void EditorUndo::CommitSettingsEdit(const std::string& label)
	{
		if (mEditor.IsPlaying()) { return; }
		Json after = mEditor.GetScene().SerializeSettings();
		if (after == mSettingsSnapshot) { return; }
		Push(std::make_unique<SceneSettingsCommand>(label, std::move(mSettingsSnapshot), after));
	}

	void EditorUndo::Push(std::unique_ptr<EditorCommand> command)
	{
		if (mEditor.IsPlaying()) { return; }
		mHistory.Push(std::move(command));
		RefreshSnapshots();
		mEditor.UpdateTitle();
	}

	// 戻した操作の対象を選び直す
	void EditorUndo::Undo()
	{
		if (mEditor.IsPlaying() || !mHistory.CanUndo()) { return; }
		mEditor.GetSelection().SetId(mHistory.Undo(mEditor.GetScene()));
		RefreshSnapshots();
		mEditor.UpdateTitle();
	}

	void EditorUndo::Redo()
	{
		if (mEditor.IsPlaying() || !mHistory.CanRedo()) { return; }
		mEditor.GetSelection().SetId(mHistory.Redo(mEditor.GetScene()));
		RefreshSnapshots();
		mEditor.UpdateTitle();
	}
}
