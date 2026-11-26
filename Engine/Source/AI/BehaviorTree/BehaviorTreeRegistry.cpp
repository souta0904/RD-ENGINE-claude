#include "RdPch.h"
#include "AI/BehaviorTree/BehaviorTreeRegistry.h"
#include "Scene/Actor/Actor.h"
#include "Scene/Component/ComponentRegistry.h"

namespace rd::bt
{
	namespace
	{
		// 決めた時間だけ待って成功する
		class WaitNode final : public Node
		{
		public:
			explicit WaitNode(float seconds) : mSeconds(seconds) {}

		protected:
			Status OnStart() override
			{
				mTime = 0.0f;
				return mSeconds <= 0.0f ? Status::Success : Status::Running;
			}
			Status OnUpdate(float dt) override
			{
				mTime += dt;
				return mTime >= mSeconds ? Status::Success : Status::Running;
			}

		private:
			float mSeconds = 0.0f;
			float mTime = 0.0f;
		};

		// すぐに決めた結果を返す（Log は文字をログに出してから成功する）
		class ResultNode final : public Node
		{
		public:
			ResultNode(Status result, std::string message, std::string actor) : mResult(result), mMessage(std::move(message)), mActor(std::move(actor)) {}

		protected:
			Status OnStart() override
			{
				if (!mMessage.empty()) { Logger::Info(LogCat::Game, "[{}] {}", mActor, mMessage); }
				return mResult;
			}

		private:
			Status mResult;
			std::string mMessage;
			std::string mActor;
		};

		ParamDef MakeParam(const char* name, ParamType type, Json value, const char* tooltip = "")
		{
			ParamDef p;
			p.name = name;
			p.type = type;
			p.defaultValue = std::move(value);
			p.tooltip = tooltip;
			return p;
		}

		struct Tables
		{
			std::vector<NodeTypeInfo> types;
			std::vector<ConditionInfo> conditions;
			std::vector<SelectorInfo> selectors;
		};

		void AddBuiltins(Tables& t)
		{
			using namespace builtin;
			auto add = [&t](const char* name, const char* category, NodeKind kind, const char* description,
				std::vector<ParamDef> params = {}, LeafFactory create = nullptr)
				{
					NodeTypeInfo info;
					info.name = name;
					info.category = category;
					info.kind = kind;
					info.description = description;
					info.params = std::move(params);
					info.create = std::move(create);
					t.types.push_back(std::move(info));
				};
			const ParamDef condition = MakeParam("Condition", ParamType::Condition, "", "Name of the condition to check");
			const ParamDef invert = MakeParam("Invert", ParamType::Bool, false, "Use the opposite of the condition");

			add(kRoot, "", NodeKind::Root, "Start of the tree",
				{ MakeParam("Repeat", ParamType::Bool, true, "Start again from the top when the tree finishes") });
			add(kSequence, "Composite", NodeKind::Composite, "Runs children from left to right. Fails as soon as one fails");
			add(kSelector, "Composite", NodeKind::Composite, "Tries children from left to right. Succeeds as soon as one succeeds");
			add(kRepeat, "Decorator", NodeKind::Decorator, "Runs the child again and again (never finishes)");
			add(kInverter, "Decorator", NodeKind::Decorator, "Turns success into failure and failure into success");
			add(kGuard, "Decorator", NodeKind::Decorator, "Runs the child only when the condition holds (checked once when entered)", { condition, invert });
			add(kBranch, "Flow", NodeKind::Branch, "Runs True or False depending on the condition", { condition, invert });
			add(kSwitch, "Flow", NodeKind::Switch, "Runs the child that matches the selector's value",
				{ MakeParam("Value", ParamType::Selector, "", "Name of the value that picks the child") });
			add(kCheck, "Condition", NodeKind::Leaf, "Succeeds when the condition holds, fails otherwise", { condition, invert });
			add(kWait, "Action", NodeKind::Leaf, "Waits and then succeeds",
				{ MakeParam("Seconds", ParamType::Float, 1.0f) },
				[](Actor&, const Json& p) -> NodePtr { return std::make_unique<WaitNode>(GetFloat(p, "Seconds", 1.0f)); });
			add(kLog, "Action", NodeKind::Leaf, "Writes a message to the log and succeeds",
				{ MakeParam("Message", ParamType::String, "") },
				[](Actor& self, const Json& p) -> NodePtr { return std::make_unique<ResultNode>(Status::Success, GetString(p, "Message"), self.GetName()); });
			add(kSucceed, "Action", NodeKind::Leaf, "Succeeds at once", {},
				[](Actor&, const Json&) -> NodePtr { return std::make_unique<ResultNode>(Status::Success, "", ""); });
			add(kFail, "Action", NodeKind::Leaf, "Fails at once", {},
				[](Actor&, const Json&) -> NodePtr { return std::make_unique<ResultNode>(Status::Failure, "", ""); });
		}

		Tables& GetTables()
		{
			static Tables tables = [] {
				Tables t;
				AddBuiltins(t);
				return t;
			}();
			return tables;
		}

		// 同じ名前がもうあれば置き換える（読み直したゲームの DLL のものが勝つ）
		template <class T>
		void AddOrReplace(std::vector<T>& list, T info)
		{
			info.module = ComponentRegistry::GetLoadingModule();
			for (T& e : list)
			{
				if (e.name == info.name)
				{
					e = std::move(info);
					return;
				}
			}
			list.push_back(std::move(info));
		}

		template <class T>
		const T* FindByName(const std::vector<T>& list, const std::string& name)
		{
			for (const T& e : list)
			{
				if (e.name == name) { return &e; }
			}
			return nullptr;
		}

		template <class T>
		std::vector<T> RemoveGame(std::vector<T>& list)
		{
			std::vector<T> removed;
			std::erase_if(list, [&removed](T& e) {
				if (e.module == 0) { return false; }
				removed.push_back(std::move(e));
				return true;
				});
			return removed;
		}
	}

	void Registry::RegisterAction(NodeTypeInfo info)
	{
		info.kind = NodeKind::Leaf;
		if (info.category.empty()) { info.category = "Action"; }
		AddOrReplace(GetTables().types, std::move(info));
	}

	void Registry::RegisterCondition(ConditionInfo info) { AddOrReplace(GetTables().conditions, std::move(info)); }
	void Registry::RegisterSelector(SelectorInfo info) { AddOrReplace(GetTables().selectors, std::move(info)); }

	const NodeTypeInfo* Registry::FindType(const std::string& name) { return FindByName(GetTables().types, name); }
	const ConditionInfo* Registry::FindCondition(const std::string& name) { return FindByName(GetTables().conditions, name); }
	const SelectorInfo* Registry::FindSelector(const std::string& name) { return FindByName(GetTables().selectors, name); }
	const std::vector<NodeTypeInfo>& Registry::GetTypes() { return GetTables().types; }
	const std::vector<ConditionInfo>& Registry::GetConditions() { return GetTables().conditions; }
	const std::vector<SelectorInfo>& Registry::GetSelectors() { return GetTables().selectors; }

	Registry::Saved Registry::RemoveGameEntries()
	{
		Tables& t = GetTables();
		Saved saved;
		saved.types = RemoveGame(t.types);
		saved.conditions = RemoveGame(t.conditions);
		saved.selectors = RemoveGame(t.selectors);
		return saved;
	}

	void Registry::RestoreEntries(Saved saved)
	{
		Tables& t = GetTables();
		for (auto& e : saved.types) { t.types.push_back(std::move(e)); }
		for (auto& e : saved.conditions) { t.conditions.push_back(std::move(e)); }
		for (auto& e : saved.selectors) { t.selectors.push_back(std::move(e)); }
	}
}
