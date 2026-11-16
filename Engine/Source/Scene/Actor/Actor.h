#pragma once
#include "Scene/Component/Component.h"
#include "Scene/Transform/Transform.h"
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	class Scene;
	using ActorId = uint64_t;

	// シーンの中の物体（Transform を持ち、コンポーネントで機能を足す）
	//  破棄やコンポーネントの削除はフレームの終わりにまとめて行う
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
		// 親も含めて有効か
		bool IsActive() const;
		void SetActive(bool active) { mActive = active; }
		bool IsPendingDestroy() const { return mPendingDestroy; }

		Transform& GetTransform() { return mTransform; }
		const Transform& GetTransform() const { return mTransform; }

		Actor* GetParent() const { return mParent; }
		const std::vector<Actor*>& GetChildren() const { return mChildren; }
		// keepWorld なら、ワールドでの姿勢を保ったまま付け替える。index は兄弟の中の位置（-1 で末尾）
		void SetParent(Actor* parent, bool keepWorld = true, int index = -1);
		bool IsDescendantOf(const Actor* other) const;
		Actor* FindChild(const std::string& name, bool recursive = true) const;
		int GetSiblingIndex() const;

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
		void RemoveComponent(Component* component);
		void MoveComponent(Component* component, int delta);
		// 並びの index 番目へ動かす（範囲の外なら端へ）。動いたら true
		bool SetComponentIndex(Component* component, int index);
		int GetComponentIndex(const Component* component) const;

		// 子も含めて破棄する
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
