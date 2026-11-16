#include "RdPch.h"
#include "Scene/Component/Component.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	Scene* Component::GetScene() const { return mActor ? mActor->GetScene() : nullptr; }
	Transform& Component::GetTransform() const { return mActor->GetTransform(); }
	bool Component::IsActiveAndEnabled() const { return mEnabled && !mPendingDestroy && mActor && mActor->IsActive(); }
}
