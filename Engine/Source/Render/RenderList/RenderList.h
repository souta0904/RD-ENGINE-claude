#pragma once
#include "Render/GpuData.h"
#include "Render/RenderList/DrawItems.h"
#include <vector>

namespace rd
{
	// 描画の一覧（コンポーネントが毎フレーム積み、Renderer が描く）
	class RenderList
	{
	public:
		void Clear()
		{
			meshes.clear();
			particles.clear();
			sprites.clear();
			dirLights.clear();
			pointLights.clear();
			spotLights.clear();
		}

		std::vector<MeshDrawItem> meshes;
		std::vector<ParticleDrawItem> particles;
		std::vector<SpriteDrawItem> sprites;
		std::vector<GpuDirLight> dirLights;
		std::vector<GpuPointLight> pointLights;
		std::vector<GpuSpotLight> spotLights;
	};
}
