#pragma once
#include "Math/Matrix4.h"

namespace rd
{
	// 平行移動・回転・拡縮に分けた姿勢
	struct TransformTRS
	{
		Vector3 translation;
		Quaternion rotation;
		Vector3 scale = Vector3::kOne;

		Matrix4 ToMatrix() const { return Matrix4::CreateAffine(scale, rotation, translation); }
		static TransformTRS Blend(const TransformTRS& a, const TransformTRS& b, float t)
		{
			return { math::Lerp(a.translation, b.translation, t), Slerp(a.rotation, b.rotation, t), math::Lerp(a.scale, b.scale, t) };
		}
	};
}
