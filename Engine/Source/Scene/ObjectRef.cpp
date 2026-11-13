#include "RdPch.h"
#include "Scene/ObjectRef.h"
#include "Scene/Scene.h"

namespace rd
{
	Actor* ObjectRef::GetActor() const
	{
		return (mId != 0 && mScene) ? mScene->FindActor(mId) : nullptr;
	}

	void ObjectRef::SetActor(Actor* actor)
	{
		mId = actor ? actor->GetId() : 0;
		if (actor) { mScene = actor->GetScene(); }
	}
}
