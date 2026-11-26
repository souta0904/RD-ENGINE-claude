#include "TestFramework.h"
#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "Scene/Scene.h"
#include <filesystem>

using namespace rd;

// 行動ツリーのアセット（.rdbt）: 保存と読み込み、組み立て、子の並べ替え
namespace
{
	// テスト用の行動: 決めた結果を返し、呼ばれた順を記録する
	std::vector<std::string>& Log()
	{
		static std::vector<std::string> log;
		return log;
	}

	bool sFlag = false;

	void RegisterTestNodes()
	{
		bt::NodeTypeInfo info;
		info.name = "TestRecord";
		info.category = "Test";
		bt::ParamDef p;
		p.name = "Label";
		p.type = bt::ParamType::String;
		p.defaultValue = "x";
		info.params = { p };
		bt::ParamDef r;
		r.name = "Succeed";
		r.type = bt::ParamType::Bool;
		r.defaultValue = true;
		info.params.push_back(r);
		info.create = [](Actor&, const Json& params) -> bt::NodePtr {
			auto n = std::make_unique<bt::Action>();
			std::string label = bt::GetString(params, "Label");
			bool ok = bt::GetBool(params, "Succeed", true);
			n->start = [label, ok] {
				Log().push_back(label);
				return ok ? bt::Status::Success : bt::Status::Failure;
				};
			return n;
			};
		bt::Registry::RegisterAction(info);

		bt::ConditionInfo c;
		c.name = "TestFlag";
		c.create = [](Actor&) -> bt::Condition { return [] { return sFlag; }; };
		bt::Registry::RegisterCondition(c);

		bt::SelectorInfo s;
		s.name = "TestIndex";
		s.cases = { "A", "B" };
		s.create = [](Actor&) -> std::function<int()> { return [] { return sFlag ? 1 : 0; }; };
		bt::Registry::RegisterSelector(s);
	}

	uint32_t AddRecord(bt::TreeAsset& asset, const char* label, float x, bool succeed = true)
	{
		bt::NodeData& d = asset.AddNode("TestRecord", Vector2(x, 100.0f));
		d.params["Label"] = label;
		d.params["Succeed"] = succeed;
		return d.id;
	}

	uint32_t RootId(const bt::TreeAsset& asset) { return asset.GetRoot()->id; }
}

RD_TEST(BehaviorTree, NewAssetHasRootAndDefaults)
{
	RegisterTestNodes();
	bt::TreeAsset asset;
	asset.FromJson(Json::object());
	RD_REQUIRE(asset.GetRoot() != nullptr);
	RD_CHECK_EQ(asset.GetRoot()->children.size(), size_t(1));
	RD_CHECK(bt::GetBool(asset.GetRoot()->params, "Repeat") == true);
	bt::NodeData& wait = asset.AddNode(bt::builtin::kWait, Vector2());
	RD_CHECK_NEAR(bt::GetFloat(wait.params, "Seconds"), 1.0f, 1e-6f);
	bt::NodeData& branch = asset.AddNode(bt::builtin::kBranch, Vector2());
	RD_CHECK_EQ(branch.children.size(), size_t(2));
	bt::NodeData& sw = asset.AddNode(bt::builtin::kSwitch, Vector2());
	sw.params["Value"] = "TestIndex";
	asset.Normalize(sw);
	RD_CHECK_EQ(sw.children.size(), size_t(2));
}

RD_TEST(BehaviorTree, SaveAndLoadKeepShape)
{
	RegisterTestNodes();
	bt::TreeAsset asset;
	asset.FromJson(Json::object());
	bt::NodeData& seq = asset.AddNode(bt::builtin::kSequence, Vector2(10.0f, 50.0f));
	seq.comment = "順番";
	uint32_t seqId = seq.id;
	uint32_t a = AddRecord(asset, "a", 0.0f);
	uint32_t b = AddRecord(asset, "b", 200.0f);
	asset.Find(RootId(asset))->children = { seqId };
	asset.Find(seqId)->children = { a, b };

	std::string path = "TestOutput/bt_roundtrip.rdbt";
	std::filesystem::create_directories("TestOutput");
	RD_REQUIRE(asset.Save(path));
	bt::TreeAsset loaded;
	RD_REQUIRE(loaded.Load(path));
	RD_CHECK_EQ(loaded.nodes.size(), asset.nodes.size());
	const bt::NodeData* s = loaded.Find(seqId);
	RD_REQUIRE(s != nullptr);
	RD_CHECK(s->comment == "順番");
	RD_CHECK_NEAR(s->position.x, 10.0f, 1e-4f);
	RD_CHECK(s->children == std::vector<uint32_t>({ a, b }));
	RD_CHECK(bt::GetString(loaded.Find(b)->params, "Label") == "b");
	RD_CHECK(loaded.nextId > b);
	std::filesystem::remove(path);
}

RD_TEST(BehaviorTree, InstantiateRunsSequenceAndSelector)
{
	RegisterTestNodes();
	Scene scene;
	Actor* actor = scene.CreateActor("Agent");
	bt::TreeAsset asset;
	asset.FromJson(Json::object());
	asset.Find(RootId(asset))->params["Repeat"] = false;
	// Selector[ Sequence[a, fail, never], b ] → a, fail, b
	uint32_t sel = asset.AddNode(bt::builtin::kSelector, Vector2()).id;
	uint32_t seq = asset.AddNode(bt::builtin::kSequence, Vector2()).id;
	uint32_t a = AddRecord(asset, "a", 0.0f);
	uint32_t f = AddRecord(asset, "fail", 0.0f, false);
	uint32_t n = AddRecord(asset, "never", 0.0f);
	uint32_t b = AddRecord(asset, "b", 0.0f);
	asset.Find(RootId(asset))->children = { sel };
	asset.Find(sel)->children = { seq, b };
	asset.Find(seq)->children = { a, f, n };

	Log().clear();
	std::unordered_map<uint32_t, bt::Node*> nodes;
	std::string warnings;
	bt::NodePtr root = asset.Instantiate(*actor, &nodes, &warnings);
	RD_REQUIRE(root != nullptr);
	RD_CHECK(warnings.empty());
	RD_CHECK(root->Tick(0.016f) == bt::Status::Success);
	RD_CHECK(Log() == std::vector<std::string>({ "a", "fail", "b" }));
	// 作ったノードはアセットの番号で引ける
	RD_REQUIRE(nodes.contains(f));
	RD_CHECK(nodes[f]->id == f);
	RD_CHECK(nodes[f]->GetLastStatus() == bt::Status::Failure);
	RD_CHECK(nodes.contains(n) && nodes[n]->GetTickCount() == 0);
}

RD_TEST(BehaviorTree, ConditionsBranchAndSwitch)
{
	RegisterTestNodes();
	Scene scene;
	Actor* actor = scene.CreateActor("Agent");
	bt::TreeAsset asset;
	asset.FromJson(Json::object());
	asset.Find(RootId(asset))->params["Repeat"] = false;
	// Sequence[ Branch(TestFlag){t1 | f1}, Switch(TestIndex){A, B}, Guard(not TestFlag){g} ]
	uint32_t seq = asset.AddNode(bt::builtin::kSequence, Vector2()).id;
	bt::NodeData& br = asset.AddNode(bt::builtin::kBranch, Vector2());
	br.params["Condition"] = "TestFlag";
	uint32_t brId = br.id;
	bt::NodeData& sw = asset.AddNode(bt::builtin::kSwitch, Vector2());
	sw.params["Value"] = "TestIndex";
	asset.Normalize(sw);
	uint32_t swId = sw.id;
	bt::NodeData& gd = asset.AddNode(bt::builtin::kGuard, Vector2());
	gd.params["Condition"] = "TestFlag";
	gd.params["Invert"] = true;
	uint32_t gdId = gd.id;
	uint32_t t1 = AddRecord(asset, "t1", 0.0f), f1 = AddRecord(asset, "f1", 0.0f);
	uint32_t ca = AddRecord(asset, "A", 0.0f), cb = AddRecord(asset, "B", 0.0f);
	uint32_t g = AddRecord(asset, "g", 0.0f);
	asset.Find(RootId(asset))->children = { seq };
	asset.Find(seq)->children = { brId, swId, gdId };
	asset.Find(brId)->children = { t1, f1 };
	asset.Find(swId)->children = { ca, cb };
	asset.Find(gdId)->children = { g };

	sFlag = false;
	Log().clear();
	bt::NodePtr root = asset.Instantiate(*actor);
	RD_CHECK(root->Tick(0.016f) == bt::Status::Success);
	RD_CHECK(Log() == std::vector<std::string>({ "f1", "A", "g" }));

	sFlag = true;
	Log().clear();
	root = asset.Instantiate(*actor);
	RD_CHECK(root->Tick(0.016f) == bt::Status::Failure); // Guard(not TestFlag) で失敗
	RD_CHECK(Log() == std::vector<std::string>({ "t1", "B" }));
	sFlag = false;
}

RD_TEST(BehaviorTree, UnknownTypesWarnAndFail)
{
	RegisterTestNodes();
	Scene scene;
	Actor* actor = scene.CreateActor("Agent");
	bt::TreeAsset asset;
	asset.FromJson(Json::object());
	asset.Find(RootId(asset))->params["Repeat"] = false;
	bt::NodeData& missing = asset.AddNode("NoSuchAction", Vector2());
	asset.Find(RootId(asset))->children = { missing.id };
	std::string warnings;
	bt::NodePtr root = asset.Instantiate(*actor, nullptr, &warnings);
	RD_REQUIRE(root != nullptr);
	RD_CHECK(warnings.find("NoSuchAction") != std::string::npos);
	RD_CHECK(root->Tick(0.016f) == bt::Status::Failure);
}

RD_TEST(BehaviorTree, CompositeChildrenFollowHorizontalPosition)
{
	RegisterTestNodes();
	bt::TreeAsset asset;
	asset.FromJson(Json::object());
	uint32_t seq = asset.AddNode(bt::builtin::kSequence, Vector2()).id;
	uint32_t a = AddRecord(asset, "a", 300.0f);
	uint32_t b = AddRecord(asset, "b", -100.0f);
	uint32_t c = AddRecord(asset, "c", 50.0f);
	asset.Find(seq)->children = { a, b, c };
	RD_CHECK(asset.SortChildrenByPosition());
	RD_CHECK(asset.Find(seq)->children == std::vector<uint32_t>({ b, c, a }));
	RD_CHECK(!asset.SortChildrenByPosition());

	// 消すと親から外れる
	asset.RemoveNode(c);
	RD_CHECK(asset.Find(seq)->children == std::vector<uint32_t>({ b, a }));
	RD_CHECK(asset.Find(c) == nullptr);

	// 並べ直すと、子は親より下に、左から順に並ぶ
	asset.Find(RootId(asset))->children = { seq };
	asset.AutoLayout(100.0f, 50.0f);
	RD_CHECK(asset.Find(seq)->position.y > asset.GetRoot()->position.y);
	RD_CHECK(asset.Find(b)->position.x < asset.Find(a)->position.x);
	RD_CHECK(asset.Find(a)->position.y > asset.Find(seq)->position.y);
}
