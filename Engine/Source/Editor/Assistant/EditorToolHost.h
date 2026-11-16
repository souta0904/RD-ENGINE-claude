#pragma once
#include "Editor/Assistant/AssistantToolHost.h"

namespace rd
{
	class Editor;

	// 本物のエディタにつなぐ AssistantToolHost
	class EditorToolHost final : public AssistantToolHost
	{
	public:
		explicit EditorToolHost(Editor& editor) : mEditor(editor) {}
		Scene& GetScene() override;
		void PushCommand(std::unique_ptr<EditorCommand> command) override;
		Actor* GetSelected() override;
		void Select(Actor* actor) override;
		bool IsPlaying() override;

	private:
		Editor& mEditor;
	};
}
