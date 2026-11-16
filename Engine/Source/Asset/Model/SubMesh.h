#pragma once
#include "Math/Geometry/AABB.h"
#include <string>

namespace rd
{
	// モデルの中の 1 つのメッシュ（インデックスバッファの範囲とマテリアル）
	struct SubMesh
	{
		std::string name;
		uint32_t indexStart = 0;
		uint32_t indexCount = 0;
		uint32_t materialIndex = 0;
		bool skinned = false;
		AABB bounds;
	};
}
