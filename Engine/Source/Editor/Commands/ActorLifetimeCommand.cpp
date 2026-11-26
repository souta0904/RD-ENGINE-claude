#include "RdPch.h"
#include "Editor/Commands/ActorLifetimeCommand.h"
#include "Scene/Scene.h"
#include "Scene/Serialization/SceneSerializer.h"

namespace rd
{
	std::unique_ptr<ActorLifetimeCommand> ActorLifetimeCommand::Created(std::string label, const std::vector<Actor*>& actors)
	{
		std::unique_ptr<ActorLifetimeCommand> cmd(new ActorLifetimeCommand(std::move(label), true));
		cmd->Capture(actors);
		cmd->mSelection = actors.empty() ? 0 : actors.back()->GetId();
		return cmd;
	}

	std::unique_ptr<ActorLifetimeCommand> ActorLifetimeCommand::Deleting(std::string label, const std::vector<Actor*>& actors)
	{
		std::unique_ptr<ActorLifetimeCommand> cmd(new ActorLifetimeCommand(std::move(label), false));
		cmd->Capture(actors);
		cmd->mSelection = actors.empty() ? 0 : actors.back()->GetId();
		return cmd;
	}

	void ActorLifetimeCommand::Capture(const std::vector<Actor*>& actors)
	{
		for (Actor* actor : actors)
		{
			Entry e;
			e.actors = SceneSerializer::SerializeActors({ actor });
			e.root = actor->GetId();
			e.parent = actor->GetParent() ? actor->GetParent()->GetId() : 0;
			e.siblingIndex = actor->GetSiblingIndex();
			mEntries.push_back(std::move(e));
		}
	}

	// ID はそのまま作り直すので、このアクターを指していた参照もそのまま生きる
	void ActorLifetimeCommand::Restore(Scene& scene)
	{
		for (const Entry& e : mEntries)
		{
			Actor* parent = e.parent ? scene.FindActor(e.parent) : nullptr;
			SceneSerializer::DeserializeActors(scene, e.actors, false, parent);
			if (Actor* root = scene.FindActor(e.root)) { root->SetParent(parent, false, e.siblingIndex); }
		}
	}

	void ActorLifetimeCommand::Remove(Scene& scene)
	{
		for (auto it = mEntries.rbegin(); it != mEntries.rend(); ++it)
		{
			if (Actor* actor = scene.FindActor(it->root)) { actor->Destroy(); }
		}
		scene.FlushPendingDestroy();
	}
}
