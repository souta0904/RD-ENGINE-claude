#pragma once
#include "Math/Color.h"
#include "Math/Vector3.h"
#include <random>

namespace rd
{
	// 乱数（スレッドごとに別のエンジンを持つので、ジョブからも呼べる）
	class Random
	{
	public:
		static void Seed(uint32_t seed) { Engine().seed(seed); }

		// [min, max]
		static int Range(int min, int max) { return std::uniform_int_distribution<int>(min, max)(Engine()); }
		// [min, max)
		static float Range(float min, float max) { return min + (max - min) * Value(); }
		// [0, 1)
		static float Value() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(Engine()); }
		static Vector3 Range(const Vector3& min, const Vector3& max)
		{
			return { Range(min.x, max.x), Range(min.y, max.y), Range(min.z, max.z) };
		}
		static Color Range(const Color& min, const Color& max)
		{
			return { Range(min.r, max.r), Range(min.g, max.g), Range(min.b, max.b), Range(min.a, max.a) };
		}
		static Vector3 InsideUnitSphere()
		{
			while (true)
			{
				Vector3 v(Range(-1.0f, 1.0f), Range(-1.0f, 1.0f), Range(-1.0f, 1.0f));
				if (LengthSq(v) <= 1.0f) { return v; }
			}
		}
		static Vector3 OnUnitSphere() { return Normalize(InsideUnitSphere() + Vector3(1e-6f, 0.0f, 0.0f)); }

	private:
		static std::mt19937& Engine()
		{
			thread_local std::mt19937 engine(std::random_device{}());
			return engine;
		}
	};
}
