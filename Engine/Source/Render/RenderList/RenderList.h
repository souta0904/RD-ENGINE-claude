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
			translucentTriangles.clear();
			outlines.clear();
			dirLights.clear();
			pointLights.clear();
			spotLights.clear();
		}

		std::vector<MeshDrawItem> meshes;
		std::vector<ParticleDrawItem> particles;
		std::vector<SpriteDrawItem> sprites;
		// 3 つずつで 1 枚（視界の扇形など、毎フレーム形が変わるもの向け。裏も描き、深度は書かない）
		std::vector<ColorVertex> translucentTriangles;
		std::vector<OutlineDrawItem> outlines;
		std::vector<GpuDirLight> dirLights;
		std::vector<GpuPointLight> pointLights;
		std::vector<GpuSpotLight> spotLights;

		// 画面の UI（Canvas）を描く大きさ（ピクセル）。0 なら描かない（シーンビューやプレビュー）
		//  集める側が CollectRender の前に設定する（Clear では消さない）
		Vector2 uiScreenSize;
	};
}
