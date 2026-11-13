#include "RdPch.h"
#include "Scene/Actor.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"

namespace rd
{
	// ==================================================
	// Component
	// ==================================================
	Scene* Component::GetScene() const { return mActor ? mActor->GetScene() : nullptr; }
	Transform& Component::GetTransform() const { return mActor->GetTransform(); }
	bool Component::IsActiveAndEnabled() const { return mEnabled && !mPendingDestroy && mActor && mActor->IsActive(); }

	// ==================================================
	// Actor
	// ==================================================
	Actor::Actor(Scene* scene, ActorId id)
		: mScene(scene)
		, mId(id)
		, mTransform(this)
	{
	}

	Actor::~Actor() = default;

	bool Actor::IsActive() const
	{
		for (const Actor* a = this; a; a = a->mParent)
		{
			if (!a->mActive || a->mPendingDestroy) { return false; }
		}
		return true;
	}

	void Actor::SetParent(Actor* parent, bool keepWorld, int index)
	{
		if (parent == this || (parent && parent->IsDescendantOf(this)))
		{
			Log::Warning(LogCat::Scene, "Cannot parent '{}' to its own descendant", mName);
			return;
		}
		Matrix4 world = mTransform.GetWorldMatrix();

		// 旧親から外す
		auto& oldList = mParent ? mParent->mChildren : mScene->mRootActors;
		oldList.erase(std::remove(oldList.begin(), oldList.end(), this), oldList.end());

		mParent = parent;
		auto& newList = mParent ? mParent->mChildren : mScene->mRootActors;
		if (index < 0 || index > int(newList.size())) { newList.push_back(this); }
		else { newList.insert(newList.begin() + index, this); }

		if (keepWorld) { mTransform.SetWorldMatrix(world); }
		else { mTransform.MarkDirty(); }
	}

	bool Actor::IsDescendantOf(const Actor* other) const
	{
		for (const Actor* a = mParent; a; a = a->mParent)
		{
			if (a == other) { return true; }
		}
		return false;
	}

	Actor* Actor::FindChild(const std::string& name, bool recursive) const
	{
		for (Actor* child : mChildren)
		{
			if (child->mName == name) { return child; }
			if (recursive)
			{
				if (Actor* found = child->FindChild(name, true)) { return found; }
			}
		}
		return nullptr;
	}

	int Actor::GetSiblingIndex() const
	{
		const auto& list = mParent ? mParent->mChildren : mScene->mRootActors;
		auto it = std::find(list.begin(), list.end(), this);
		return it == list.end() ? -1 : int(it - list.begin());
	}

	Component* Actor::AddComponent(std::unique_ptr<Component> component, bool awake)
	{
		if (!component) { return nullptr; }
		Component* raw = component.get();
		raw->mActor = this;
		mComponents.push_back(std::move(component));
		mScene->RegisterComponent(raw);// 型ごとのリストと呼び出しリストへ
		if (awake) { raw->Awake(); }
		return raw;
	}

	Component* Actor::AddComponent(const std::string& typeName)
	{
		return AddComponent(ComponentRegistry::Create(typeName));
	}

	void Actor::RemoveComponent(Component* component)
	{
		if (component && component->mActor == this && !component->mPendingDestroy)
		{
			component->mPendingDestroy = true;
			mScene->mHasPendingDestroy = true;
		}
	}

	void Actor::MoveComponent(Component* component, int delta)
	{
		for (size_t i = 0; i < mComponents.size(); ++i)
		{
			if (mComponents[i].get() != component) { continue; }
			int target = int(i) + delta;
			if (target >= 0 && target < int(mComponents.size())) { std::swap(mComponents[i], mComponents[size_t(target)]); }
			return;
		}
	}

	void Actor::Destroy()
	{
		if (mPendingDestroy) { return; }
		mPendingDestroy = true;
		mScene->mHasPendingDestroy = true;
		for (Actor* child : mChildren) { child->Destroy(); }
	}

	void Actor::FlushDestroyedComponents()
	{
		for (auto it = mComponents.begin(); it != mComponents.end();)
		{
			if ((*it)->mPendingDestroy)
			{
				(*it)->OnDestroy();
				it = mComponents.erase(it);
			}
			else
			{
				++it;
			}
		}
	}
}
