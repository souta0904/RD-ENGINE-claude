#pragma once
#include "Scene/Component/Component.h"

namespace rd
{
	// 点光源（range までで弱まって消える）
	class RD_API PointLight final : public Component
	{
		RD_COMPONENT(PointLight)
	public:
		Color color = Color::kWhite;
		float intensity = 20.0f;
		float range = 10.0f;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif
	};
}
