#pragma once
#include "Scene/Component.h"
#include "Scene/Transform.h"
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class Scene;
	using ActorId = uint64_t;

	// シーン上のオブジェクト。Transform を持ち、コンポーネントで機能を追加する
	class Actor
	{
	public:
		Actor(Scene* scene, ActorId id);
		~Actor();
		Actor(const Actor&) = delete;
		Actor& operator=(const Actor&) = delete;

		ActorId GetId() const { return mId; }
		Scene* GetScene() const { return mScene; }
		const std::string& GetName() const { return mName; }
		void SetName(std::string name) { mName = std::move(name); }
		const std::string& GetTag() const { return mTag; }
		void SetTag(std::string tag) { mTag = std::move(tag); }
		uint32_t GetLayer() const { return mLayer; }
		void SetLayer(uint32_t layer) { mLayer = layer < 32 ? layer : 0; }

		bool IsActiveSelf() const { return mActive; }
		bool IsActive() const;// 親も含めて有効か
		void SetActive(bool active) { mActive = active; }
		bool IsPendingDestroy() const { return mPendingDestroy; }

		Transform& GetTransform() { return mTransform; }
		const Transform& GetTransform() const { return mTransform; }

		// 階層
		Actor* GetParent() const { return mParent; }
		const std::vector<Actor*>& GetChildren() const { return mChildren; }
		// keepWorld=true ならワールド姿勢を保ったまま付け替える
		void SetParent(Actor* parent, bool keepWorld = true, int index = -1);
		bool IsDescendantOf(const Actor* other) const;
		Actor* FindChild(const std::string& name, bool recursive = true) const;
		int GetSiblingIndex() const;

		// コンポーネント
		template <class T>
		T* AddComponent()
		{
			auto comp = std::make_unique<T>();
			T* raw = comp.get();
			AddComponent(std::move(comp));
			return raw;
		}
		Component* AddComponent(std::unique_ptr<Component> component, bool awake = true);
		Component* AddComponent(const std::string& typeName);
		template <class T>
		T* GetComponent() const
		{
			for (auto& c : mComponents)
			{
				if (auto p = dynamic_cast<T*>(c.get()); p && !c->mPendingDestroy) { return p; }
			}
			return nullptr;
		}
		template <class T>
		T* GetComponentInChildren() const
		{
			if (auto c = GetComponent<T>()) { return c; }
			for (Actor* child : mChildren)
			{
				if (auto c = child->GetComponentInChildren<T>()) { return c; }
			}
			return nullptr;
		}
		template <class T>
		std::vector<T*> GetComponents() const
		{
			std::vector<T*> result;
			for (auto& c : mComponents)
			{
				if (auto p = dynamic_cast<T*>(c.get()); p && !c->mPendingDestroy) { result.push_back(p); }
			}
			return result;
		}
		const std::vector<std::unique_ptr<Component>>& GetAllComponents() const { return mComponents; }
		void RemoveComponent(Component* component);// フレーム終わりに破棄
		void MoveComponent(Component* component, int delta);

		// 破棄（フレーム終わりに子も含めて破棄される）
		void Destroy();

	private:
		friend class Scene;
		void FlushDestroyedComponents();

		Scene* mScene;
		ActorId mId;
		std::string mName = "Actor";
		std::string mTag;
		uint32_t mLayer = 0;
		bool mActive = true;
		bool mPendingDestroy = false;
		Transform mTransform;
		Actor* mParent = nullptr;
		std::vector<Actor*> mChildren;
		std::vector<std::unique_ptr<Component>> mComponents;
	};

	template <class T>
	T* Component::GetComponent() const
	{
		return mActor ? mActor->GetComponent<T>() : nullptr;
	}
}
