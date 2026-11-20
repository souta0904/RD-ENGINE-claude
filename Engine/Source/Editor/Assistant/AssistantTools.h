#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Assistant/AssistantToolHost.h"
#include <string>

// AI アシスタントが使うツール（シーンの読み書き）
//  変えるツールは、エディタの普段の操作と同じコマンドを積むので Ctrl+Z で戻せる
namespace rd::AssistantTools
{
	// Messages API の tools にそのまま渡せる定義
	RD_API const Json& GetDefinitions();
	// ツールを実行して、結果（tool_result の content）を返す。失敗したら isError=true
	RD_API std::string Run(AssistantToolHost& host, const std::string& name, const Json& input, bool& isError);
}
