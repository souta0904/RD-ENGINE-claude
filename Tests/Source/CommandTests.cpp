#include "TestFramework.h"
#include "TestComponents.h"
#include "Scene/Scene.h"

#if RD_EDITOR
#include "Editor/CommandHistory.h"
#include <thread>

using namespace rd;
using namespace rdtest;

namespace
{
	// エディタと同じ手順: 変更前を覚える → 変更する → コマンドを積む
	template <class Func>
	void EditActor(CommandHistory& history, Scene& scene, Actor* actor, const char* label, Func&& edit)
	{
		Json before = scene.SerializeActorState(actor);
		edit();
		history.Push(std::make_unique<ActorStateCommand>(label, actor->GetId(), std::move(before), scene.SerializeActorState(actor)));
	}
}

RD_TEST(Undo, ActorStateUndoRedo)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");
	auto* r = a->AddComponent<Recorder>();

	EditActor(history, scene, a, "Set Value", [&] { r->value = 5; });
	EditActor(history, scene, a, "Rename", [&] { a->SetName("B"); });
	RD_CHECK_EQ(history.GetUndoLabel(), std::string("Rename"));

	RD_CHECK_EQ(history.Undo(scene), a->GetId());
	RD_CHECK_EQ(a->GetName(), std::string("A"));
	history.Undo(scene);
	RD_CHECK_EQ(r->value, 0);
	RD_CHECK(!history.CanUndo());

	history.Redo(scene);
	history.Redo(scene);
	RD_CHECK_EQ(r->value, 5);
	RD_CHECK_EQ(a->GetName(), std::string("B"));
	RD_CHECK(!history.CanRedo());
}

RD_TEST(Undo, MergeContinuousEdits)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");
	auto* r = a->AddComponent<Recorder>();
	// ドラッグ中のように同じ操作が続いたら 1 つにまとまる
	for (int i = 1; i <= 10; ++i) { EditActor(history, scene, a, "Drag", [&] { r->value = i; }); }
	history.Undo(scene);
	RD_CHECK_EQ(r->value, 0);
	RD_CHECK(!history.CanUndo());

	// 違う操作はまとめない
	history.Redo(scene);
	EditActor(history, scene, a, "Other", [&] { r->value = 100; });
	history.Undo(scene);
	RD_CHECK_EQ(r->value, 10);
}

RD_TEST(Undo, DeleteRestoresSameIdsAndRefs)
{
	Scene scene;
	CommandHistory history;
	Actor* holder = scene.CreateActor("Holder");
	Actor* parent = scene.CreateActor("Parent");
	Actor* target = scene.CreateActor("Target", parent);
	scene.CreateActor("Sibling", parent);
	scene.CreateActor("Child", target);
	auto* p = holder->AddComponent<AllProps>();
	p->target = target;
	ActorId targetId = target->GetId();

	auto cmd = ActorLifetimeCommand::Deleting("Delete", scene, { target });
	target->Destroy();
	scene.FlushPendingDestroy();
	history.Push(std::move(cmd));
	RD_CHECK(p->target.Get() == nullptr);
	RD_CHECK_EQ(scene.GetActorCount(), size_t(3));

	RD_CHECK_EQ(history.Undo(scene), targetId);
	Actor* restored = scene.FindActor(targetId);
	RD_REQUIRE(restored != nullptr);
	RD_CHECK(p->target.Get() == restored);// 同じ ID で戻るので参照も戻る
	RD_CHECK(restored->GetParent() == parent);
	RD_CHECK_EQ(restored->GetSiblingIndex(), 0);// 兄弟の順番も戻る
	RD_CHECK(restored->FindChild("Child") != nullptr);

	history.Redo(scene);
	RD_CHECK(scene.FindActor(targetId) == nullptr);
}

RD_TEST(Undo, CreateAndReparent)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");
	Actor* b = scene.CreateActor("B");
	b->GetTransform().SetLocalPosition({ 5, 0, 0 });

	auto reparent = std::make_unique<ReparentCommand>("Reparent", a, b);
	reparent->Redo(scene);
	history.Push(std::move(reparent));
	RD_CHECK(a->GetParent() == b);
	RD_CHECK_NEAR(a->GetTransform().GetPosition().x, 0.0f, 1.0e-4f);// ワールド位置は保つ

	history.Undo(scene);
	RD_CHECK(a->GetParent() == nullptr);
	RD_CHECK_EQ(a->GetSiblingIndex(), 0);

	Actor* c = scene.CreateActor("C");
	ActorId cId = c->GetId();
	history.Push(ActorLifetimeCommand::Created("Create", scene, { c }));
	history.Undo(scene);
	RD_CHECK(scene.FindActor(cId) == nullptr);
	history.Redo(scene);
	RD_CHECK(scene.FindActor(cId) != nullptr);
}

RD_TEST(Undo, SettingsAndComposite)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");
	auto composite = std::make_unique<CompositeCommand>("Both");

	Json before = scene.SerializeSettings();
	scene.SetGravity({ 0, 1, 0 });
	composite->Add(std::make_unique<SceneSettingsCommand>("Gravity", before, scene.SerializeSettings()));
	Json aBefore = scene.SerializeActorState(a);
	a->SetName("Changed");
	composite->Add(std::make_unique<ActorStateCommand>("Name", a->GetId(), aBefore, scene.SerializeActorState(a)));
	history.Push(std::move(composite));

	history.Undo(scene);
	RD_CHECK_NEAR(scene.GetGravity().y, -9.81f, 1.0e-4f);
	RD_CHECK_EQ(a->GetName(), std::string("A"));
	history.Redo(scene);
	RD_CHECK_NEAR(scene.GetGravity().y, 1.0f, 1.0e-4f);
	RD_CHECK_EQ(a->GetName(), std::string("Changed"));
}

RD_TEST(Undo, DirtyTracking)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");
	RD_CHECK(!history.IsDirty());
	EditActor(history, scene, a, "One", [&] { a->SetName("1"); });
	RD_CHECK(history.IsDirty());
	history.MarkSaved();
	RD_CHECK(!history.IsDirty());

	// 保存した直後の操作は、前のものとまとめない（保存位置へ戻れるように）
	EditActor(history, scene, a, "One", [&] { a->SetName("2"); });
	RD_CHECK(history.IsDirty());
	history.Undo(scene);
	RD_CHECK(!history.IsDirty());
	RD_CHECK_EQ(a->GetName(), std::string("1"));

	// 保存位置より先を Undo してから別の操作をすると、保存した状態には戻れない
	history.Undo(scene);
	EditActor(history, scene, a, "Two", [&] { a->SetName("x"); });
	RD_CHECK(history.IsDirty());
	history.Undo(scene);
	RD_CHECK(history.IsDirty());
}

RD_TEST(Undo, HistoryIsBounded)
{
	Scene scene;
	CommandHistory history;
	Actor* a = scene.CreateActor("A");
	for (int i = 0; i < 250; ++i)
	{
		// ラベルを変えてまとめないようにする
		EditActor(history, scene, a, i % 2 ? "Odd" : "Even", [&] { a->SetName(std::to_string(i)); });
	}
	int undos = 0;
	while (history.CanUndo()) { history.Undo(scene); ++undos; }
	RD_CHECK_EQ(undos, 200);
	RD_CHECK_EQ(a->GetName(), std::string("49"));
}
#endif
