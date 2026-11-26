#include "RdPch.h"
#include "Editor/Assistant/EditorToolHost.h"
#include "Editor/Editor.h"

namespace rd
{
	Scene& EditorToolHost::GetScene() { return mEditor.GetScene(); }
	void EditorToolHost::PushCommand(std::unique_ptr<EditorCommand> command) { mEditor.GetUndo().Push(std::move(command)); }
	Actor* EditorToolHost::GetSelected() { return mEditor.GetSelection().Get(); }
	void EditorToolHost::Select(Actor* actor) { mEditor.GetSelection().Select(actor); }
	bool EditorToolHost::IsPlaying() { return mEditor.IsPlaying(); }
}
