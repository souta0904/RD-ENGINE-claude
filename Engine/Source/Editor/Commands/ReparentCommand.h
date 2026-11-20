#pragma once
#include "Editor/Commands/EditorCommand.h"

namespace rd
{
	// 親子の付け替えと兄弟の中での並び替え（ワールドでの位置は保つ。付け替える直前に作る）
	class RD_API ReparentCommand final : public EditorCommand
	{
	public:
		ReparentCommand(std::string label, Actor* actor, Actor* newParent, int newIndex = -1);
		void Undo(Scene& scene) override { Apply(scene, mOldParent, mOldIndex); }
		void Redo(Scene& scene) override { Apply(scene, mNewParent, mNewIndex); }

	private:
		void Apply(Scene& scene, ActorId parent, int index);
		ActorId mActor;
		ActorId mOldParent;
		int mOldIndex;
		ActorId mNewParent;
		int mNewIndex;
	};
}
