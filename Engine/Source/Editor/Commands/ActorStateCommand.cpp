#include "RdPch.h"
#include "Editor/Commands/ActorStateCommand.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		constexpr auto kMergeWindow = std::chrono::milliseconds(600);
	}

	ActorStateCommand::ActorStateCommand(std::string label, ActorId actor, Json before, Json after)
		: EditorCommand(std::move(label))
		, mActor(actor)
		, mBefore(std::move(before))
		, mAfter(std::move(after))
	{
		mSelection = actor;
	}

	void ActorStateCommand::Undo(Scene& scene)
	{
		if (Actor* actor = scene.FindActor(mActor)) { scene.ApplyActorState(actor, mBefore); }
	}

	void ActorStateCommand::Redo(Scene& scene)
	{
		if (Actor* actor = scene.FindActor(mActor)) { scene.ApplyActorState(actor, mAfter); }
	}

	bool ActorStateCommand::MergeWith(const EditorCommand& next)
	{
		auto other = dynamic_cast<const ActorStateCommand*>(&next);
		if (!other || other->mActor != mActor || other->mLabel != mLabel) { return false; }
		if (other->mTime - mTime > kMergeWindow) { return false; }
		mAfter = other->mAfter;
		mTime = other->mTime;
		return true;
	}
}
