#pragma once
#include "Scene/Actor/Actor.h"
#include "Scene/Reference/ObjectRef.h"

namespace rd
{
	// コンポーネントへの参照（参照先のアクターの、最初の T を指す）
	template <class T>
	class ComponentRef final : public ObjectRef
	{
	public:
		ComponentRef() : ObjectRef(T::kTypeName, HasComponent) {}
		ComponentRef(T* component) : ComponentRef() { *this = component; }
		ComponentRef& operator=(T* component)
		{
			SetActor(component ? component->GetActor() : nullptr);
			return *this;
		}

		T* Get() const
		{
			Actor* actor = GetActor();
			return actor ? actor->GetComponent<T>() : nullptr;
		}
		T* operator->() const { return Get(); }

	private:
		static bool HasComponent(const Actor* actor) { return actor->GetComponent<T>() != nullptr; }
	};
}
