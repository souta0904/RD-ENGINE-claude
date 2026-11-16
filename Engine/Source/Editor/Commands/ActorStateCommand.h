#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/EditorCommand.h"
#include <chrono>

namespace rd
{
	// アクター 1 つの状態の変更（名前・Transform・コンポーネントの値・追加・削除・並び替え）
	//  同じアクターへの同じ種類の操作が短い間隔で続いたら 1 つにまとめる
	class ActorStateCommand final : public EditorCommand
	{
	public:
		ActorStateCommand(std::string label, ActorId actor, Json before, Json after);
		void Undo(Scene& scene) override;
		void Redo(Scene& scene) override;
		bool MergeWith(const EditorCommand& next) override;

	private:
		ActorId mActor;
		Json mBefore;
		Json mAfter;
		std::chrono::steady_clock::time_point mTime = std::chrono::steady_clock::now();
	};
}
