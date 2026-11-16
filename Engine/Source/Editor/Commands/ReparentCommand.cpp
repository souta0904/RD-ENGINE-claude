#include "RdPch.h"
#include "Editor/Commands/ReparentCommand.h"
#include "Scene/Scene.h"

namespace rd
{
	ReparentCommand::ReparentCommand(std::string label, Actor* actor, Actor* newParent, int newIndex)
		: EditorCommand(std::move(label))
		, mActor(actor->GetId())
		, mOldParent(actor->GetParent() ? actor->GetParent()->GetId() : 0)
		, mOldIndex(actor->GetSiblingIndex())
		, mNewParent(newParent ? newParent->GetId() : 0)
		, mNewIndex(newIndex)
	{
		mSelection = mActor;
	}

	void ReparentCommand::Apply(Scene& scene, ActorId parent, int index)
	{
		Actor* actor = scene.FindActor(mActor);
		if (!actor) { return; }
		actor->SetParent(parent ? scene.FindActor(parent) : nullptr, true, index);
	}
}
