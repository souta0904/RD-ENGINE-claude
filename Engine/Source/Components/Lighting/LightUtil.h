#pragma once
#include "Math/Color.h"
#include "Math/Vector3.h"

// ライトのコンポーネントで共通に使う小さな関数と定数
namespace rd::light
{
	inline Vector3 Rgb(const Color& c) { return { c.r, c.g, c.b }; }
	inline constexpr Color kGizmoColor{ 1.0f, 0.9f, 0.3f, 0.9f };
}
