#include "RdPch.h"
#include "Components/Lights.h"
#include "Render/DebugDraw.h"
#include "Render/RenderTypes.h"
#include "Scene/Actor.h"

namespace rd
{
	namespace
	{
		Vector3 Rgb(const Color& c) { return { c.r, c.g, c.b }; }
#if RD_EDITOR
		const Color kGizmoColor(1.0f, 0.9f, 0.3f, 0.9f);
#endif
	}

	// ==================================================
	// DirectionalLight
	// ==================================================
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
		l.color = Rgb(color);
		l.intensity = intensity;
		l.castShadow = castShadows ? 1u : 0u;
		list.dirLights.push_back(l);
	}

#if RD_EDITOR
	void DirectionalLight::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		Vector3 p = GetTransform().GetPosition();
		draw.Arrow(p, p + GetTransform().GetForward() * (selected ? 3.0f : 1.5f), kGizmoColor);
	}
#endif

	// ==================================================
	// PointLight
	// ==================================================
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
		l.color = Rgb(color);
		l.intensity = intensity;
		l.range = range;
		list.pointLights.push_back(l);
	}

#if RD_EDITOR
	void PointLight::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		draw.Sphere(GetTransform().GetPosition(), selected ? range : 0.2f, kGizmoColor, true, selected ? 32 : 12);
	}
#endif

	// ==================================================
	// SpotLight
	// ==================================================
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
		l.color = Rgb(color);
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
			math::ToRadians(outerAngle), kGizmoColor);
	}
#endif
}
