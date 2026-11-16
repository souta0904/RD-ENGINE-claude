#include "RdPch.h"
#include "Components/Lighting/DirectionalLight.h"
#include "Components/Lighting/LightUtil.h"
#include "Render/Debug/DebugDraw.h"
#include "Render/RenderList/RenderList.h"
#include "Scene/Actor/Actor.h"

namespace rd
{
	void DirectionalLight::Reflect(PropertyVisitor& v)
	{
		v.Property("Color", color);
		v.Property("Intensity", intensity, { .speed = 0.05f, .min = 0.0f, .max = 1000.0f });
		v.Property("Cast Shadows", castShadows);
	}

	void DirectionalLight::OnRender(RenderList& list)
	{
		GpuDirLight l;
		l.direction = GetTransform().GetForward();
		l.color = light::Rgb(color);
		l.intensity = intensity;
		l.castShadow = castShadows ? 1u : 0u;
		list.dirLights.push_back(l);
	}

#if RD_EDITOR
	void DirectionalLight::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		Vector3 p = GetTransform().GetPosition();
		draw.Arrow(p, p + GetTransform().GetForward() * (selected ? 3.0f : 1.5f), light::kGizmoColor);
	}
#endif
}
