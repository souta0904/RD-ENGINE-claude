#pragma once
#include "Math/Geometry/Ray.h"
#include "Math/Matrix4.h"

namespace rd
{
	// 描画に使うカメラ（ビュー行列と射影行列）
	struct RD_API RenderCamera
	{
		Matrix4 view;
		Matrix4 proj;
		Vector3 position;
		Quaternion rotation;
		float fovY = math::ToRadians(60.0f);
		float nearZ = 0.1f;
		float farZ = 1000.0f;

		static RenderCamera Make(const Vector3& position, const Quaternion& rotation, float fovY, float aspect, float nearZ, float farZ);
		// uv はスクリーンの 0～1（左上が原点）
		Ray ScreenPointToRay(const Vector2& uv) const;
		Vector3 GetForward() const { return Vector3::kUnitZ * rotation; }
		Vector3 GetRight() const { return Vector3::kUnitX * rotation; }
		Vector3 GetUp() const { return Vector3::kUnitY * rotation; }
	};
}
