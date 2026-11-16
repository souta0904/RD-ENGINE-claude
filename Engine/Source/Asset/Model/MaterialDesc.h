#pragma once
#include "Math/Color.h"
#include <string>

namespace rd
{
	// モデルの中のマテリアルの初期値
	struct MaterialDesc
	{
		std::string name;
		Color baseColor = Color::kWhite;
		float metallic = 0.0f;
		float roughness = 0.6f;
		Color emissive = Color::kBlack;
		std::string albedoPath;
		std::string normalPath;
		bool transparent = false;
	};
}
