#pragma once
#include "Scene/Reference/ObjectRef.h"

namespace rd
{
	// アクターへの参照
	class ActorRef final : public ObjectRef
	{
	public:
		ActorRef() : ObjectRef("Actor", nullptr) {}
		ActorRef(Actor* actor) : ActorRef() { SetActor(actor); }
		ActorRef& operator=(Actor* actor) { SetActor(actor); return *this; }

		Actor* Get() const { return GetActor(); }
		Actor* operator->() const { return GetActor(); }
	};
}
