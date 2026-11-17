#pragma once
#include "Core/Reflection/PropertyVisitor.h"

namespace rd
{
	// トーンマップの方式（None はトーンマップせずに切り詰めるだけ）
	enum class Tonemapper { ACES, Neutral, None };

	// シーンごとの環境（環境光・空・霧・色の調整・レトロ表示）
	//  色の調整は Unity の Color Adjustments と同じ意味（露出は段数、コントラストと彩度は -100～100）
	//  レトロ表示は縦 retroHeight ピクセルに落として、色を retroColorLevels 段階にディザで減らす（0 なら使わない）
	struct Environment
	{
		Color ambientSky = { 0.35f, 0.42f, 0.55f, 1.0f };
		Color ambientGround = { 0.18f, 0.16f, 0.14f, 1.0f };
		// 地平線の色も使う（Unity の Gradient の環境光）。使わなければ空と地面の 2 色
		bool ambientTrilight = false;
		Color ambientEquator = { 0.3f, 0.3f, 0.3f, 1.0f };
		float ambientIntensity = 1.0f;
		bool drawSky = true;
		Color skyZenith = { 0.18f, 0.36f, 0.75f, 1.0f };
		Color skyHorizon = { 0.65f, 0.75f, 0.9f, 1.0f };
		Color fogColor = { 0.6f, 0.7f, 0.85f, 1.0f };
		float fogDensity = 0.0f;
		float fogStart = 10.0f;
		Tonemapper tonemapper = Tonemapper::ACES;
		float postExposure = 0.0f;
		float contrast = 0.0f;
		float saturation = 0.0f;
		uint32_t retroHeight = 0;
		uint32_t retroColorLevels = 32;

		void Reflect(PropertyVisitor& v);
	};
}
