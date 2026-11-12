#include "RdPch.h"
#include "Components/CameraComponent.h"
#include "Render/DebugDraw.h"
#include "Scene/Actor.h"

namespace rd
{
	void CameraComponent::Reflect(PropertyVisitor& v)
	{
		v.Property("FOV", fov, { .speed = 0.1f, .min = 1.0f, .max = 179.0f });
		v.Property("Near", nearZ, { .speed = 0.01f, .min = 0.001f, .max = 100.0f });
		v.Property("Far", farZ, { .speed = 1.0f, .min = 1.0f, .max = 100000.0f });
		v.Property("Priority", priority);
	}

	RenderCamera CameraComponent::GetRenderCamera(float aspect) const
	{
		const Transform& t = GetTransform();
		return RenderCamera::Make(t.GetPosition(), t.GetRotation(), math::ToRadians(fov), aspect, nearZ, farZ);
	}

	void CameraComponent::OnDrawGizmos(DebugDraw& draw, bool selected)
	{
		if (!selected) { return; }
		RenderCamera cam = RenderCamera::Make(GetTransform().GetPosition(), GetTransform().GetRotation(),
			math::ToRadians(fov), 16.0f / 9.0f, nearZ, std::min(farZ, 10.0f));
		draw.Frustum(Inverse(cam.view * cam.proj), Color(1.0f, 1.0f, 0.4f, 1.0f));
	}
}
