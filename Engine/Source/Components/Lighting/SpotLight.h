#pragma once
#include "Scene/Component/Component.h"

namespace rd
{
	// スポットライト（Transform の前方向を照らす。角度は度）
	class SpotLight final : public Component
	{
		RD_COMPONENT(SpotLight)
	public:
		Color color = Color::kWhite;
		float intensity = 40.0f;
		float range = 15.0f;
		float innerAngle = 20.0f;
		float outerAngle = 30.0f;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
#if RD_EDITOR
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
#endif
	};
}
