#include "RdPch.h"
#include "Scene/Serialization/SceneSerializer.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Scene/Component/ComponentRegistry.h"
#include "Scene/Component/MissingComponent.h"
#include "Scene/Scene.h"
#include "Scene/Serialization/RefFixupVisitor.h"

namespace rd
{
	namespace
	{
		constexpr int kSceneVersion = 1;

		// 型が登録されていなければ、値を持ったままの MissingComponent を置く（保存し直しても値が消えない）
		std::unique_ptr<Component> CreateComponent(const Json& cj)
		{
			std::string type = cj.value("Type", std::string());
			if (!type.empty() && !ComponentRegistry::Has(type))
			{
				Logger::Warning(LogCat::Scene, "Missing component type: {} (its values are kept)", type);
				auto missing = std::make_unique<MissingComponent>(type, cj);
				missing->SetEnabled(cj.value("Enabled", true));
				return missing;
			}
			auto comp = ComponentRegistry::Create(type);
			if (!comp) { return nullptr; }
			comp->SetEnabled(cj.value("Enabled", true));
			JsonReader r(cj);
			comp->Reflect(r);
			return comp;
		}
	}

	// 読み込んだコンポーネントの Awake を呼ぶ。Awake の中で同じアクターにコンポーネントを足すことがある
	//  （足したものは AddComponent の中で Awake される）ので、一覧を回しながらではなく、先に写した一覧で呼ぶ。
	//  一覧を回しながら足すと、一覧の入れ物が作り直されて、回している途中の場所が壊れる
	void SceneSerializer::AwakeComponents(Actor* actor)
	{
		std::vector<Component*> loaded;
		loaded.reserve(actor->mComponents.size());
		for (auto& c : actor->mComponents) { loaded.push_back(c.get()); }
		for (Component* c : loaded) { c->Awake(); }
	}

	Json SceneSerializer::SerializeActorState(const Actor* actor)
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
			if (auto* missing = dynamic_cast<const MissingComponent*>(c.get()))
			{
				Json cj = missing->GetData();
				cj["Enabled"] = missing->IsEnabled();
				comps.push_back(std::move(cj));
				continue;
			}
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

	void SceneSerializer::ApplyActorState(Actor* actor, const Json& state)
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
		if (!ApplyComponentValues(actor, comps)) { RebuildComponents(actor, comps); }
	}

	// コンポーネントの並びが同じなら、その場で値だけ書き戻す（Awake / OnDestroy を呼ばない）
	bool SceneSerializer::ApplyComponentValues(Actor* actor, const Json& comps)
	{
		if (comps.size() != actor->mComponents.size()) { return false; }
		for (size_t i = 0; i < comps.size(); ++i)
		{
			if (comps[i].value("Type", std::string()) != actor->mComponents[i]->GetTypeName() || actor->mComponents[i]->mPendingDestroy) { return false; }
		}
		for (size_t i = 0; i < comps.size(); ++i)
		{
			Component* c = actor->mComponents[i].get();
			c->SetEnabled(comps[i].value("Enabled", true));
			if (auto* missing = dynamic_cast<MissingComponent*>(c)) { missing->SetData(comps[i]); continue; }
			JsonReader r(comps[i]);
			c->Reflect(r);
			c->OnValidate();
		}
		return true;
	}

	// 追加・削除・並び替えがあったときは作り直す（すべてそろってから Awake）
	void SceneSerializer::RebuildComponents(Actor* actor, const Json& comps)
	{
		Scene* scene = actor->GetScene();
		std::vector<Component*> old;
		for (auto& c : actor->mComponents) { old.push_back(c.get()); }
		scene->UnregisterComponents(old);
		for (auto& c : actor->mComponents)
		{
			c->mPendingDestroy = true;
			c->OnDestroy();
		}
		actor->mComponents.clear();

		std::unordered_map<ActorId, Actor*> noRemap;
		RefFixupVisitor fixup(scene, noRemap, false, true);
		for (const auto& cj : comps)
		{
			auto comp = CreateComponent(cj);
			if (!comp) { continue; }
			comp->Reflect(fixup);
			actor->AddComponent(std::move(comp), false);
		}
		AwakeComponents(actor);
	}

	Json SceneSerializer::SerializeSettings(const Scene& scene)
	{
		Json j = Json::object();
		Json env = Json::object();
		JsonWriter w(env);
		const_cast<Environment&>(scene.mEnvironment).Reflect(w);
		j["Environment"] = std::move(env);
		j["Gravity"] = scene.mGravity;
		return j;
	}

	void SceneSerializer::ApplySettings(Scene& scene, const Json& json)
	{
		if (auto env = json.find("Environment"); env != json.end())
		{
			JsonReader r(*env);
			scene.mEnvironment.Reflect(r);
		}
		if (auto g = json.find("Gravity"); g != json.end() && g->is_array()) { scene.mGravity = g->get<Vector3>(); }
	}

	void SceneSerializer::SerializeActor(const Actor* actor, Json& out)
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

	Json SceneSerializer::SerializeActors(const std::vector<Actor*>& actors)
	{
		Json out = Json::array();
		for (const Actor* a : actors)
		{
			if (!a->IsPendingDestroy()) { SerializeActor(a, out); }
		}
		return out;
	}

	// 1. アクターを作る（ID を保てるなら保つ） 2. 親子とプロパティ 3. 参照をこのシーンに結び付ける 4. すべてそろってから Awake
	std::vector<Actor*> SceneSerializer::DeserializeActors(Scene& scene, const Json& actors, bool remapIds, Actor* parent, bool keepExternalRefs)
	{
		std::vector<Actor*> roots;
		std::vector<std::pair<Actor*, const Json*>> created;
		std::unordered_map<ActorId, Actor*> idMap;
		if (!actors.is_array()) { return roots; }

		for (const auto& j : actors)
		{
			ActorId oldId = j.value("Id", ActorId(0));
			Actor* actor = nullptr;
			if (!remapIds && oldId != 0 && !scene.mIdMap.contains(oldId))
			{
				auto a = std::make_unique<Actor>(&scene, oldId);
				actor = a.get();
				scene.mIdMap.emplace(oldId, actor);
				scene.mActors.push_back(std::move(a));
				scene.mRootActors.push_back(actor);
				scene.mNextId = std::max(scene.mNextId, oldId + 1);
			}
			else
			{
				actor = scene.CreateActor();
			}
			actor->SetName(j.value("Name", std::string("Actor")));
			actor->SetActive(j.value("Active", true));
			actor->SetTag(j.value("Tag", std::string()));
			actor->SetLayer(j.value("Layer", 0u));
			if (oldId != 0) { idMap[oldId] = actor; }
			created.emplace_back(actor, &j);
		}

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
					if (auto comp = CreateComponent(cj)) { actor->AddComponent(std::move(comp), false); }
				}
			}
		}

		RefFixupVisitor fixup(&scene, idMap, remapIds, keepExternalRefs);
		for (auto& [actor, jp] : created)
		{
			for (auto& c : actor->mComponents) { c->Reflect(fixup); }
		}

		for (auto& [actor, jp] : created) { AwakeComponents(actor); }
		return roots;
	}

	Json SceneSerializer::Serialize(const Scene& scene)
	{
		Json j = Json::object();
		j["Version"] = kSceneVersion;
		j["Name"] = scene.mName;
		j.update(SerializeSettings(scene));
		j["Actors"] = SerializeActors(scene.mRootActors);
		return j;
	}

	bool SceneSerializer::Deserialize(Scene& scene, const Json& json)
	{
		scene.Clear();
		scene.mNextId = 1;
		scene.mName = json.value("Name", std::string("Untitled"));
		ApplySettings(scene, json);
		if (auto actors = json.find("Actors"); actors != json.end())
		{
			DeserializeActors(scene, *actors, false);
		}
		return true;
	}

	bool SceneSerializer::SaveToFile(Scene& scene, const std::string& path)
	{
		if (!SaveJsonFile(path, Serialize(scene)))
		{
			Logger::Error(LogCat::Scene, "Failed to save scene: {}", path);
			return false;
		}
		scene.mPath = path;
		Logger::Info(LogCat::Scene, "Saved scene: {}", path);
		return true;
	}

	bool SceneSerializer::LoadFromFile(Scene& scene, const std::string& path)
	{
		Json json;
		if (!LoadJsonFile(path, json))
		{
			Logger::Error(LogCat::Scene, "Failed to load scene: {}", path);
			return false;
		}
		Deserialize(scene, json);
		scene.mPath = path;
		Logger::Info(LogCat::Scene, "Loaded scene: {} ({} actors)", path, scene.mActors.size());
		return true;
	}

	// プレハブの外を指していた参照は、このシーンでは意味がないので外す
	Actor* SceneSerializer::InstantiatePrefab(Scene& scene, const std::string& path, Actor* parent)
	{
		Json json;
		if (!LoadJsonFile(path, json))
		{
			Logger::Error(LogCat::Scene, "Prefab not found: {}", path);
			return nullptr;
		}
		auto roots = DeserializeActors(scene, json.value("Actors", Json::array()), true, parent, false);
		return roots.empty() ? nullptr : roots.front();
	}

	bool SceneSerializer::SavePrefab(const Actor* actor, const std::string& path)
	{
		Json json = Json::object();
		json["Version"] = kSceneVersion;
		Json actors = SerializeActors({ const_cast<Actor*>(actor) });
		if (!actors.empty()) { actors[0]["Parent"] = 0; }
		json["Actors"] = std::move(actors);
		return SaveJsonFile(path, json);
	}
}
