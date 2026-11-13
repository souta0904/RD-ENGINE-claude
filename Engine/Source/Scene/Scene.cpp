#include "RdPch.h"
#include "Scene/Scene.h"
#include "Core/Profiler.h"
#include "Core/StringUtil.h"
#include "Core/Time.h"
#include "Render/DebugDraw.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/ObjectRef.h"
#include <unordered_set>

namespace rd
{
	namespace
	{
		constexpr int kSceneVersion = 1;
		constexpr int kMaxFixedStepsPerFrame = 5;

		// 読み込んだ ObjectRef を、このシーンに結び付けて ID を付け替えるビジター（参照以外は何もしない）
		class RefFixupVisitor final : public PropertyVisitor
		{
		public:
			RefFixupVisitor(Scene* scene, const std::unordered_map<ActorId, Actor*>& idMap, bool remap, bool keepExternal)
				: mScene(scene), mIdMap(idMap), mRemap(remap), mKeepExternal(keepExternal) {}

			bool Property(const char*, ObjectRefBase& base, const PropertyMeta&) override
			{
				auto& value = static_cast<ObjectRef&>(base);// 参照プロパティはすべて ObjectRef の派生
				value.SetScene(mScene);
				if (value.GetId() == 0 || !mRemap) { return false; }
				if (auto it = mIdMap.find(value.GetId()); it != mIdMap.end())
				{
					// 一緒に複製されたアクターを指していたら、複製先へ付け替える
					value.SetId(it->second->GetId());
				}
				else if (!mKeepExternal)
				{
					value.Clear();
				}
				return false;
			}

			bool Property(const char*, bool&, const PropertyMeta&) override { return false; }
			bool Property(const char*, int32_t&, const PropertyMeta&) override { return false; }
			bool Property(const char*, uint32_t&, const PropertyMeta&) override { return false; }
			bool Property(const char*, float&, const PropertyMeta&) override { return false; }
			bool Property(const char*, Vector2&, const PropertyMeta&) override { return false; }
			bool Property(const char*, Vector3&, const PropertyMeta&) override { return false; }
			bool Property(const char*, Vector4&, const PropertyMeta&) override { return false; }
			bool Property(const char*, Quaternion&, const PropertyMeta&) override { return false; }
			bool Property(const char*, Color&, const PropertyMeta&) override { return false; }
			bool Property(const char*, std::string&, const PropertyMeta&) override { return false; }
			using PropertyVisitor::Property;

		protected:
			bool EnumProperty(const char*, int64_t&, std::span<const std::string_view>, std::span<const int64_t>, const PropertyMeta&) override { return false; }

		private:
			Scene* mScene;
			const std::unordered_map<ActorId, Actor*>& mIdMap;
			bool mRemap;
			bool mKeepExternal;
		};
	}

	Scene::Scene() = default;

	Scene::~Scene()
	{
		Clear();
	}

	void Scene::Clear()
	{
		// 子から順に OnDestroy を呼ぶ
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

	// ==================================================
	// アクター
	// ==================================================
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

	// ==================================================
	// 更新
	// ==================================================
	namespace
	{
		// 呼び出し中に追加されたものは次のフレームから（呼ぶ前の数だけ回す）
		template <class Func>
		void CallEach(const std::vector<Component*>& list, Func&& func)
		{
			for (size_t i = 0, n = list.size(); i < n; ++i)
			{
				Component* c = list[i];
				if (c->HasStarted() && c->IsActiveAndEnabled()) { func(c); }
			}
		}
	}

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

			// 固定時間ステップ（物理などのシステムはコンポーネントの後）
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

	void Scene::StartPendingComponents()
	{
		if (mPendingStart.empty()) { return; }
		// Start の中で追加されたものは次のフレームに回す
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
				mPendingStart.push_back(c);// 有効になるまで待つ
			}
		}
	}

	// ==================================================
	// コンポーネントの登録
	// ==================================================
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
		auto erase = [&](std::vector<Component*>& list)
			{
				list.erase(std::remove_if(list.begin(), list.end(), [&](Component* c) { return removed.contains(c); }), list.end());
			};
		for (auto& bucket : mBuckets) { erase(bucket.components); }
		erase(mCallbacks.update);
		erase(mCallbacks.fixedUpdate);
		erase(mCallbacks.lateUpdate);
		erase(mCallbacks.render);
		erase(mCallbacks.editorUpdate);
		erase(mCallbacks.gizmos);
		erase(mPendingStart);
	}

	void Scene::FlushDestroyed()
	{
		mHasPendingDestroy = false;
		std::vector<Component*> destroyed;
		// コンポーネント単位の破棄
		for (auto& a : mActors)
		{
			if (a->mPendingDestroy) { continue; }
			for (auto& c : a->mComponents)
			{
				if (c->mPendingDestroy) { destroyed.push_back(c.get()); }
			}
		}
		// アクター単位の破棄
		for (auto& a : mActors)
		{
			if (!a->mPendingDestroy) { continue; }
			for (auto& c : a->mComponents) { destroyed.push_back(c.get()); }
		}
		// 呼び出しリストから外してから破棄する
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
	void Scene::DrawGizmos(DebugDraw& draw, const Actor* selected)
	{
		// ギズモは無効なコンポーネントでも表示する（アクターが非アクティブなら出さない）
		for (Component* c : mCallbacks.gizmos)
		{
			if (!c->mPendingDestroy && c->GetActor()->IsActive()) { c->OnDrawGizmos(draw, c->GetActor() == selected); }
		}
	}
#endif

	// ==================================================
	// シリアライズ
	// ==================================================
	Json Scene::SerializeActorState(const Actor* actor) const
	{
		Json j = Json::object();
		j["Name"] = actor->GetName();
		j["Active"] = actor->IsActiveSelf();
		if (!actor->GetTag().empty()) { j["Tag"] = actor->GetTag(); }
		if (actor->GetLayer() != 0) { j["Layer"] = actor->GetLayer(); }

		Json transform = Json::object();
		JsonWriter tw(transform);
		const_cast<Transform&>(actor->GetTransform()).Reflect(tw);
		j["Transform"] = std::move(transform);

		Json comps = Json::array();
		for (auto& c : actor->GetAllComponents())
		{
			if (c->mPendingDestroy) { continue; }
			Json cj = Json::object();
			cj["Type"] = c->GetTypeName();
			cj["Enabled"] = c->IsEnabled();
			JsonWriter w(cj);
			c->Reflect(w);
			comps.push_back(std::move(cj));
		}
		j["Components"] = std::move(comps);
		return j;
	}

	void Scene::ApplyActorState(Actor* actor, const Json& state)
	{
		actor->SetName(state.value("Name", actor->GetName()));
		actor->SetActive(state.value("Active", true));
		actor->SetTag(state.value("Tag", std::string()));
		actor->SetLayer(state.value("Layer", 0u));
		if (auto t = state.find("Transform"); t != state.end())
		{
			JsonReader r(*t);
			actor->GetTransform().Reflect(r);
		}

		const Json& comps = state.contains("Components") ? state["Components"] : Json::array();
		// コンポーネントの並びが同じなら、その場で値だけ書き戻す（Awake / OnDestroy を呼ばない）
		bool sameLayout = comps.size() == actor->mComponents.size();
		for (size_t i = 0; sameLayout && i < comps.size(); ++i)
		{
			sameLayout = comps[i].value("Type", std::string()) == actor->mComponents[i]->GetTypeName() && !actor->mComponents[i]->mPendingDestroy;
		}
		if (sameLayout)
		{
			for (size_t i = 0; i < comps.size(); ++i)
			{
				Component* c = actor->mComponents[i].get();
				c->SetEnabled(comps[i].value("Enabled", true));
				JsonReader r(comps[i]);
				c->Reflect(r);
				c->OnValidate();
			}
			return;
		}

		// 追加・削除・並び替えがあったときは作り直す
		std::vector<Component*> old;
		for (auto& c : actor->mComponents) { old.push_back(c.get()); }
		UnregisterComponents(old);
		for (auto& c : actor->mComponents)
		{
			c->mPendingDestroy = true;
			c->OnDestroy();
		}
		actor->mComponents.clear();

		std::unordered_map<ActorId, Actor*> noRemap;
		RefFixupVisitor fixup(this, noRemap, false, true);
		for (const auto& cj : comps)
		{
			auto comp = ComponentRegistry::Create(cj.value("Type", std::string()));
			if (!comp) { continue; }
			comp->SetEnabled(cj.value("Enabled", true));
			JsonReader r(cj);
			comp->Reflect(r);
			comp->Reflect(fixup);
			actor->AddComponent(std::move(comp), false);
		}
		for (auto& c : actor->mComponents) { c->Awake(); }
	}

	Json Scene::SerializeSettings() const
	{
		Json j = Json::object();
		Json env = Json::object();
		JsonWriter w(env);
		const_cast<Environment&>(mEnvironment).Reflect(w);
		j["Environment"] = std::move(env);
		j["Gravity"] = mGravity;
		return j;
	}

	void Scene::ApplySettings(const Json& json)
	{
		if (auto env = json.find("Environment"); env != json.end())
		{
			JsonReader r(*env);
			mEnvironment.Reflect(r);
		}
		if (auto g = json.find("Gravity"); g != json.end() && g->is_array()) { mGravity = g->get<Vector3>(); }
	}

	void Scene::SerializeActor(const Actor* actor, Json& out) const
	{
		Json j = Json::object();
		j["Id"] = actor->GetId();
		j["Parent"] = actor->GetParent() ? actor->GetParent()->GetId() : 0;
		j.update(SerializeActorState(actor));
		out.push_back(std::move(j));

		for (const Actor* child : actor->GetChildren())
		{
			if (!child->IsPendingDestroy()) { SerializeActor(child, out); }
		}
	}

	Json Scene::SerializeActors(const std::vector<Actor*>& actors) const
	{
		Json out = Json::array();
		for (const Actor* a : actors)
		{
			if (!a->IsPendingDestroy()) { SerializeActor(a, out); }
		}
		return out;
	}

	std::vector<Actor*> Scene::DeserializeActors(const Json& actors, bool remapIds, Actor* parent, bool keepExternalRefs)
	{
		std::vector<Actor*> roots;
		std::vector<std::pair<Actor*, const Json*>> created;
		std::unordered_map<ActorId, Actor*> idMap;
		if (!actors.is_array()) { return roots; }

		// 1. アクターを作る
		for (const auto& j : actors)
		{
			ActorId oldId = j.value("Id", ActorId(0));
			Actor* actor = nullptr;
			if (!remapIds && oldId != 0 && !mIdMap.contains(oldId))
			{
				auto a = std::make_unique<Actor>(this, oldId);
				actor = a.get();
				mIdMap.emplace(oldId, actor);
				mActors.push_back(std::move(a));
				mRootActors.push_back(actor);
				mNextId = std::max(mNextId, oldId + 1);
			}
			else
			{
				actor = CreateActor();
			}
			actor->SetName(j.value("Name", std::string("Actor")));
			actor->SetActive(j.value("Active", true));
			actor->SetTag(j.value("Tag", std::string()));
			actor->SetLayer(j.value("Layer", 0u));
			if (oldId != 0) { idMap[oldId] = actor; }
			created.emplace_back(actor, &j);
		}

		// 2. 親子関係とプロパティ
		for (auto& [actor, jp] : created)
		{
			const Json& j = *jp;
			ActorId parentId = j.value("Parent", ActorId(0));
			auto it = idMap.find(parentId);
			Actor* newParent = it != idMap.end() ? it->second : parent;
			if (newParent) { actor->SetParent(newParent, false); }
			if (it == idMap.end()) { roots.push_back(actor); }

			if (auto t = j.find("Transform"); t != j.end())
			{
				JsonReader r(*t);
				actor->GetTransform().Reflect(r);
			}
			if (auto comps = j.find("Components"); comps != j.end() && comps->is_array())
			{
				for (const auto& cj : *comps)
				{
					auto comp = ComponentRegistry::Create(cj.value("Type", std::string()));
					if (!comp) { continue; }
					comp->SetEnabled(cj.value("Enabled", true));
					JsonReader r(cj);
					comp->Reflect(r);
					actor->AddComponent(std::move(comp), false);
				}
			}
		}

		// 3. 参照をこのシーンに結び付ける（複製なら、一緒に複製したアクターへの参照を付け替える）
		RefFixupVisitor fixup(this, idMap, remapIds, keepExternalRefs);
		for (auto& [actor, jp] : created)
		{
			for (auto& c : actor->mComponents) { c->Reflect(fixup); }
		}

		// 4. すべてそろってから Awake
		for (auto& [actor, jp] : created)
		{
			for (auto& c : actor->mComponents) { c->Awake(); }
		}
		return roots;
	}

	Json Scene::Serialize() const
	{
		Json j = Json::object();
		j["Version"] = kSceneVersion;
		j["Name"] = mName;
		j.update(SerializeSettings());
		j["Actors"] = SerializeActors(mRootActors);
		return j;
	}

	bool Scene::Deserialize(const Json& json)
	{
		Clear();
		mNextId = 1;
		mName = json.value("Name", std::string("Untitled"));
		ApplySettings(json);
		if (auto actors = json.find("Actors"); actors != json.end())
		{
			DeserializeActors(*actors, false);
		}
		return true;
	}

	bool Scene::SaveToFile(const std::string& path)
	{
		if (!SaveJsonFile(path, Serialize()))
		{
			Log::Error(LogCat::Scene, "Failed to save scene: {}", path);
			return false;
		}
		mPath = path;
		Log::Info(LogCat::Scene, "Saved scene: {}", path);
		return true;
	}

	bool Scene::LoadFromFile(const std::string& path)
	{
		Json json;
		if (!LoadJsonFile(path, json))
		{
			Log::Error(LogCat::Scene, "Failed to load scene: {}", path);
			return false;
		}
		Deserialize(json);
		mPath = path;
		Log::Info(LogCat::Scene, "Loaded scene: {} ({} actors)", path, mActors.size());
		return true;
	}

	Actor* Scene::Instantiate(const std::string& prefabPath, Actor* parent)
	{
		Json json;
		if (!LoadJsonFile(prefabPath, json))
		{
			Log::Error(LogCat::Scene, "Prefab not found: {}", prefabPath);
			return nullptr;
		}
		// プレハブの外を指していた参照は、このシーンでは意味がないので外す
		auto roots = DeserializeActors(json.value("Actors", Json::array()), true, parent, false);
		return roots.empty() ? nullptr : roots.front();
	}

	Actor* Scene::Duplicate(Actor* actor)
	{
		if (!actor) { return nullptr; }
		Json json = SerializeActors({ actor });
		auto roots = DeserializeActors(json, true, actor->GetParent());
		if (roots.empty()) { return nullptr; }
		roots.front()->SetName(actor->GetName());
		return roots.front();
	}

	bool Scene::SavePrefab(const Actor* actor, const std::string& path)
	{
		Json json = Json::object();
		json["Version"] = kSceneVersion;
		Json actors = actor->GetScene()->SerializeActors({ const_cast<Actor*>(actor) });
		// ルートは親なしとして保存する
		if (!actors.empty()) { actors[0]["Parent"] = 0; }
		json["Actors"] = std::move(actors);
		return SaveJsonFile(path, json);
	}
}
