#include "RdPch.h"
#include "Editor/AssistantTools.h"
#include "Asset/AudioClip.h"
#include "Core/StringUtil.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"

// AI アシスタントが使うツール（シーンの読み書き）
//  変更するツールは、エディタの通常の操作と同じコマンドを積むので Ctrl+Z で戻せる
namespace rd
{
	namespace
	{
		// ---------------- JSON スキーマの部品 ----------------
		Json Vec3Schema(const char* description)
		{
			return { { "type", "array" }, { "items", { { "type", "number" } } }, { "minItems", 3 }, { "maxItems", 3 }, { "description", description } };
		}
		Json ComponentListSchema()
		{
			return { { "type", "array" }, { "items", {
				{ "type", "object" },
				{ "properties", {
					{ "type", { { "type", "string" }, { "description", "Component type name, e.g. MeshRenderer, BoxCollider, Rigidbody, PointLight" } } },
					{ "properties", { { "type", "object" }, { "description", "Property values to set (same keys as get_actor / list_component_types). Omitted keys keep their defaults." } } },
				} },
				{ "required", Json::array({ "type" }) },
			} } };
		}
		Json Tool(const char* name, const char* description, Json properties, Json required = Json::array())
		{
			return { { "name", name }, { "description", description },
				{ "input_schema", { { "type", "object" }, { "properties", std::move(properties) }, { "required", std::move(required) } } } };
		}

		// ---------------- 変換 ----------------
		Vector3 ReadVec3(const Json& j)
		{
			if (!j.is_array() || j.size() != 3) { throw std::runtime_error("expected an array of 3 numbers"); }
			return { j[0].get<float>(), j[1].get<float>(), j[2].get<float>() };
		}
		Quaternion EulerDegrees(const Vector3& deg)
		{
			return Quaternion::FromEuler({ math::ToRadians(deg.x), math::ToRadians(deg.y), math::ToRadians(deg.z) });
		}
		Json ToEulerDegrees(const Quaternion& q)
		{
			Vector3 e = q.ToEuler();
			auto round = [](float v) { return std::round(math::ToDegrees(v) * 100.0f) / 100.0f; };
			return Json::array({ round(e.x), round(e.y), round(e.z) });
		}

		// アクターの状態（SerializeActorState）の Transform を AI 向けに読みやすくする
		Json DescribeActor(const Scene& scene, const Actor* a)
		{
			Json j = scene.SerializeActorState(a);
			j["Id"] = a->GetId();
			j["Parent"] = a->GetParent() ? a->GetParent()->GetId() : 0;
			Json children = Json::array();
			for (Actor* c : a->GetChildren()) { children.push_back(c->GetId()); }
			j["Children"] = children;
			// 回転はクォータニオンではなくオイラー角（度）で見せる
			if (auto t = j.find("Transform"); t != j.end())
			{
				(*t)["Rotation"] = ToEulerDegrees(a->GetTransform().GetLocalRotation());
				(*t)["WorldPosition"] = a->GetTransform().GetPosition();
			}
			return j;
		}

		// AI が送ってきたコンポーネント指定を、SerializeActorState の Components の要素にする
		Json MakeComponentJson(const Json& spec)
		{
			std::string type = spec.value("type", "");
			auto component = ComponentRegistry::Create(type);
			if (!component) { throw std::runtime_error("unknown component type: " + type + " (see list_component_types)"); }
			Json j = Json::object();
			j["Type"] = type;
			j["Enabled"] = true;
			JsonWriter writer(j);
			component->Reflect(writer);// 既定値
			if (auto props = spec.find("properties"); props != spec.end() && props->is_object()) { j.merge_patch(*props); }
			return j;
		}

		void ApplyTransform(Json& state, const Json& input)
		{
			Json& t = state["Transform"];
			if (input.contains("position")) { t["Position"] = ReadVec3(input["position"]); }
			if (input.contains("rotation")) { t["Rotation"] = EulerDegrees(ReadVec3(input["rotation"])); }
			if (input.contains("scale")) { t["Scale"] = ReadVec3(input["scale"]); }
		}

		Json ListAssets(const std::string& kind)
		{
			Json out = Json::array();
			std::error_code ec;
			for (auto& e : std::filesystem::recursive_directory_iterator("Assets", ec))
			{
				if (!e.is_regular_file()) { continue; }
				std::string ext = ToLower(e.path().extension().string());
				std::string type;
				if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") { type = "model"; }
				else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".dds" || ext == ".bmp") { type = "texture"; }
				else if (AudioClip::IsSupportedExtension(ext)) { type = "audio"; }
				else if (ext == ".prefab") { type = "prefab"; }
				else if (ext == ".scene") { type = "scene"; }
				else { continue; }
				if (kind != "all" && kind != type) { continue; }
				std::string path = ToUtf8(e.path().generic_wstring());
				out.push_back({ { "path", path }, { "type", type } });
				if (out.size() >= 300) { break; }
			}
			return out;
		}
	}

	const Json& AssistantTools::GetDefinitions()
	{
		static const Json tools = Json::array({
			Tool("get_scene", "List every actor in the open scene (id, name, parent id, local position, component types), in hierarchy order.", Json::object()),
			Tool("get_actor", "Get the full state of one actor: name, tag, transform (rotation in Euler degrees), and every component with all of its properties.",
				{ { "actor_id", { { "type", "integer" } } } }, Json::array({ "actor_id" })),
			Tool("list_component_types", "List every component type that can be added, with its category and default property values.", Json::object()),
			Tool("list_assets", "List asset files under Assets/ (models, textures, audio, prefabs, scenes).",
				{ { "kind", { { "type", "string" }, { "enum", Json::array({ "all", "model", "texture", "audio", "prefab", "scene" }) } } } }),
			Tool("create_actor", "Create a new actor. For a visible object add a MeshRenderer with properties {\"Model\": \"Engine/Cube\"} (or another model) and usually a collider.",
				{
					{ "name", { { "type", "string" } } },
					{ "parent_id", { { "type", "integer" }, { "description", "Parent actor id. Omit or 0 for a root actor." } } },
					{ "position", Vec3Schema("Local position in meters") },
					{ "rotation", Vec3Schema("Local rotation, Euler degrees (X, Y, Z)") },
					{ "scale", Vec3Schema("Local scale") },
					{ "tag", { { "type", "string" } } },
					{ "components", ComponentListSchema() },
				}, Json::array({ "name" })),
			Tool("modify_actor", "Change an existing actor. Only the fields you pass are changed.",
				{
					{ "actor_id", { { "type", "integer" } } },
					{ "name", { { "type", "string" } } },
					{ "active", { { "type", "boolean" } } },
					{ "tag", { { "type", "string" } } },
					{ "parent_id", { { "type", "integer" }, { "description", "New parent id (0 = make it a root actor). World position is kept." } } },
					{ "position", Vec3Schema("Local position in meters") },
					{ "rotation", Vec3Schema("Local rotation, Euler degrees (X, Y, Z)") },
					{ "scale", Vec3Schema("Local scale") },
					{ "set_properties", { { "type", "array" }, { "description", "Change properties of existing components (the first component of that type).",
						}, { "items", { { "type", "object" }, { "properties", {
							{ "type", { { "type", "string" } } },
							{ "properties", { { "type", "object" } } },
						} }, { "required", Json::array({ "type", "properties" }) } } } } },
					{ "add_components", ComponentListSchema() },
					{ "remove_components", { { "type", "array" }, { "items", { { "type", "string" } } }, { "description", "Component type names to remove" } } },
				}, Json::array({ "actor_id" })),
			Tool("delete_actor", "Delete an actor and all of its children.", { { "actor_id", { { "type", "integer" } } } }, Json::array({ "actor_id" })),
			Tool("select_actor", "Select an actor in the editor so the user can see it in the inspector.", { { "actor_id", { { "type", "integer" } } } }, Json::array({ "actor_id" })),
		});
		return tools;
	}

	static std::string RunUnchecked(AssistantToolHost& host, const std::string& name, const Json& input, bool& isError)
	{
		Scene& scene = host.GetScene();
		auto findActor = [&](const char* key) -> Actor*
			{
				ActorId id = input.value(key, ActorId(0));
				Actor* a = scene.FindActor(id);
				if (!a) { throw std::runtime_error(std::format("actor {} not found", id)); }
				return a;
			};
		const bool mutating = name == "create_actor" || name == "modify_actor" || name == "delete_actor";
		if (mutating && host.IsPlaying())
		{
			isError = true;
			return "The editor is in play mode. Changes would be lost when play stops; ask the user to stop play mode first.";
		}

		if (name == "get_scene")
		{
			Json actors = Json::array();
			scene.ForEachActor([&](Actor* a)
				{
					if (actors.size() >= 500) { return; }
					Json comps = Json::array();
					for (auto& c : a->GetAllComponents()) { comps.push_back(c->GetTypeName()); }
					actors.push_back({ { "id", a->GetId() }, { "name", a->GetName() }, { "parent", a->GetParent() ? a->GetParent()->GetId() : 0 },
						{ "position", a->GetTransform().GetLocalPosition() }, { "components", comps } });
				});
			return Json{ { "scene", scene.GetPath().empty() ? "Untitled" : scene.GetPath() }, { "actors", actors } }.dump();
		}
		if (name == "get_actor")
		{
			return DescribeActor(scene, findActor("actor_id")).dump();
		}
		if (name == "list_component_types")
		{
			Json out = Json::array();
			for (const auto& entry : ComponentRegistry::GetEntries())
			{
				auto c = entry.create();
				Json defaults = Json::object();
				JsonWriter writer(defaults);
				c->Reflect(writer);
				out.push_back({ { "type", entry.name }, { "category", entry.category }, { "defaults", defaults } });
			}
			return out.dump();
		}
		if (name == "list_assets")
		{
			return ListAssets(input.value("kind", "all")).dump();
		}
		if (name == "select_actor")
		{
			host.Select(findActor("actor_id"));
			return "ok";
		}

		if (name == "create_actor")
		{
			Actor* parent = nullptr;
			if (ActorId pid = input.value("parent_id", ActorId(0)); pid != 0)
			{
				parent = scene.FindActor(pid);
				if (!parent) { throw std::runtime_error(std::format("parent {} not found", pid)); }
			}
			// 先に中身を作って検証してから、アクターを作る（途中で失敗してゴミが残らないように）
			Json components = Json::array();
			for (const auto& spec : input.value("components", Json::array())) { components.push_back(MakeComponentJson(spec)); }

			Actor* actor = scene.CreateActor(input.value("name", std::string("Actor")), parent);
			Json state = scene.SerializeActorState(actor);
			ApplyTransform(state, input);
			if (input.contains("tag")) { state["Tag"] = input.value("tag", ""); }
			state["Components"] = components;
			scene.ApplyActorState(actor, state);
			host.PushCommand(ActorLifetimeCommand::Created("AI: Create " + actor->GetName(), scene, { actor }));
			return Json{ { "created_id", actor->GetId() } }.dump();
		}

		if (name == "modify_actor")
		{
			Actor* actor = findActor("actor_id");
			// 親の付け替えは別のコマンド（兄弟の順番も戻せるように）
			if (input.contains("parent_id"))
			{
				ActorId pid = input.value("parent_id", ActorId(0));
				Actor* parent = pid ? scene.FindActor(pid) : nullptr;
				if (pid && !parent) { throw std::runtime_error(std::format("parent {} not found", pid)); }
				if (parent && (parent == actor || parent->IsDescendantOf(actor))) { throw std::runtime_error("cannot parent an actor to itself or its child"); }
				if (parent != actor->GetParent())
				{
					auto command = std::make_unique<ReparentCommand>("AI: Reparent " + actor->GetName(), actor, parent);
					command->Redo(scene);
					host.PushCommand(std::move(command));
				}
			}

			Json before = scene.SerializeActorState(actor);
			Json state = before;
			if (input.contains("name")) { state["Name"] = input.value("name", ""); }
			if (input.contains("active")) { state["Active"] = input.value("active", true); }
			if (input.contains("tag")) { state["Tag"] = input.value("tag", ""); }
			ApplyTransform(state, input);

			Json& comps = state["Components"];
			for (const auto& removeType : input.value("remove_components", Json::array()))
			{
				std::string type = removeType.get<std::string>();
				auto it = std::find_if(comps.begin(), comps.end(), [&](const Json& c) { return c.value("Type", "") == type; });
				if (it == comps.end()) { throw std::runtime_error("the actor has no " + type); }
				comps.erase(it);
			}
			for (const auto& set : input.value("set_properties", Json::array()))
			{
				std::string type = set.value("type", "");
				auto it = std::find_if(comps.begin(), comps.end(), [&](const Json& c) { return c.value("Type", "") == type; });
				if (it == comps.end()) { throw std::runtime_error("the actor has no " + type + " (use add_components)"); }
				if (auto props = set.find("properties"); props != set.end() && props->is_object()) { it->merge_patch(*props); }
			}
			for (const auto& spec : input.value("add_components", Json::array())) { comps.push_back(MakeComponentJson(spec)); }

			scene.ApplyActorState(actor, state);
			Json after = scene.SerializeActorState(actor);
			if (after != before)
			{
				host.PushCommand(std::make_unique<ActorStateCommand>("AI: Edit " + actor->GetName(), actor->GetId(), std::move(before), after));
			}
			return DescribeActor(scene, actor).dump();
		}

		if (name == "delete_actor")
		{
			Actor* actor = findActor("actor_id");
			auto command = ActorLifetimeCommand::Deleting("AI: Delete " + actor->GetName(), scene, { actor });
			if (host.GetSelected() == actor || (host.GetSelected() && host.GetSelected()->IsDescendantOf(actor))) { host.Select(nullptr); }
			actor->Destroy();
			scene.FlushPendingDestroy();
			host.PushCommand(std::move(command));
			return "deleted";
		}

		isError = true;
		return "unknown tool: " + name;
	}

	std::string AssistantTools::Run(AssistantToolHost& host, const std::string& name, const Json& input, bool& isError)
	{
		// 入力の誤り（存在しない ID・型の違う値など）は例外にせず、エラーとして Claude に返す（Claude が直して呼び直せる）
		try
		{
			return RunUnchecked(host, name, input, isError);
		}
		catch (const std::exception& e)
		{
			isError = true;
			return std::string("Error: ") + e.what();
		}
	}
}