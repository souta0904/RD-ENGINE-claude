#include "RdPch.h"
#include "Scene/Scene.h"
#include "Components/CameraComponent.h"
#include "Core/StringUtil.h"
#include "Core/Time.h"
#include "Physics/PhysicsWorld.h"
#include "Render/DebugDraw.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/ObjectRef.h"

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

			bool Property(const char*, ObjectRef& value, const PropertyMeta&) override
			{
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

	Scene::Scene()
		: mPhysics(std::make_unique<PhysicsWorld>())
	{
	}

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
	void Scene::Update(float dt, bool playing)
	{
		mPlaying = playing;

		// 途中で増減しても安全なように対象を確定させておく
		std::vector<Component*> comps;
		comps.reserve(mActors.size() * 3);
		for (auto& a : mActors)
		{
			if (!a->IsActive()) { continue; }
			for (auto& c : a->mComponents)
			{
				if (c->mEnabled && !c->mPendingDestroy) { comps.push_back(c.get()); }
			}
		}

		if (playing)
		{
			for (Component* c : comps)
			{
				if (!c->mStarted && !c->mPendingDestroy)
				{
					c->mStarted = true;
					c->Start();
				}
			}
			for (Component* c : comps) { if (!c->mPendingDestroy) { c->Update(dt); } }

			// 固定時間ステップで物理を進める
			float fixed = Time::FixedDeltaTime();
			mFixedAccumulator = std::min(mFixedAccumulator + dt, fixed * kMaxFixedStepsPerFrame);
			while (mFixedAccumulator >= fixed)
			{
				for (Component* c : comps) { if (!c->mPendingDestroy) { c->FixedUpdate(fixed); } }
				mPhysics->Step(*this, fixed);
				mFixedAccumulator -= fixed;
			}

			for (Component* c : comps) { if (!c->mPendingDestroy) { c->LateUpdate(dt); } }
		}
		else
		{
			for (Component* c : comps) { c->OnEditorUpdate(dt); }
		}

		if (mHasPendingDestroy) { FlushDestroyed(); }
	}

	void Scene::FlushDestroyed()
	{
		mHasPendingDestroy = false;
		// コンポーネント単位の破棄
		for (auto& a : mActors)
		{
			if (!a->mPendingDestroy) { a->FlushDestroyedComponents(); }
		}
		// アクター単位の破棄
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
		for (auto& a : mActors)
		{
			if (!a->IsActive()) { continue; }
			for (auto& c : a->mComponents)
			{
				if (c->mEnabled && !c->mPendingDestroy) { c->OnRender(list); }
			}
		}
	}

#if RD_EDITOR
	void Scene::DrawGizmos(DebugDraw& draw, const Actor* selected)
	{
		for (auto& a : mActors)
		{
			if (!a->IsActive()) { continue; }
			bool isSelected = a.get() == selected;
			for (auto& c : a->mComponents)
			{
				if (!c->mPendingDestroy) { c->OnDrawGizmos(draw, isSelected); }
			}
		}
	}
#endif

	CameraComponent* Scene::GetMainCamera() const
	{
		CameraComponent* best = nullptr;
		for (auto& a : mActors)
		{
			if (!a->IsActive()) { continue; }
			for (auto* cam : a->GetComponents<CameraComponent>())
			{
				if (cam->IsEnabled() && (!best || cam->GetPriority() > best->GetPriority())) { best = cam; }
			}
		}
		return best;
	}

	// ==================================================
	// シリアライズ
	// ==================================================
	void Scene::SerializeActor(const Actor* actor, Json& out) const
	{
		Json j = Json::object();
		j["Id"] = actor->GetId();
		j["Name"] = actor->GetName();
		j["Parent"] = actor->GetParent() ? actor->GetParent()->GetId() : 0;
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
		Json env = Json::object();
		JsonWriter w(env);
		const_cast<Environment&>(mEnvironment).Reflect(w);
		j["Environment"] = std::move(env);
		j["Actors"] = SerializeActors(mRootActors);
		return j;
	}

	bool Scene::Deserialize(const Json& json)
	{
		Clear();
		mNextId = 1;
		mName = json.value("Name", std::string("Untitled"));
		if (auto env = json.find("Environment"); env != json.end())
		{
			JsonReader r(*env);
			mEnvironment.Reflect(r);
		}
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
			Log::Error("Failed to save scene: {}", path);
			return false;
		}
		mPath = path;
		Log::Info("Saved scene: {}", path);
		return true;
	}

	bool Scene::LoadFromFile(const std::string& path)
	{
		Json json;
		if (!LoadJsonFile(path, json))
		{
			Log::Error("Failed to load scene: {}", path);
			return false;
		}
		Deserialize(json);
		mPath = path;
		Log::Info("Loaded scene: {} ({} actors)", path, mActors.size());
		return true;
	}

	Actor* Scene::Instantiate(const std::string& prefabPath, Actor* parent)
	{
		Json json;
		if (!LoadJsonFile(prefabPath, json))
		{
			Log::Error("Prefab not found: {}", prefabPath);
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
