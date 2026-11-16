#pragma once
#include <chrono>

namespace rd
{
	// 経過時間の計測（Tick は前回の Tick からの秒数を返す）
	class Timer
	{
	public:
		using Clock = std::chrono::steady_clock;

		Timer() { Reset(); }

		void Reset()
		{
			mStart = Clock::now();
			mLast = mStart;
		}

		float Tick()
		{
			auto now = Clock::now();
			float dt = std::chrono::duration<float>(now - mLast).count();
			mLast = now;
			return dt;
		}

		double GetTotalSeconds() const
		{
			return std::chrono::duration<double>(Clock::now() - mStart).count();
		}

	private:
		Clock::time_point mStart;
		Clock::time_point mLast;
	};
}
