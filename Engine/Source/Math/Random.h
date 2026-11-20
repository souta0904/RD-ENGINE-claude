#pragma once
#include "Math/Color.h"
#include "Math/Vector3.h"
#include <random>

namespace rd
{
	// 乱数（スレッドごとに別のエンジンを持つので、ジョブからも呼べる）
	// Unity の Random と同じように、どこからでも Random::Range(1, 6) のように呼べる
	class RD_API Random
	{
	public:
		// 種を決めると、そのスレッドで同じ順番の乱数が出る（再現したいテストなどで使う）
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
		// 半径 1 の球の中のランダムな点
		// 立方体の中の点を作り、球の外なら作り直す（棄却法）。そのまま正規化すると角の方向に偏るため
		// 当たる確率は約 52% なので、平均 2 回ほどで終わる
		static Vector3 InsideUnitSphere()
		{
			while (true)
			{
				Vector3 v(Range(-1.0f, 1.0f), Range(-1.0f, 1.0f), Range(-1.0f, 1.0f));
				if (LengthSq(v) <= 1.0f) { return v; }
			}
		}
		// 半径 1 の球の表面のランダムな点（ランダムな向き）
		// 小さな値を足しているのは、たまたま原点が出たときに正規化で 0 にならないようにするため
		static Vector3 OnUnitSphere() { return Normalize(InsideUnitSphere() + Vector3(1e-6f, 0.0f, 0.0f)); }

	private:
		// 乱数を作るエンジン（メルセンヌ・ツイスタ）。thread_local なのでスレッドごとに 1 つずつ作られ、
		// ロックなしで複数のスレッドから同時に呼べる。最初の種は random_device（OS の乱数）で決める
		static std::mt19937& Engine()
		{
			thread_local std::mt19937 engine(std::random_device{}());
			return engine;
		}
	};
}
