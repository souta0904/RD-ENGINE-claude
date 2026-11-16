#pragma once
#include "Core/Reflection/PropertyVisitor.h"

namespace rd
{
	// シーンごとの環境（環境光・空・霧）
	struct Environment
	{
		Color ambientSky = { 0.35f, 0.42f, 0.55f, 1.0f };
		Color ambientGround = { 0.18f, 0.16f, 0.14f, 1.0f };
		float ambientIntensity = 1.0f;
		bool drawSky = true;
		Color skyZenith = { 0.18f, 0.36f, 0.75f, 1.0f };
		Color skyHorizon = { 0.65f, 0.75f, 0.9f, 1.0f };
		Color fogColor = { 0.6f, 0.7f, 0.85f, 1.0f };
		float fogDensity = 0.0f;
		float fogStart = 10.0f;

		void Reflect(PropertyVisitor& v);
	};
}
