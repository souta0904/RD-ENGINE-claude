#include "RdPch.h"
#include "Components/AI/BehaviorTreeAgent.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	void BehaviorTreeAgent::Reflect(PropertyVisitor& v)
	{
		v.Property("Tree", tree, { .flags = PropertyMeta::AssetBehaviorTree, .tooltip = "Behavior tree asset (.rdbt) to run" });
	}

	void BehaviorTreeAgent::Start()
	{
		if (tree.empty()) { return; }
		if (!mInstance.Load(tree, *GetActor())) { Logger::Warning(LogCat::Game, "BehaviorTreeAgent: failed to load {}", tree); }
	}

	void BehaviorTreeAgent::Update(float dt)
	{
		mInstance.Tick(dt);
	}

	void BehaviorTreeAgent::OnDestroy()
	{
		// 同じアクターのほかのコンポーネントも一緒に消えるので、アクションの後始末は呼ばずに捨てる
		mInstance.Clear();
	}
}
