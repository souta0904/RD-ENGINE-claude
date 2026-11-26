#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace rd
{
	// 小さな行動ツリー（Unity Behavior の基本のノードと同じ動き）
	//  ノードは Tick のたびに Running / Success / Failure を返す。始まるときに OnStart、
	//  終わるときに必ず OnEnd が呼ばれる（OnStart がすぐ終わりを返したときも呼ばれる）。
	//  条件は入るときに 1 回だけ調べる（途中で割り込まない）。割り込みたいときはアクションの側で
	//  条件を見て Failure を返す。
	namespace bt
	{
		enum class Status { Running, Success, Failure };

		class Node
		{
		public:
			virtual ~Node() = default;

			Status Tick(float dt)
			{
				++mTickCount;
				if (!mRunning)
				{
					mRunning = true;
					Status s = OnStart();
					if (s != Status::Running) { return Finish(s); }
					mLastStatus = Status::Running;
					return Status::Running;
				}
				Status s = OnUpdate(dt);
				if (s == Status::Running)
				{
					mLastStatus = s;
					return s;
				}
				return Finish(s);
			}

			// 途中でやめる（動いていれば OnEnd を呼ぶ）
			void Abort()
			{
				if (!mRunning) { return; }
				OnAbort();
				Finish(Status::Failure);
			}

			bool IsRunning() const { return mRunning; }
			// 最後に返した結果と、Tick された回数（エディタで動いている様子を見せるのに使う）
			Status GetLastStatus() const { return mLastStatus; }
			uint64_t GetTickCount() const { return mTickCount; }
			std::string name;
			// アセットから作ったときの、アセットの中のノードの番号（0 ならアセットから作っていない）
			uint32_t id = 0;

		protected:
			virtual Status OnStart() { return Status::Running; }
			virtual Status OnUpdate(float /*dt*/) { return Status::Success; }
			virtual void OnEnd() {}
			// 子を持つノードが、動いている子を止めるために使う
			virtual void OnAbort() {}

		private:
			Status Finish(Status s)
			{
				mRunning = false;
				mLastStatus = s;
				OnEnd();
				return s;
			}

			bool mRunning = false;
			Status mLastStatus = Status::Failure;
			uint64_t mTickCount = 0;
		};

		using NodePtr = std::unique_ptr<Node>;
		using Condition = std::function<bool()>;

		// 子を順に動かす。どれかが失敗したら失敗、全部成功したら成功
		class Sequence final : public Node
		{
		public:
			std::vector<NodePtr> children;

		protected:
			Status OnStart() override { mIndex = 0; return Run(0.0f); }
			Status OnUpdate(float dt) override { return Run(dt); }
			void OnAbort() override { if (mIndex < children.size()) { children[mIndex]->Abort(); } }

		private:
			Status Run(float dt)
			{
				while (mIndex < children.size())
				{
					Status s = children[mIndex]->Tick(dt);
					if (s == Status::Running) { return s; }
					if (s == Status::Failure) { return s; }
					++mIndex;
				}
				return Status::Success;
			}
			size_t mIndex = 0;
		};

		// 子を順に試す（Unity の Try In Order）。どれかが成功したら成功、全部失敗したら失敗
		class Selector final : public Node
		{
		public:
			std::vector<NodePtr> children;

		protected:
			Status OnStart() override { mIndex = 0; return Run(0.0f); }
			Status OnUpdate(float dt) override { return Run(dt); }
			void OnAbort() override { if (mIndex < children.size()) { children[mIndex]->Abort(); } }

		private:
			Status Run(float dt)
			{
				while (mIndex < children.size())
				{
					Status s = children[mIndex]->Tick(dt);
					if (s == Status::Running) { return s; }
					if (s == Status::Success) { return s; }
					++mIndex;
				}
				return Status::Failure;
			}
			size_t mIndex = 0;
		};

		// 条件が成り立つときだけ子を動かす（Unity の Conditional Guard）。成り立たなければ失敗
		class Guard final : public Node
		{
		public:
			Condition condition;
			NodePtr child;

		protected:
			Status OnStart() override
			{
				if (!condition || !condition()) { return Status::Failure; }
				return child->Tick(0.0f);
			}
			Status OnUpdate(float dt) override { return child->Tick(dt); }
			void OnAbort() override { child->Abort(); }
		};

		// 条件で 2 つの子のどちらかを動かす（Unity の Conditional Branch）
		class Branch final : public Node
		{
		public:
			Condition condition;
			NodePtr onTrue;
			NodePtr onFalse;

		protected:
			Status OnStart() override
			{
				mActive = (condition && condition()) ? onTrue.get() : onFalse.get();
				if (!mActive) { return Status::Failure; }
				return mActive->Tick(0.0f);
			}
			Status OnUpdate(float dt) override { return mActive->Tick(dt); }
			void OnAbort() override { if (mActive) { mActive->Abort(); } }

		private:
			Node* mActive = nullptr;
		};

		// 値で子を選ぶ（Unity の Switch）。範囲の外なら失敗
		class Switch final : public Node
		{
		public:
			std::function<int()> selector;
			std::vector<NodePtr> cases;

		protected:
			Status OnStart() override
			{
				int index = selector ? selector() : -1;
				mActive = (index >= 0 && size_t(index) < cases.size()) ? cases[index].get() : nullptr;
				if (!mActive) { return Status::Failure; }
				return mActive->Tick(0.0f);
			}
			Status OnUpdate(float dt) override { return mActive->Tick(dt); }
			void OnAbort() override { if (mActive) { mActive->Abort(); } }

		private:
			Node* mActive = nullptr;
		};

		// 子を何度でも動かす（終わったら次の Tick で最初から）
		class Repeat final : public Node
		{
		public:
			NodePtr child;

		protected:
			Status OnStart() override { child->Tick(0.0f); return Status::Running; }
			Status OnUpdate(float dt) override { child->Tick(dt); return Status::Running; }
			void OnAbort() override { child->Abort(); }
		};

		// 関数で書く手軽なアクション
		class Action final : public Node
		{
		public:
			std::function<Status()> start;
			std::function<Status(float)> update;
			std::function<void()> end;

		protected:
			Status OnStart() override { return start ? start() : Status::Running; }
			Status OnUpdate(float dt) override { return update ? update(dt) : Status::Success; }
			void OnEnd() override { if (end) { end(); } }
		};

		// ツリーを組み立てる関数
		template <class... Nodes>
		NodePtr MakeSequence(Nodes&&... nodes)
		{
			auto n = std::make_unique<Sequence>();
			(n->children.push_back(std::forward<Nodes>(nodes)), ...);
			return n;
		}

		template <class... Nodes>
		NodePtr MakeSelector(Nodes&&... nodes)
		{
			auto n = std::make_unique<Selector>();
			(n->children.push_back(std::forward<Nodes>(nodes)), ...);
			return n;
		}

		inline NodePtr MakeGuard(Condition condition, NodePtr child)
		{
			auto n = std::make_unique<Guard>();
			n->condition = std::move(condition);
			n->child = std::move(child);
			return n;
		}

		inline NodePtr MakeBranch(Condition condition, NodePtr onTrue, NodePtr onFalse)
		{
			auto n = std::make_unique<Branch>();
			n->condition = std::move(condition);
			n->onTrue = std::move(onTrue);
			n->onFalse = std::move(onFalse);
			return n;
		}

		inline NodePtr MakeRepeat(NodePtr child)
		{
			auto n = std::make_unique<Repeat>();
			n->child = std::move(child);
			return n;
		}
	}
}
