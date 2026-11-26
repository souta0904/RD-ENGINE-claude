#include "RdPch.h"
#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "Core/Util/StringUtil.h"
#include "Scene/Actor/Actor.h"
#include <algorithm>
#include <unordered_set>

namespace rd::bt
{
	namespace
	{
		constexpr int kVersion = 1;

		// 子の結果を逆にする
		class Inverter final : public Node
		{
		public:
			NodePtr child;

		protected:
			Status OnStart() override { return Flip(child->Tick(0.0f)); }
			Status OnUpdate(float dt) override { return Flip(child->Tick(dt)); }
			void OnAbort() override { child->Abort(); }

		private:
			static Status Flip(Status s)
			{
				if (s == Status::Success) { return Status::Failure; }
				if (s == Status::Failure) { return Status::Success; }
				return s;
			}
		};

		NodePtr MakeResult(Status status)
		{
			auto n = std::make_unique<Action>();
			n->start = [status] { return status; };
			return n;
		}

		// アセットからツリーを組み立てる（間違いは警告に書き、失敗するノードに置き換えて続ける）
		class Builder
		{
		public:
			Builder(const TreeAsset& asset, Actor& self, std::unordered_map<uint32_t, Node*>* nodes, std::string* warnings)
				: mAsset(asset), mSelf(self), mNodes(nodes), mWarnings(warnings) {}

			NodePtr Build(uint32_t id)
			{
				const NodeData* d = mAsset.Find(id);
				if (!d) { return nullptr; }
				if (!mVisited.insert(id).second)
				{
					Warn(std::format("node {} is used twice (loop)", id));
					return nullptr;
				}
				NodePtr node = Create(*d);
				if (!node) { node = MakeResult(Status::Failure); }
				node->id = id;
				node->name = d->comment.empty() ? d->type : d->comment;
				if (mNodes) { (*mNodes)[id] = node.get(); }
				return node;
			}

		private:
			uint32_t Child(const NodeData& d, size_t slot) const { return slot < d.children.size() ? d.children[slot] : 0; }

			// つないでいなければ失敗するノードにする（子が必ずある前提のノードのため）
			NodePtr BuildOrFail(uint32_t id)
			{
				NodePtr n = id ? Build(id) : nullptr;
				return n ? std::move(n) : MakeResult(Status::Failure);
			}

			Condition MakeCondition(const NodeData& d)
			{
				std::string name = GetString(d.params, "Condition");
				const ConditionInfo* info = Registry::FindCondition(name);
				if (!info || !info->create)
				{
					Warn(std::format("unknown condition '{}' in {}", name, d.type));
					return [] { return false; };
				}
				Condition c = info->create(mSelf);
				if (!c) { return [] { return false; }; }
				if (GetBool(d.params, "Invert")) { return [c] { return !c(); }; }
				return c;
			}

			NodePtr Create(const NodeData& d)
			{
				using namespace builtin;
				const NodeTypeInfo* info = Registry::FindType(d.type);
				if (!info)
				{
					Warn(std::format("unknown node type '{}'", d.type));
					return nullptr;
				}
				switch (info->kind)
				{
				case NodeKind::Root:
				{
					NodePtr child = BuildOrFail(Child(d, 0));
					return GetBool(d.params, "Repeat", true) ? MakeRepeat(std::move(child)) : std::move(child);
				}
				case NodeKind::Composite:
				{
					std::vector<NodePtr> children;
					for (uint32_t c : d.children)
					{
						if (NodePtr n = c ? Build(c) : nullptr) { children.push_back(std::move(n)); }
					}
					if (d.type == kSelector)
					{
						auto n = std::make_unique<Selector>();
						n->children = std::move(children);
						return n;
					}
					auto n = std::make_unique<Sequence>();
					n->children = std::move(children);
					return n;
				}
				case NodeKind::Decorator:
				{
					NodePtr child = BuildOrFail(Child(d, 0));
					if (d.type == kInverter)
					{
						auto n = std::make_unique<Inverter>();
						n->child = std::move(child);
						return n;
					}
					if (d.type == kGuard) { return MakeGuard(MakeCondition(d), std::move(child)); }
					return MakeRepeat(std::move(child));
				}
				case NodeKind::Branch:
				{
					auto n = std::make_unique<Branch>();
					n->condition = MakeCondition(d);
					n->onTrue = Child(d, 0) ? Build(Child(d, 0)) : nullptr;
					n->onFalse = Child(d, 1) ? Build(Child(d, 1)) : nullptr;
					return n;
				}
				case NodeKind::Switch:
				{
					auto n = std::make_unique<Switch>();
					std::string name = GetString(d.params, "Value");
					const SelectorInfo* sel = Registry::FindSelector(name);
					if (sel && sel->create) { n->selector = sel->create(mSelf); }
					else { Warn(std::format("unknown selector '{}' in Switch", name)); }
					for (uint32_t c : d.children) { n->cases.push_back(c ? Build(c) : nullptr); }
					return n;
				}
				case NodeKind::Leaf:
				{
					if (d.type == kCheck)
					{
						auto n = std::make_unique<Action>();
						Condition c = MakeCondition(d);
						n->start = [c] { return c() ? Status::Success : Status::Failure; };
						return n;
					}
					if (!info->create) { return nullptr; }
					NodePtr n = info->create(mSelf, d.params);
					if (!n) { Warn(std::format("{} could not be created for {}", d.type, mSelf.GetName())); }
					return n;
				}
				}
				return nullptr;
			}

			void Warn(const std::string& text)
			{
				if (mWarnings)
				{
					if (!mWarnings->empty()) { *mWarnings += "\n"; }
					*mWarnings += text;
				}
			}

			const TreeAsset& mAsset;
			Actor& mSelf;
			std::unordered_map<uint32_t, Node*>* mNodes;
			std::string* mWarnings;
			std::unordered_set<uint32_t> mVisited;
		};

		std::vector<TreeInstance*>& Instances()
		{
			static std::vector<TreeInstance*> list;
			return list;
		}
	}

	bool TreeAsset::IsTreePath(const std::string& path)
	{
		return path.size() > 5 && ToLower(path.substr(path.size() - 5)) == kExtension;
	}

	bool TreeAsset::CreateFile(const std::string& path)
	{
		TreeAsset asset;
		asset.AddNode(builtin::kRoot, Vector2(0.0f, 0.0f));
		return asset.Save(path);
	}

	bool TreeAsset::Load(const std::string& path)
	{
		Json json;
		if (!LoadJsonFile(path, json)) { return false; }
		FromJson(json);
		return true;
	}

	bool TreeAsset::Save(const std::string& path) const
	{
		return SaveJsonFile(path, ToJson());
	}

	void TreeAsset::FromJson(const Json& json)
	{
		nodes.clear();
		nextId = 1;
		for (const Json& j : json.value("Nodes", Json::array()))
		{
			NodeData d;
			d.id = j.value("Id", 0u);
			d.type = j.value("Type", std::string());
			d.comment = j.value("Comment", std::string());
			if (j.contains("Position")) { d.position = j["Position"].get<Vector2>(); }
			if (j.contains("Params") && j["Params"].is_object()) { d.params = j["Params"]; }
			for (const Json& c : j.value("Children", Json::array())) { d.children.push_back(c.get<uint32_t>()); }
			if (d.id == 0 || d.type.empty() || Find(d.id)) { continue; }
			nextId = std::max(nextId, d.id + 1);
			nodes.push_back(std::move(d));
		}
		if (!GetRoot()) { AddNode(builtin::kRoot, Vector2(0.0f, 0.0f)); }
		for (NodeData& d : nodes) { Normalize(d); }
	}

	Json TreeAsset::ToJson() const
	{
		Json nodesJson = Json::array();
		for (const NodeData& d : nodes)
		{
			Json j;
			j["Id"] = d.id;
			j["Type"] = d.type;
			if (!d.comment.empty()) { j["Comment"] = d.comment; }
			// 位置は 0.1 px で丸める（差分を見やすく）
			j["Position"] = Vector2(std::round(d.position.x * 10.0f) / 10.0f, std::round(d.position.y * 10.0f) / 10.0f);
			if (!d.params.empty()) { j["Params"] = d.params; }
			if (!d.children.empty()) { j["Children"] = d.children; }
			nodesJson.push_back(std::move(j));
		}
		Json json;
		json["Version"] = kVersion;
		json["Nodes"] = std::move(nodesJson);
		return json;
	}

	NodeData* TreeAsset::Find(uint32_t id)
	{
		for (NodeData& d : nodes)
		{
			if (d.id == id) { return &d; }
		}
		return nullptr;
	}

	const NodeData* TreeAsset::Find(uint32_t id) const
	{
		return const_cast<TreeAsset*>(this)->Find(id);
	}

	const NodeData* TreeAsset::GetRoot() const
	{
		for (const NodeData& d : nodes)
		{
			if (d.type == builtin::kRoot) { return &d; }
		}
		return nullptr;
	}

	NodeData* TreeAsset::FindParent(uint32_t id, int* slot)
	{
		for (NodeData& d : nodes)
		{
			for (size_t i = 0; i < d.children.size(); ++i)
			{
				if (d.children[i] == id)
				{
					if (slot) { *slot = int(i); }
					return &d;
				}
			}
		}
		return nullptr;
	}

	NodeData& TreeAsset::AddNode(const std::string& type, const Vector2& position)
	{
		NodeData d;
		d.id = nextId++;
		d.type = type;
		d.position = position;
		Normalize(d);
		nodes.push_back(std::move(d));
		return nodes.back();
	}

	void TreeAsset::RemoveNode(uint32_t id)
	{
		for (NodeData& d : nodes)
		{
			for (uint32_t& c : d.children)
			{
				if (c == id) { c = 0; }
			}
			if (GetFixedSlotCount(d) < 0) { std::erase(d.children, 0u); }
		}
		std::erase_if(nodes, [id](const NodeData& d) { return d.id == id; });
	}

	int TreeAsset::GetFixedSlotCount(const NodeData& node)
	{
		const NodeTypeInfo* info = Registry::FindType(node.type);
		if (!info) { return int(node.children.size()); }
		switch (info->kind)
		{
		case NodeKind::Root:
		case NodeKind::Decorator: return 1;
		case NodeKind::Branch: return 2;
		case NodeKind::Switch:
		{
			const SelectorInfo* sel = Registry::FindSelector(GetString(node.params, "Value"));
			return sel ? int(sel->cases.size()) : std::max(1, int(node.children.size()));
		}
		case NodeKind::Leaf: return 0;
		case NodeKind::Composite: return -1;
		}
		return 0;
	}

	void TreeAsset::Normalize(NodeData& node) const
	{
		const NodeTypeInfo* info = Registry::FindType(node.type);
		if (!info) { return; }
		if (!node.params.is_object()) { node.params = Json::object(); }
		for (const ParamDef& p : info->params)
		{
			if (!node.params.contains(p.name)) { node.params[p.name] = p.defaultValue; }
		}
		int slots = GetFixedSlotCount(node);
		if (slots < 0) { std::erase(node.children, 0u); }
		else { node.children.resize(size_t(slots), 0u); }
	}

	bool TreeAsset::SortChildrenByPosition()
	{
		bool changed = false;
		for (NodeData& d : nodes)
		{
			if (GetFixedSlotCount(d) >= 0 || d.children.size() < 2) { continue; }
			std::vector<uint32_t> sorted = d.children;
			std::stable_sort(sorted.begin(), sorted.end(), [this](uint32_t a, uint32_t b) {
				const NodeData* na = Find(a);
				const NodeData* nb = Find(b);
				return (na ? na->position.x : 0.0f) < (nb ? nb->position.x : 0.0f);
				});
			if (sorted != d.children)
			{
				d.children = std::move(sorted);
				changed = true;
			}
		}
		return changed;
	}

	void TreeAsset::AutoLayout(float nodeWidth, float rowHeight)
	{
		std::unordered_set<uint32_t> placed;
		float nextX = 0.0f;
		// 葉は左から順に置き、親は子の真ん中に置く
		std::function<float(uint32_t, int)> layout = [&](uint32_t id, int depth) -> float {
			NodeData* d = Find(id);
			if (!d || !placed.insert(id).second) { return nextX; }
			std::vector<float> xs;
			for (uint32_t c : d->children)
			{
				if (c && Find(c) && !placed.contains(c)) { xs.push_back(layout(c, depth + 1)); }
			}
			float x;
			if (xs.empty())
			{
				x = nextX;
				nextX += nodeWidth;
			}
			else { x = (xs.front() + xs.back()) * 0.5f; }
			d->position = Vector2(x, float(depth) * rowHeight);
			return x;
			};
		if (const NodeData* root = GetRoot()) { layout(root->id, 0); }
		// つながっていないものは、木の下に並べる
		float maxDepth = 0.0f;
		for (const NodeData& d : nodes)
		{
			if (placed.contains(d.id)) { maxDepth = std::max(maxDepth, d.position.y); }
		}
		float x = 0.0f;
		for (NodeData& d : nodes)
		{
			if (placed.contains(d.id)) { continue; }
			d.position = Vector2(x, maxDepth + rowHeight * 1.5f);
			x += nodeWidth;
		}
	}

	NodePtr TreeAsset::Instantiate(Actor& self, std::unordered_map<uint32_t, Node*>* outNodes, std::string* outWarnings) const
	{
		const NodeData* root = GetRoot();
		if (!root) { return nullptr; }
		Builder builder(*this, self, outNodes, outWarnings);
		return builder.Build(root->id);
	}

	TreeInstance::TreeInstance()
	{
		Instances().push_back(this);
	}

	TreeInstance::~TreeInstance()
	{
		std::erase(Instances(), this);
	}

	bool TreeInstance::Load(const std::string& path, Actor& self)
	{
		mRoot.reset();
		mNodes.clear();
		mPath = path;
		mOwner = &self;
		TreeAsset asset;
		if (!asset.Load(path)) { return false; }
		std::string warnings;
		mRoot = asset.Instantiate(self, &mNodes, &warnings);
		if (!warnings.empty()) { Logger::Warning(LogCat::Game, "Behavior tree {} ({}):\n{}", path, self.GetName(), warnings); }
		return mRoot != nullptr;
	}

	void TreeInstance::Tick(float dt)
	{
		if (!mRoot) { return; }
		++mTicks;
		mRoot->Tick(dt);
	}

	void TreeInstance::Clear()
	{
		mRoot.reset();
		mNodes.clear();
	}

	const Node* TreeInstance::FindNode(uint32_t id) const
	{
		auto it = mNodes.find(id);
		return it != mNodes.end() ? it->second : nullptr;
	}

	const std::vector<TreeInstance*>& TreeInstance::GetAll()
	{
		return Instances();
	}
}
