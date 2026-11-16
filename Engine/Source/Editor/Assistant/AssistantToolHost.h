#pragma once
#include "Editor/Commands/EditorCommand.h"
#include <memory>

namespace rd
{
	class Actor;
	class Scene;

	// AI アシスタントのツールが使うエディタの機能（テストでは偽物に差し替える）
	class AssistantToolHost
	{
	public:
		virtual ~AssistantToolHost() = default;
		virtual Scene& GetScene() = 0;
		virtual void PushCommand(std::unique_ptr<EditorCommand> command) = 0;
		virtual Actor* GetSelected() = 0;
		virtual void Select(Actor* actor) = 0;
		virtual bool IsPlaying() = 0;
	};
}
