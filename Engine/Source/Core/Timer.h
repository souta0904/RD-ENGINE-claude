#pragma once
#include <chrono>

namespace rd
{
	// フレーム時間の計測
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

		// 前回の Tick からの経過秒数を返す
		float Tick()
		{
			auto now = Clock::now();
			float dt = std::chrono::duration<float>(now - mLast).count();
			mLast = now;
			return dt;
		}

		// 起動からの経過秒数
		double GetTotalSeconds() const
		{
			return std::chrono::duration<double>(Clock::now() - mStart).count();
		}

	private:
		Clock::time_point mStart;
		Clock::time_point mLast;
	};

	// スコープ内の処理時間を計測（ミリ秒）
	class ScopedTimer
	{
	public:
		explicit ScopedTimer(float& outMs) : mOut(outMs), mStart(Timer::Clock::now()) {}
		~ScopedTimer()
		{
			mOut = std::chrono::duration<float, std::milli>(Timer::Clock::now() - mStart).count();
		}
	private:
		float& mOut;
		Timer::Clock::time_point mStart;
	};
}
