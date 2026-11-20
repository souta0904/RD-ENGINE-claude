#pragma once
#include "Scene/Component/Component.h"

namespace rd
{
	// 平行光源（Transform の前方向 +Z に光が進む。影を落とせるのは 1 つだけ）
	class RD_API DirectionalLight final : public Component
	{
		RD_COMPONENT(DirectionalLight)
	public:
		Color color = Color::kWhite;
		float intensity = 3.0f;
		bool castShadows = true;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif
	};
}
