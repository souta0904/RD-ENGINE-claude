#pragma once
#include "Scene/Component/Component.h"
#include <string>

namespace rd
{
	// ナビメッシュの形を読み込む（Unity の NavMeshSurface。焼いた結果は data の JSON から読む）
	//  エディタでの表示は、シーンビューの「NavMesh」が オン のときか、このコンポーネントを持つ
	//  アクターを選んでいるときに、エディタがまとめて描く（Editor::DrawSceneGizmos）
	class RD_API NavMeshSurface final : public Component
	{
		RD_COMPONENT(NavMeshSurface)
	public:
		std::string data;

		void Reflect(PropertyVisitor& v) override;
		void Awake() override;
	};

	// 離れた場所どうしを跳んで渡る道（Unity の NavMeshLink。両端はこのアクターから見た位置）
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
		void Register();
	};
}
