#include "RdPch.h"
#include "Render/Camera/RenderCamera.h"

namespace rd
{
	RenderCamera RenderCamera::Make(const Vector3& position, const Quaternion& rotation, float fovY, float aspect, float nearZ, float farZ)
	{
		RenderCamera c;
		c.position = position;
		c.rotation = rotation;
		c.fovY = fovY;
		c.nearZ = nearZ;
		c.farZ = farZ;
		c.view = Inverse(Matrix4::CreateRotate(rotation) * Matrix4::CreateTranslate(position));
		c.proj = Matrix4::CreatePerspectiveFov(fovY, aspect, nearZ, farZ);
		return c;
	}

	// 画面上の点を深度 0 と 1 でワールドへ戻し、その 2 点を結ぶ
	Ray RenderCamera::ScreenPointToRay(const Vector2& uv) const
	{
		Matrix4 invVP = Inverse(view * proj);
		Vector2 ndc(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f);
		Vector3 nearP = TransformCoord(Vector3(ndc.x, ndc.y, 0.0f), invVP);
		Vector3 farP = TransformCoord(Vector3(ndc.x, ndc.y, 1.0f), invVP);
		return { nearP, Normalize(farP - nearP) };
	}
}
