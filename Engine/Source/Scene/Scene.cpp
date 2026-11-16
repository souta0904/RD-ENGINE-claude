#include "RdPch.h"
#include "Scene/Scene.h"
#include "Core/Profiling/ProfileScope.h"
#include "Core/Time/Time.h"
#include "Render/Debug/DebugDraw.h"

namespace rd
{
	namespace
	{
		constexpr int kMaxFixedStepsPerFrame = 5;

		// 呼んでいる間に追加されたものは次のフレームから（呼ぶ前の数だけ回す）
		template <class Func>
		void CallEach(const std::vector<Component*>& list, Func&& func)
		{
			for (size_t i = 0, n = list.size(); i < n; ++i)
			{
				Component* c = list[i];
				if (c->HasStarted() && c->IsActiveAndEnabled()) { func(c); }
			}
		}

		void EraseComponents(std::vector<Component*>& list, const std::unordered_set<Component*>& removed)
		{
			list.erase(std::remove_if(list.begin(), list.end(), [&](Component* c) { return removed.contains(c); }), list.end());
		}
	}

	Scene::Scene() = default;

	Scene::~Scene()
	{
		Clear();
	}

	// 子から順に OnDestroy を呼ぶ
	void Scene::Clear()
	{
		for (auto it = mActors.rbegin(); it != mActors.rend(); ++it)
		{
			for (auto& c : (*it)->mComponents) { c->OnDestroy(); }
		}
		mBuckets.clear();
		mBucketIndex.clear();
		mCallbacks = {};
		mPendingStart.clear();
		mRootActors.clear();
		mIdMap.clear();
		mActors.clear();
	}

	Actor* Scene::CreateActor(const std::string& name, Actor* parent)
	{
		auto actor = std::make_unique<Actor>(this, mNextId++);
		Actor* raw = actor.get();
		raw->SetName(name);
		mIdMap.emplace(raw->GetId(), raw);
		mActors.push_back(std::move(actor));
		(parent ? parent->mChildren : mRootActors).push_back(raw);
		raw->mParent = parent;
		return raw;
	}

	Actor* Scene::FindActor(ActorId id) const
	{
		auto it = mIdMap.find(id);
		return it != mIdMap.end() && !it->second->IsPendingDestroy() ? it->second : nullptr;
	}

	Actor* Scene::FindActorByName(const std::string& name) const
	{
		for (auto& a : mActors)
		{
			if (a->GetName() == name && !a->IsPendingDestroy()) { return a.get(); }
		}
		return nullptr;
	}

	std::vector<Actor*> Scene::FindActorsWithTag(const std::string& tag) const
	{
		std::vector<Actor*> result;
		for (auto& a : mActors)
		{
			if (a->GetTag() == tag && !a->IsPendingDestroy()) { result.push_back(a.get()); }
		}
		return result;
	}

	// プレイ中: Start → Update → 固定時間の FixedUpdate（何回か）→ LateUpdate。システムはコンポーネントの後
	void Scene::Update(float dt, bool playing)
	{
		mPlaying = playing;

		if (playing)
		{
			{
				RD_PROFILE_SCOPE("Start");
				StartPendingComponents();
			}
			{
				RD_PROFILE_SCOPE("Update");
				CallEach(mCallbacks.update, [dt](Component* c) { c->Update(dt); });
				for (size_t i = 0; i < mSystems.size(); ++i) { mSystems[i]->Update(*this, dt); }
			}

			float fixed = Time::FixedDeltaTime();
			mFixedAccumulator = std::min(mFixedAccumulator + dt, fixed * kMaxFixedStepsPerFrame);
			while (mFixedAccumulator >= fixed)
			{
				RD_PROFILE_SCOPE("FixedUpdate");
				CallEach(mCallbacks.fixedUpdate, [fixed](Component* c) { c->FixedUpdate(fixed); });
				for (size_t i = 0; i < mSystems.size(); ++i) { mSystems[i]->FixedUpdate(*this, fixed); }
				mFixedAccumulator -= fixed;
			}

			RD_PROFILE_SCOPE("LateUpdate");
			CallEach(mCallbacks.lateUpdate, [dt](Component* c) { c->LateUpdate(dt); });
			for (size_t i = 0; i < mSystems.size(); ++i) { mSystems[i]->LateUpdate(*this, dt); }
		}
		else
		{
			auto& list = mCallbacks.editorUpdate;
			for (size_t i = 0, n = list.size(); i < n; ++i)
			{
				if (list[i]->IsActiveAndEnabled()) { list[i]->OnEditorUpdate(dt); }
			}
		}

		if (mHasPendingDestroy) { FlushDestroyed(); }
	}

	// Start の中で追加されたものは次のフレームに回す。無効なものは有効になるまで待たせる
	void Scene::StartPendingComponents()
	{
		if (mPendingStart.empty()) { return; }
		std::vector<Component*> pending;
		pending.swap(mPendingStart);
		for (Component* c : pending)
		{
			if (c->mPendingDestroy || c->mStarted) { continue; }
			if (c->IsActiveAndEnabled())
			{
				c->mStarted = true;
				c->Start();
			}
			else
			{
				mPendingStart.push_back(c);
			}
		}
	}

	void Scene::RegisterComponent(Component* component)
	{
		const ComponentTypeInfo& info = component->GetTypeInfo();
		auto [it, inserted] = mBucketIndex.try_emplace(info.type, mBuckets.size());
		if (inserted) { mBuckets.push_back({ info.type, {} }); }
		mBuckets[it->second].components.push_back(component);

		if (info.callbacks & kCallbackUpdate) { mCallbacks.update.push_back(component); }
		if (info.callbacks & kCallbackFixedUpdate) { mCallbacks.fixedUpdate.push_back(component); }
		if (info.callbacks & kCallbackLateUpdate) { mCallbacks.lateUpdate.push_back(component); }
		if (info.callbacks & kCallbackRender) { mCallbacks.render.push_back(component); }
		if (info.callbacks & kCallbackEditorUpdate) { mCallbacks.editorUpdate.push_back(component); }
		if (info.callbacks & kCallbackGizmos) { mCallbacks.gizmos.push_back(component); }
		mPendingStart.push_back(component);
	}

	void Scene::UnregisterComponents(const std::vector<Component*>& components)
	{
		if (components.empty()) { return; }
		std::unordered_set<Component*> removed(components.begin(), components.end());
		for (auto& bucket : mBuckets) { EraseComponents(bucket.components, removed); }
		EraseComponents(mCallbacks.update, removed);
		EraseComponents(mCallbacks.fixedUpdate, removed);
		EraseComponents(mCallbacks.lateUpdate, removed);
		EraseComponents(mCallbacks.render, removed);
		EraseComponents(mCallbacks.editorUpdate, removed);
		EraseComponents(mCallbacks.gizmos, removed);
		EraseComponents(mPendingStart, removed);
	}

	// 消すコンポーネントを先に呼び出しの一覧から外してから、コンポーネント単位 → アクター単位の順に消す
	void Scene::FlushDestroyed()
	{
		mHasPendingDestroy = false;
		std::vector<Component*> destroyed;
		for (auto& a : mActors)
		{
			if (a->mPendingDestroy) { continue; }
			for (auto& c : a->mComponents)
			{
				if (c->mPendingDestroy) { destroyed.push_back(c.get()); }
			}
		}
		for (auto& a : mActors)
		{
			if (!a->mPendingDestroy) { continue; }
			for (auto& c : a->mComponents) { destroyed.push_back(c.get()); }
		}
		UnregisterComponents(destroyed);

		for (auto& a : mActors)
		{
			if (!a->mPendingDestroy) { a->FlushDestroyedComponents(); }
		}
		for (auto& a : mActors)
		{
			if (!a->mPendingDestroy) { continue; }
			for (auto& c : a->mComponents)
			{
				c->mPendingDestroy = true;
				c->OnDestroy();
			}
			if (a->mParent && !a->mParent->mPendingDestroy)
			{
				auto& list = a->mParent->mChildren;
				list.erase(std::remove(list.begin(), list.end(), a.get()), list.end());
			}
			else if (!a->mParent)
			{
				mRootActors.erase(std::remove(mRootActors.begin(), mRootActors.end(), a.get()), mRootActors.end());
			}
			mIdMap.erase(a->GetId());
		}
		mActors.erase(std::remove_if(mActors.begin(), mActors.end(), [](auto& a) { return a->mPendingDestroy; }), mActors.end());
	}

	void Scene::CollectRender(RenderList& list)
	{
		RD_PROFILE_FUNCTION();
		for (Component* c : mCallbacks.render)
		{
			if (c->IsActiveAndEnabled()) { c->OnRender(list); }
		}
	}

#if RD_EDITOR
	// ギズモは無効なコンポーネントでも出す（アクターが非アクティブなら出さない）
	void Scene::DrawGizmos(DebugDraw& draw, const Actor* selected)
	{
		for (Component* c : mCallbacks.gizmos)
		{
			if (!c->mPendingDestroy && c->GetActor()->IsActive()) { c->OnDrawGizmos(draw, c->GetActor() == selected); }
		}
	}
#endif
}
