#pragma once
#include "Core/Time/Timer.h"

namespace rd
{
	// スコープを抜けたときに、かかった時間（ミリ秒）を書き込む
	//
	//   float ms;
	//   {
	//       ScopedTimer timer(ms);
	//       HeavyWork();
	//   } // ここで ms に HeavyWork の時間が入る
	//
	//  コンストラクタで時刻を覚え、デストラクタで差を書き込む（RAII: 作った時と壊れる時に仕事をさせる書き方）。
	//  途中で return しても、例外で抜けても、必ずデストラクタが呼ばれるので書き忘れがない
	class ScopedTimer
	{
	public:
		explicit ScopedTimer(float& outMs) : mOut(outMs), mStart(Timer::Clock::now()) {}
		// duration<float, std::milli> は「ミリ秒を float で表す長さ」
		~ScopedTimer()
		{
			mOut = std::chrono::duration<float, std::milli>(Timer::Clock::now() - mStart).count();
		}

	private:
		// 書き込み先への参照（ScopedTimer より書き込み先の変数が長生きしていること）
		float& mOut;
		Timer::Clock::time_point mStart;
	};
}
