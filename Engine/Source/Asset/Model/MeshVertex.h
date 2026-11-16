#pragma once
#include "Math/Vector4.h"
#include <cstdint>

namespace rd
{
	// メッシュの頂点（tangent.w は従法線の向き）
	struct MeshVertex
	{
		Vector3 position;
		Vector3 normal;
		Vector4 tangent;
		Vector2 uv;
	};

	// スキニング用の頂点データ（最大 4 本の骨）
	struct SkinVertex
	{
		uint32_t joints[4] = {};
		float weights[4] = {};
	};
}
