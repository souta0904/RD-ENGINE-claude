#include "RdPch.h"
#include "Editor/Core/EditorSelection.h"
#include "Editor/Commands/EditorUndo.h"
#include "Editor/Editor.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Scene/Scene.h"

namespace rd
{
	Actor* EditorSelection::Get() const
	{
		return mId ? mEditor.GetScene().FindActor(mId) : nullptr;
	}

	void EditorSelection::Select(Actor* actor)
	{
		mId = actor ? actor->GetId() : 0;
		if (actor) { mEditor.GetProject().ClearSelection(); }
		mEditor.GetUndo().RefreshSnapshots();
	}
}
