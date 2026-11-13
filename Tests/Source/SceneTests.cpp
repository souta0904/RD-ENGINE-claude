#include "TestFramework.h"
#include "TestComponents.h"
#include "Core/Time.h"
#include "Scene/ComponentRegistry.h"
#include "Scene/Scene.h"
#include <algorithm>

using namespace rd;
using namespace rdtest;

RD_REGISTER_COMPONENT(Recorder, "Test");
RD_REGISTER_COMPONENT(Passive, "Test");
RD_REGISTER_COMPONENT(LateOnly, "Test");
RD_REGISTER_COMPONENT(AllProps, "Test");

namespace
{
	bool Contains(const std::vector<std::string>& log, const std::string& s)
	{
		return std::find(log.begin(), log.end(), s) != log.end();
	}
	size_t IndexOf(const std::vector<std::string>& log, const std::string& s)
	{
		return size_t(std::find(log.begin(), log.end(), s) - log.begin());
	}
}

RD_TEST(Component, CallbackFlagsDetectOverrides)
{
	RD_CHECK_EQ(GetComponentTypeInfo<Passive>().callbacks, 0u);
	RD_CHECK_EQ(GetComponentTypeInfo<LateOnly>().callbacks, uint32_t(kCallbackLateUpdate));
	uint32_t rec = GetComponentTypeInfo<Recorder>().callbacks;
	RD_CHECK((rec & kCallbackUpdate) && (rec & kCallbackFixedUpdate));
	RD_CHECK(!(rec & kCallbackLateUpdate) && !(rec & kCallbackRender));
}

RD_TEST(Scene, LifecycleOrder)
{
	std::vector<std::string> log;// シーンより先に作る（シーンの破棄で OnDestroy が書き込む）
	Scene scene;
	Actor* a = scene.CreateActor("A");
	auto* r = a->AddComponent<Recorder>();
	r->log = &log;
	// AddComponent の時点では log が無いので Awake は記録されない。もう一度呼んで確認する
	r->Awake();
	RD_CHECK(Contains(log, "A.Awake.0"));

	scene.Update(0.0f, false);// 編集中は Start / Update を呼ばない
	RD_CHECK(!Contains(log, "A.Start.0"));

	scene.Update(Time::FixedDeltaTime(), true);
	RD_REQUIRE(Contains(log, "A.Start.0"));
	RD_REQUIRE(Contains(log, "A.Update.0"));
	RD_CHECK(IndexOf(log, "A.Start.0") < IndexOf(log, "A.Update.0"));
	RD_CHECK(Contains(log, "A.FixedUpdate.0"));

	log.clear();
	scene.Update(0.016f, true);
	RD_CHECK(!Contains(log, "A.Start.0"));// Start は 1 回だけ

	a->Destroy();
	RD_CHECK(scene.FindActor(a->GetId()) == nullptr);// 破棄予約の時点で見つからない
	scene.FlushPendingDestroy();
	RD_CHECK(Contains(log, "A.OnDestroy.0"));
	RD_CHECK_EQ(scene.GetActorCount(), size_t(0));
}

RD_TEST(Scene, DisabledComponentIsSkipped)
{
	std::vector<std::string> log;
	Scene scene;
	auto* r = scene.CreateActor("A")->AddComponent<Recorder>();
	r->log = &log;
	r->SetEnabled(false);
	scene.Update(0.016f, true);
	RD_CHECK(!Contains(log, "A.Start.0"));
	RD_CHECK(!Contains(log, "A.Update.0"));

	r->SetEnabled(true);// 有効になったら Start から
	scene.Update(0.016f, true);
	RD_CHECK(Contains(log, "A.Start.0"));
	RD_CHECK(Contains(log, "A.Update.0"));

	// 親が非アクティブなら子も止まる
	log.clear();
	Actor* parent = scene.CreateActor("P");
	r->GetActor()->SetParent(parent);
	parent->SetActive(false);
	scene.Update(0.016f, true);
	RD_CHECK(!Contains(log, "A.Update.0"));
}

RD_TEST(Scene, FindComponentsByTypeInAddOrder)
{
	Scene scene;
	auto* l1 = scene.CreateActor("1")->AddComponent<LateOnly>();
	scene.CreateActor("2")->AddComponent<Passive>();
	auto* l3 = scene.CreateActor("3")->AddComponent<LateOnly>();
	auto list = scene.FindComponents<LateOnly>();
	RD_REQUIRE(list.size() == 2);
	RD_CHECK(list[0] == l1 && list[1] == l3);
	RD_CHECK(scene.FindComponent<LateOnly>() == l1);
	RD_CHECK(scene.FindComponents<Component>().size() == 3);// 基底型でも集められる

	scene.Update(0.016f, true);
	RD_CHECK_EQ(l1->calls, 1);

	l1->GetActor()->RemoveComponent(l1);
	scene.FlushPendingDestroy();
	list = scene.FindComponents<LateOnly>();
	RD_CHECK(list.size() == 1 && list[0] == l3);
}

RD_TEST(Scene, SaveLoadRoundTrip)
{
	Scene scene;
	scene.SetGravity({ 0, -3, 0 });
	Actor* target = scene.CreateActor("Target");
	target->AddComponent<Recorder>()->value = 7;
	Actor* holder = scene.CreateActor("Holder");
	Actor* child = scene.CreateActor("Child", holder);
	child->GetTransform().SetLocalPosition({ 1, 2, 3 });
	child->SetTag("Enemy");
	auto* p = holder->AddComponent<AllProps>();
	p->flag = true;
	p->count = -5;
	p->mask = 0xF0u;
	p->speed = 2.5f;
	p->offset = { 1, 2, 3 };
	p->rotation = Quaternion::AngleAxis(Vector3::kUnitY, 1.0f);
	p->tint = { 0.5f, 0.25f, 1.0f, 1.0f };
	p->label = "日本語もOK";
	p->mood = Mood::Sleepy;
	p->target = target;
	p->recorder = target->GetComponent<Recorder>();

	Json json = scene.Serialize();
	// 列挙型は名前で、参照は { "$ref": id } で保存される
	std::string text = json.dump();
	RD_CHECK(text.find("\"Sleepy\"") != std::string::npos);
	RD_CHECK(text.find("\"$ref\"") != std::string::npos);

	Scene loaded;
	RD_REQUIRE(loaded.Deserialize(json));
	RD_CHECK_EQ(loaded.GetActorCount(), size_t(3));
	RD_CHECK_NEAR(loaded.GetGravity().y, -3.0f, 1.0e-6f);
	Actor* h = loaded.FindActorByName("Holder");
	RD_REQUIRE(h != nullptr);
	auto* q = h->GetComponent<AllProps>();
	RD_REQUIRE(q != nullptr);
	RD_CHECK(q->flag && q->count == -5 && q->mask == 0xF0u && q->speed == 2.5f);
	RD_CHECK(q->offset == p->offset && q->tint == p->tint && q->label == p->label && q->mood == Mood::Sleepy);
	RD_CHECK(q->rotation == p->rotation);
	RD_REQUIRE(q->target.Get() != nullptr);
	RD_CHECK_EQ(q->target->GetName(), std::string("Target"));
	RD_REQUIRE(q->recorder.Get() != nullptr);
	RD_CHECK_EQ(q->recorder->value, 7);
	Actor* c = h->FindChild("Child");
	RD_REQUIRE(c != nullptr);
	RD_CHECK(c->GetTransform().GetLocalPosition() == Vector3(1, 2, 3));
	RD_CHECK_EQ(c->GetTag(), std::string("Enemy"));
	RD_CHECK(loaded.FindActorsWithTag("Enemy").size() == 1);
}

RD_TEST(Scene, ReaderKeepsMissingKeys)
{
	AllProps p;
	p.count = 42;
	p.label = "keep";
	Json j = { { "Speed", 3.0f }, { "Count", "not a number" } };// 型が違うものは読まない
	JsonReader r(j);
	p.Reflect(r);
	RD_CHECK_EQ(p.speed, 3.0f);
	RD_CHECK_EQ(p.count, 42);
	RD_CHECK_EQ(p.label, std::string("keep"));
}

RD_TEST(ObjectRef, DanglingRefBecomesNull)
{
	Scene scene;
	Actor* target = scene.CreateActor("T");
	auto* p = scene.CreateActor("H")->AddComponent<AllProps>();
	p->target = target;
	RD_CHECK(p->target.Get() == target);
	RD_CHECK(!p->recorder);// T は Recorder を持たない
	target->Destroy();
	scene.FlushPendingDestroy();
	RD_CHECK(p->target.Get() == nullptr);
	RD_CHECK(!p->target);
}

RD_TEST(ObjectRef, ComponentRefAccepts)
{
	Scene scene;
	Actor* plain = scene.CreateActor("Plain");
	Actor* withRec = scene.CreateActor("WithRecorder");
	withRec->AddComponent<Recorder>();
	ComponentRef<Recorder> ref;
	RD_CHECK(!ref.Accepts(plain));
	RD_CHECK(ref.Accepts(withRec));
	RD_CHECK(ActorRef().Accepts(plain));
	RD_CHECK_EQ(std::string(ref.GetTypeName()), std::string("Recorder"));
}

RD_TEST(ObjectRef, DuplicateRemapsInternalRefs)
{
	Scene scene;
	Actor* outside = scene.CreateActor("Outside");
	Actor* root = scene.CreateActor("Root");
	Actor* inner = scene.CreateActor("Inner", root);
	auto* p = root->AddComponent<AllProps>();
	p->target = inner;// 複製の範囲内
	auto* p2 = inner->AddComponent<AllProps>();
	p2->target = outside;// 範囲外

	Actor* copy = scene.Duplicate(root);
	RD_REQUIRE(copy != nullptr && copy != root);
	Actor* copyInner = copy->FindChild("Inner");
	RD_REQUIRE(copyInner != nullptr && copyInner != inner);
	RD_CHECK(copy->GetComponent<AllProps>()->target.Get() == copyInner);// 複製先を指す
	RD_CHECK(copyInner->GetComponent<AllProps>()->target.Get() == outside);// 外はそのまま

	// プレハブとして読み込むときは、外への参照を外す
	Json json = scene.SerializeActors({ root });
	auto roots = scene.DeserializeActors(json, true, nullptr, false);
	RD_REQUIRE(roots.size() == 1);
	Actor* prefabInner = roots[0]->FindChild("Inner");
	RD_REQUIRE(prefabInner != nullptr);
	RD_CHECK(roots[0]->GetComponent<AllProps>()->target.Get() == prefabInner);
	RD_CHECK(prefabInner->GetComponent<AllProps>()->target.Get() == nullptr);
}

RD_TEST(Scene, ActorStateApplyInPlaceAndRebuild)
{
	std::vector<std::string> log;
	Scene scene;
	Actor* a = scene.CreateActor("A");
	auto* r = a->AddComponent<Recorder>();
	r->log = &log;
	Json before = scene.SerializeActorState(a);

	// 値だけの変更はその場で戻す（同じオブジェクトのまま、Awake / OnDestroy は呼ばない）
	r->value = 99;
	a->SetName("Renamed");
	scene.ApplyActorState(a, before);
	RD_CHECK_EQ(a->GetName(), std::string("A"));
	RD_CHECK(a->GetComponent<Recorder>() == r);
	RD_CHECK_EQ(r->value, 0);
	RD_CHECK(log.empty());

	// コンポーネントを足したら作り直して戻す
	a->AddComponent<Passive>();
	Json after = scene.SerializeActorState(a);
	scene.ApplyActorState(a, before);
	RD_CHECK(a->GetComponent<Passive>() == nullptr);
	RD_CHECK(a->GetComponent<Recorder>() != nullptr);
	RD_CHECK(Contains(log, "A.OnDestroy.0"));
	scene.ApplyActorState(a, after);
	RD_CHECK(a->GetComponent<Passive>() != nullptr);
	RD_CHECK(scene.FindComponents<Recorder>().size() == 1);// 型ごとのリストも作り直されている
}

RD_TEST(Scene, ReparentKeepsWorldPosition)
{
	Scene scene;
	Actor* parent = scene.CreateActor("P");
	parent->GetTransform().SetLocalPosition({ 10, 0, 0 });
	parent->GetTransform().SetLocalScale({ 2, 2, 2 });
	Actor* child = scene.CreateActor("C");
	child->GetTransform().SetLocalPosition({ 4, 0, 0 });
	child->SetParent(parent, true);
	Vector3 world = child->GetTransform().GetPosition();
	RD_CHECK_NEAR(world.x, 4.0f, 1.0e-4f);
	RD_CHECK_NEAR(child->GetTransform().GetLocalPosition().x, -3.0f, 1.0e-4f);
	RD_CHECK(child->IsDescendantOf(parent));
	RD_CHECK(!parent->IsDescendantOf(child));
}
