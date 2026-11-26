#pragma once
#include "Math/Vector3.h"
#include "Scene/Component/Component.h"

namespace rd
{
	// 離れた場所どうしを跳んで渡る道（Unity の NavMeshLink。両端はこのアクターから見た位置）
	//  Start でシーンの NavigationSystem にリンクを登録し、OnDestroy で外す。
	//  エージェントはリンクの入口で止まり、NavMeshAgent::CompleteOffMeshLink で続きを歩く。
	class RD_API NavMeshLink final : public Component
	{
		RD_COMPONENT(NavMeshLink)
	public:
		Vector3 startPoint;
		Vector3 endPoint = Vector3(0.0f, 0.0f, 2.5f);
		float width = 0.0f;
		float costModifier = 1.0f;
		bool isOverridingCost = false;
		bool bidirectional = true;
		bool activated = true;
		int32_t area = 2;

		void Reflect(PropertyVisitor& v) override;
		void Start() override;
		void OnDestroy() override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif

	private:
		// 今の値でリンクを登録し直す（無効なら外す）
		void Register();
	};
}
