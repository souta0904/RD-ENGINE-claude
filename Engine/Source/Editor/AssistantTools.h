#pragma once
#include "Core/JsonArchive.h"
#include "Editor/EditorCommands.h"
#include <memory>
#include <string>

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

	// AI アシスタントが使うツール（シーンの読み書き）
	//  変更するツールは、エディタの通常の操作と同じコマンドを積むので Ctrl+Z で戻せる
	namespace AssistantTools
	{
		// Messages API の tools にそのまま渡せる定義
		const Json& GetDefinitions();
		// ツールを実行して、結果（tool_result の content）を返す。失敗したら isError=true
		std::string Run(AssistantToolHost& host, const std::string& name, const Json& input, bool& isError);
	}
}
