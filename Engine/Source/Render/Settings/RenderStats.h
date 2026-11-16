#pragma once
#include <cstdint>

namespace rd
{
	// 1 フレームの描画の統計
	struct RenderStats
	{
		uint32_t drawCalls = 0;
		uint32_t triangles = 0;
		uint32_t meshesVisible = 0;
		uint32_t meshesCulled = 0;
		uint32_t shadowCasters = 0;
		uint32_t particles = 0;
		uint32_t sprites = 0;
		uint32_t lights = 0;
	};
}
