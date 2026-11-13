#include "TestFramework.h"
#include "TestComponents.h"

#if RD_EDITOR
#include "Editor/AssistantTools.h"
#include "Editor/CommandHistory.h"
#include "Scene/Scene.h"

using namespace rd;
using namespace rdtest;

// AI アシスタントのツールを、API を呼ばずに直接試す（Claude が送ってくるのと同じ形の JSON を渡す）
namespace
{
	class FakeHost final : public AssistantToolHost
	{
	public:
		Scene scene;
		CommandHistory history;
		Actor* selected = nullptr;
		bool playing = false;

		Scene& GetScene() override { return scene; }
		void PushCommand(std::unique_ptr<EditorCommand> command) override { history.Push(std::move(command)); }
		Actor* GetSelected() override { return selected; }
		void Select(Actor* actor) override { selected = actor; }
		bool IsPlaying() override { return playing; }
	};

	Json Call(FakeHost& host, const std::string& name, const Json& input, bool expectError = false)
	{
		bool isError = false;
		std::string result = AssistantTools::Run(host, name, input, isError);
		RD_CHECK_EQ(isError, expectError);
		try { return Json::parse(result); }
		catch (...) { return result; }
	}
}

RD_TEST(Assistant, ToolDefinitionsAreWellFormed)
{
	const Json& tools = AssistantTools::GetDefinitions();
	RD_REQUIRE(tools.is_array() && tools.size() >= 8);
	for (const auto& t : tools)
	{
		RD_CHECK(t.contains("name") && t.contains("description"));
		RD_CHECK(t["input_schema"].value("type", "") == "object");
		RD_CHECK(t["input_schema"]["properties"].is_object());
		RD_CHECK(t["input_schema"]["required"].is_array());
	}
}

RD_TEST(Assistant, CreateModifyDeleteWithUndo)
{
	FakeHost host;
	Json created = Call(host, "create_actor", {
		{ "name", "Red Box" },
		{ "position", { 1, 2, 3 } },
		{ "rotation", { 0, 90, 0 } },
		{ "components", Json::array({ { { "type", "Recorder" }, { "properties", { { "Value", 42 } } } } }) },
	});
	ActorId id = created.value("created_id", ActorId(0));
	Actor* actor = host.scene.FindActor(id);
	RD_REQUIRE(actor != nullptr);
	RD_CHECK_EQ(actor->GetName(), std::string("Red Box"));
	RD_CHECK(actor->GetTransform().GetLocalPosition() == Vector3(1, 2, 3));
	RD_REQUIRE(actor->GetComponent<Recorder>() != nullptr);
	RD_CHECK_EQ(actor->GetComponent<Recorder>()->value, 42);
	// 回転は度で受け取り、度で返す
	Json described = Call(host, "get_actor", { { "actor_id", id } });
	RD_CHECK_NEAR(described["Transform"]["Rotation"][1].get<float>(), 90.0f, 0.01f);

	// 値を変える・コンポーネントを足す
	Call(host, "modify_actor", {
		{ "actor_id", id },
		{ "name", "Blue Box" },
		{ "set_properties", Json::array({ { { "type", "Recorder" }, { "properties", { { "Value", 7 } } } } }) },
		{ "add_components", Json::array({ { { "type", "Passive" } } }) },
	});
	RD_CHECK_EQ(actor->GetName(), std::string("Blue Box"));
	RD_CHECK_EQ(actor->GetComponent<Recorder>()->value, 7);
	RD_CHECK(actor->GetComponent<Passive>() != nullptr);

	// Ctrl+Z で 1 つずつ戻る
	host.history.Undo(host.scene);
	RD_CHECK_EQ(actor->GetName(), std::string("Red Box"));
	RD_CHECK(actor->GetComponent<Passive>() == nullptr);
	host.history.Redo(host.scene);

	Call(host, "delete_actor", { { "actor_id", id } });
	RD_CHECK(host.scene.FindActor(id) == nullptr);
	host.history.Undo(host.scene);
	RD_CHECK(host.scene.FindActor(id) != nullptr);// 同じ ID で戻る
	host.history.Undo(host.scene);
	host.history.Undo(host.scene);
	RD_CHECK(host.scene.FindActor(id) == nullptr);// 作成も取り消せる
}

RD_TEST(Assistant, ReferencesAndReparent)
{
	FakeHost host;
	ActorId target = Call(host, "create_actor", { { "name", "Target" } }).value("created_id", ActorId(0));
	ActorId holder = Call(host, "create_actor", { { "name", "Holder" }, { "position", { 5, 0, 0 } },
		{ "components", Json::array({ { { "type", "AllProps" }, { "properties", { { "Target", { { "$ref", target } } }, { "Mood", "Angry" } } } } }) } })
		.value("created_id", ActorId(0));
	auto* props = host.scene.FindActor(holder)->GetComponent<AllProps>();
	RD_REQUIRE(props != nullptr);
	RD_CHECK(props->target.Get() == host.scene.FindActor(target));
	RD_CHECK(props->mood == Mood::Angry);// 列挙は名前で指定できる

	Call(host, "modify_actor", { { "actor_id", target }, { "parent_id", holder } });
	RD_CHECK(host.scene.FindActor(target)->GetParent() == host.scene.FindActor(holder));
	RD_CHECK_NEAR(host.scene.FindActor(target)->GetTransform().GetPosition().x, 0.0f, 1.0e-4f);// ワールド位置は保つ
	host.history.Undo(host.scene);
	RD_CHECK(host.scene.FindActor(target)->GetParent() == nullptr);

	// 自分の子を親にはできない
	Call(host, "modify_actor", { { "actor_id", holder }, { "parent_id", holder } }, true);
}

RD_TEST(Assistant, ErrorsAreReportedNotThrown)
{
	FakeHost host;
	Json r = Call(host, "get_actor", { { "actor_id", 999 } }, true);
	RD_CHECK(r.get<std::string>().find("not found") != std::string::npos);
	Call(host, "create_actor", { { "name", "X" }, { "components", Json::array({ { { "type", "NoSuchComponent" } } }) } }, true);
	RD_CHECK_EQ(host.scene.GetActorCount(), size_t(0));// 失敗したら何も作らない
	Call(host, "no_such_tool", Json::object(), true);

	// プレイ中は変更しない（止めたときに消えてしまうので）
	host.playing = true;
	Call(host, "create_actor", { { "name", "X" } }, true);
	RD_CHECK_EQ(host.scene.GetActorCount(), size_t(0));
	host.playing = false;

	// 一覧のツール
	Json scene = Call(host, "get_scene", Json::object());
	RD_CHECK(scene["actors"].is_array());
	Json types = Call(host, "list_component_types", Json::object());
	bool hasRecorder = false;
	for (auto& t : types) { hasRecorder |= t.value("type", "") == "Recorder"; }
	RD_CHECK(hasRecorder);
}
#endif
