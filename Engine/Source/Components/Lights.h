#pragma once
#include "Scene/Component.h"
#include "Math/Color.h"

namespace rd
{
	// 平行光源（Transform の前方向 +Z に光が進む）
	class DirectionalLight final : public Component
	{
		RD_COMPONENT(DirectionalLight)
	public:
		Color color = Color::kWhite;
		float intensity = 3.0f;
		bool castShadows = true;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
	};

	// 点光源
	class PointLight final : public Component
	{
		RD_COMPONENT(PointLight)
	public:
		Color color = Color::kWhite;
		float intensity = 20.0f;
		float range = 10.0f;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
	};

	// スポットライト（Transform の前方向を照らす）
	class SpotLight final : public Component
	{
		RD_COMPONENT(SpotLight)
	public:
		Color color = Color::kWhite;
		float intensity = 40.0f;
		float range = 15.0f;
		float innerAngle = 20.0f;// 度
		float outerAngle = 30.0f;

		void Reflect(PropertyVisitor& v) override;
		void OnRender(RenderList& list) override;
		void OnDrawGizmos(DebugDraw& draw, bool selected) override;
	};
}
