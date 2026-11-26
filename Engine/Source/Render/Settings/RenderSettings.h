#pragma once
#include "Core/Reflection/PropertyVisitor.h"

namespace rd
{
	// 不透明な物の描き方
	//  Forward: 物を描くときにその場で光を当てる（物ごとのライトの数の制限 Lights Per Object が効く）
	//  Deferred: 先に G バッファーへ色・法線・質感を書き、画面全体で 1 回だけ光を当てる（ライトが多くても速い）
	//  半透明とパーティクルはどちらでもフォワードで描く
	enum class RenderPath : uint8_t
	{
		Forward,
		Deferred,
	};

	// 描画の設定（エンジン全体）
	//  shadowMapSize はカスケード 1 つ分（テクスチャはこの 2 倍四方）
	//  cascadeSplitLambda は 0 で等間隔、1 で対数（カメラの近くを細かくする）
	struct RD_API RenderSettings
	{
		RenderPath path = RenderPath::Forward;
		bool shadows = true;
		uint32_t shadowMapSize = 1024;
		uint32_t shadowCascades = 4;
		float shadowDistance = 80.0f;
		float cascadeSplitLambda = 0.75f;
		float shadowBias = 0.0005f;
		float shadowStrength = 0.9f;
		bool debugCascades = false;
		bool bloom = true;
		float bloomThreshold = 1.0f;
		float bloomKnee = 0.5f;
		float bloomIntensity = 0.5f;
		float exposure = 1.0f;
		bool tonemap = true;
		float vignette = 0.25f;
		bool frustumCulling = true;
		bool wireframe = false;

		void Reflect(PropertyVisitor& v);
	};
}
