#include "RdPch.h"
#include "Editor/EditorCommands.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace
	{
		// この時間内に同じ操作が続いたら 1 つの Undo にまとめる
		constexpr auto kMergeWindow = std::chrono::milliseconds(600);
	}

	// ==================================================
	// ActorStateCommand
	// ==================================================
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

	// ==================================================
	// ActorLifetimeCommand
	// ==================================================
	std::unique_ptr<ActorLifetimeCommand> ActorLifetimeCommand::Created(std::string label, Scene& scene, const std::vector<Actor*>& actors)
	{
		std::unique_ptr<ActorLifetimeCommand> cmd(new ActorLifetimeCommand(std::move(label), true));
		cmd->Capture(scene, actors);
		cmd->mSelection = actors.empty() ? 0 : actors.back()->GetId();
		return cmd;
	}

	std::unique_ptr<ActorLifetimeCommand> ActorLifetimeCommand::Deleting(std::string label, Scene& scene, const std::vector<Actor*>& actors)
	{
		std::unique_ptr<ActorLifetimeCommand> cmd(new ActorLifetimeCommand(std::move(label), false));
		cmd->Capture(scene, actors);
		cmd->mSelection = actors.empty() ? 0 : actors.back()->GetId();// Undo（復元）したら選択し直す
		return cmd;
	}

	void ActorLifetimeCommand::Capture(Scene& scene, const std::vector<Actor*>& actors)
	{
		for (Actor* actor : actors)
		{
			Entry e;
			e.actors = scene.SerializeActors({ actor });
			e.root = actor->GetId();
			e.parent = actor->GetParent() ? actor->GetParent()->GetId() : 0;
			e.siblingIndex = actor->GetSiblingIndex();
			mEntries.push_back(std::move(e));
		}
	}

	void ActorLifetimeCommand::Restore(Scene& scene)
	{
		for (const Entry& e : mEntries)
		{
			Actor* parent = e.parent ? scene.FindActor(e.parent) : nullptr;
			scene.DeserializeActors(e.actors, false, parent);// ID はそのまま（参照もそのまま生きる）
			if (Actor* root = scene.FindActor(e.root)) { root->SetParent(parent, false, e.siblingIndex); }
		}
	}

	void ActorLifetimeCommand::Remove(Scene& scene)
	{
		// 後から作ったものから消す（子を先に消しても親の削除で一緒に消えるので順番は問わない）
		for (auto it = mEntries.rbegin(); it != mEntries.rend(); ++it)
		{
			if (Actor* actor = scene.FindActor(it->root)) { actor->Destroy(); }
		}
		scene.FlushPendingDestroy();
	}

	// ==================================================
	// ReparentCommand
	// ==================================================
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

	// ==================================================
	// SceneSettingsCommand / CompositeCommand
	// ==================================================
	void SceneSettingsCommand::Undo(Scene& scene) { scene.ApplySettings(mBefore); }
	void SceneSettingsCommand::Redo(Scene& scene) { scene.ApplySettings(mAfter); }

	void CompositeCommand::Undo(Scene& scene)
	{
		for (auto it = mCommands.rbegin(); it != mCommands.rend(); ++it) { (*it)->Undo(scene); }
	}

	void CompositeCommand::Redo(Scene& scene)
	{
		for (auto& c : mCommands) { c->Redo(scene); }
	}
}
