#pragma once
#include <chrono>

namespace rd
{
	// 経過時間の計測
	//  エンジンはこれで毎フレームの DeltaTime を測る（Tick が前回の Tick からの秒数を返す）。
	//  処理にかかった時間を手軽に測るときにも使える
	class Timer
	{
	public:
		// steady_clock は「戻らない時計」。PC の時刻合わせで時計が巻き戻っても、測った時間が負にならない
		// （時刻を知りたいなら system_clock だが、時間の長さを測るなら steady_clock を使う）
		using Clock = std::chrono::steady_clock;

		Timer() { Reset(); }

		// 計測をやり直す（合計も Tick の基準も今にする）
		void Reset()
		{
			mStart = Clock::now();
			mLast = mStart;
		}

		// 前回の Tick（初回は Reset）からの秒数を返し、基準を今に進める
		// duration<float> は「秒を float で表す長さ」。count() でその数値を取り出す
		float Tick()
		{
			auto now = Clock::now();
			float dt = std::chrono::duration<float>(now - mLast).count();
			mLast = now;
			return dt;
		}

		// Reset からの合計の秒数（長い時間でも精度が落ちないよう double）
		double GetTotalSeconds() const
		{
			return std::chrono::duration<double>(Clock::now() - mStart).count();
		}

	private:
		Clock::time_point mStart;
		Clock::time_point mLast;
	};
}
