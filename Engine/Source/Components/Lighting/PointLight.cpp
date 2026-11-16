#include "RdPch.h"
#include "Components/Lighting/PointLight.h"
#include "Components/Lighting/LightUtil.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/RenderList/RenderList.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	void PointLight::Reflect(PropertyVisitor& v)
	{
		v.Property("Color", color);
		v.Property("Intensity", intensity, { .speed = 0.1f, .min = 0.0f, .max = 100000.0f });
		v.Property("Range", range, { .speed = 0.05f, .min = 0.01f, .max = 1000.0f });
	}

	void PointLight::OnRender(RenderList& list)
	{
		GpuPointLight l;
		l.position = GetTransform().GetPosition();
		l.color = light::Rgb(color);
		l.intensity = intensity;
		l.range = range;
		list.pointLights.push_back(l);
	}

#if RD_EDITOR
	void PointLight::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		draw.Sphere(GetTransform().GetPosition(), selected ? range : 0.2f, light::kGizmoColor, true, selected ? 32 : 12);
	}
#endif
}
