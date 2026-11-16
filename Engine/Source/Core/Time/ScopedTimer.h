#pragma once
#include "Core/Time/Timer.h"

namespace rd
{
	// スコープを抜けたときに、かかった時間（ミリ秒）を書き込む
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
