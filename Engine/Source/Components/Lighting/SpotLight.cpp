#include "RdPch.h"
#include "Components/Lighting/SpotLight.h"
#include "Components/Lighting/LightUtil.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/RenderList/RenderList.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	void SpotLight::Reflect(PropertyVisitor& v)
	{
		v.Property("Color", color);
		v.Property("Intensity", intensity, { .speed = 0.1f, .min = 0.0f, .max = 100000.0f });
		v.Property("Range", range, { .speed = 0.05f, .min = 0.01f, .max = 1000.0f });
		v.Property("Inner Angle", innerAngle, { .speed = 0.1f, .min = 0.0f, .max = 89.0f });
		v.Property("Outer Angle", outerAngle, { .speed = 0.1f, .min = 0.0f, .max = 89.0f });
	}

	void SpotLight::OnRender(RenderList& list)
	{
		GpuSpotLight l;
		l.position = GetTransform().GetPosition();
		l.direction = GetTransform().GetForward();
		l.color = light::Rgb(color);
		l.intensity = intensity;
		l.range = range;
		float outer = std::max(outerAngle, innerAngle + 0.1f);
		l.cosInner = std::cos(math::ToRadians(innerAngle));
		l.cosOuter = std::cos(math::ToRadians(outer));
		list.spotLights.push_back(l);
	}

#if RD_EDITOR
	void SpotLight::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		draw.Cone(GetTransform().GetPosition(), GetTransform().GetForward(), selected ? range : 1.0f,
			math::ToRadians(outerAngle), light::kGizmoColor);
	}
#endif
}
