#include "TestFramework.h"
#include "TestComponents.h"
#include "Scene/Component/ComponentRegistry.h"
#include "Scene/Component/MissingComponent.h"
#include "Scene/Scene.h"

using namespace rd;
using namespace rdtest;

// ゲームのコードの読み直し（ホットリロード）で使う仕組みのテスト
//  DLL の読み直しそのものはエディタでしか試せないので、ここでは登録の入れ替えと Missing の扱いを確かめる

namespace
{
	// 1 つのアクターに、登録されていない型のコンポーネントを 1 つ持つシーンの JSON
	Json SceneWithUnknownComponent()
	{
		Json comp = { { "Type", "NoSuchComponent" }, { "Enabled", false }, { "Speed", 3.5 }, { "Name", "abc" } };
		Json actor = { { "Id", 1 }, { "Parent", 0 }, { "Name", "A" }, { "Components", Json::array({ comp }) } };
		return { { "Version", 1 }, { "Name", "Test" }, { "Actors", Json::array({ actor }) } };
	}
}

RD_TEST(HotReload, MissingComponentKeepsValues)
{
	Scene scene;
	scene.Deserialize(SceneWithUnknownComponent());
	Actor* actor = scene.FindActorByName("A");
	RD_REQUIRE(actor != nullptr);
	RD_REQUIRE(actor->GetAllComponents().size() == 1);
	auto* missing = dynamic_cast<MissingComponent*>(actor->GetAllComponents()[0].get());
	RD_REQUIRE(missing != nullptr);
	RD_CHECK(std::string(missing->GetTypeName()) == "NoSuchComponent");
	RD_CHECK(!missing->IsEnabled());

	// 保存し直しても、元の型名と値がそのまま残る
	Json saved = scene.Serialize();
	const Json& comp = saved["Actors"][0]["Components"][0];
	RD_CHECK(comp.value("Type", "") == "NoSuchComponent");
	RD_CHECK(comp.value("Speed", 0.0) == 3.5);
	RD_CHECK(comp.value("Name", "") == "abc");
	RD_CHECK(comp.value("Enabled", true) == false);
}

RD_TEST(HotReload, MissingComponentComesBackWhenRegistered)
{
	Scene first;
	first.Deserialize(SceneWithUnknownComponent());
	Json saved = first.Serialize();

	// 型が登録された（ゲームのコードを読み直した）あとで作り直すと、元のコンポーネントになる
	ComponentRegistry::SetLoadingModule(7);
	ComponentRegistry::Add({ "NoSuchComponent", "Test", [] { return std::make_unique<Passive>(); } });
	ComponentRegistry::SetLoadingModule(0);

	Scene second;
	second.Deserialize(saved);
	Actor* actor = second.FindActorByName("A");
	RD_REQUIRE(actor != nullptr && actor->GetAllComponents().size() == 1);
	RD_CHECK(dynamic_cast<MissingComponent*>(actor->GetAllComponents()[0].get()) == nullptr);
	RD_CHECK(dynamic_cast<Passive*>(actor->GetAllComponents()[0].get()) != nullptr);

	// 後片付け（ほかのテストに残さない）
	auto removed = ComponentRegistry::RemoveGameEntries();
	RD_CHECK(removed.size() == 1 && removed[0].name == "NoSuchComponent" && removed[0].module == 7);
}

RD_TEST(HotReload, RemoveAndRestoreGameEntries)
{
	size_t before = ComponentRegistry::GetEntries().size();
	ComponentRegistry::SetLoadingModule(3);
	ComponentRegistry::Add({ "HotReloadA", "Test", [] { return std::make_unique<Passive>(); } });
	ComponentRegistry::Add({ "HotReloadB", "Test", [] { return std::make_unique<Passive>(); } });
	ComponentRegistry::SetLoadingModule(0);
	RD_CHECK(ComponentRegistry::Has("HotReloadA") && ComponentRegistry::Has("HotReloadB"));

	// ゲームの登録だけが外れ、エンジン（モジュール 0）の登録は残る
	auto removed = ComponentRegistry::RemoveGameEntries();
	RD_CHECK(removed.size() == 2);
	RD_CHECK(!ComponentRegistry::Has("HotReloadA"));
	RD_CHECK(ComponentRegistry::GetEntries().size() == before);

	// 新しい DLL を読めなかったときは、外したものを戻す
	ComponentRegistry::RestoreEntries(std::move(removed));
	RD_CHECK(ComponentRegistry::Has("HotReloadA") && ComponentRegistry::Has("HotReloadB"));
	ComponentRegistry::RemoveGameEntries();
	RD_CHECK(ComponentRegistry::GetEntries().size() == before);
}
