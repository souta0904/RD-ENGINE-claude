#pragma once
#include "Math/Color.h"
#include "Math/Vector.h"
#include <random>

namespace rd
{
	// 乱数（スレッドごとに独立したエンジンを持つのでジョブからも安全に呼べる）
	class Random
	{
	public:
		static void Seed(uint32_t seed) { Engine().seed(seed); }

		static int Range(int min, int max)// [min, max]
		{
			return std::uniform_int_distribution<int>(min, max)(Engine());
		}
		static float Range(float min, float max)// [min, max)
		{
			return min + (max - min) * Value();
		}
		static float Value()// [0, 1)
		{
			return std::uniform_real_distribution<float>(0.0f, 1.0f)(Engine());
		}
		static Vector3 Range(const Vector3& min, const Vector3& max)
		{
			return { Range(min.x, max.x), Range(min.y, max.y), Range(min.z, max.z) };
		}
		static Color Range(const Color& min, const Color& max)
		{
			return { Range(min.r, max.r), Range(min.g, max.g), Range(min.b, max.b), Range(min.a, max.a) };
		}
		// 単位球内の一様乱数
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
