#include "RdPch.h"
#include "Editor/Assistant/EditorToolHost.h"
#include "Editor/Editor.h"

namespace rd
{
	Scene& EditorToolHost::GetScene() { return mEditor.GetScene(); }
	void EditorToolHost::PushCommand(std::unique_ptr<EditorCommand> command) { mEditor.PushCommand(std::move(command)); }
	Actor* EditorToolHost::GetSelected() { return mEditor.GetSelected(); }
	void EditorToolHost::Select(Actor* actor) { mEditor.Select(actor); }
	bool EditorToolHost::IsPlaying() { return mEditor.IsPlaying(); }
}
