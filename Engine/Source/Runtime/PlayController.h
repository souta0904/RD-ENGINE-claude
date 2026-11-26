#pragma once

namespace rd
{
	// プレイの状態（Play / Pause / Step）とゲーム時間の進め方
	//  エディタのない構成では起動からずっとプレイ中。エディタではエディタの PlayMode が Start / Stop を呼ぶ
	//  （プレイ前のシーンを取っておいて戻すのはエディタの仕事）
	class RD_API PlayController
	{
	public:
		// advance: ゲームを進めるか、gameDt: TimeScale を掛けた経過時間（進めないときは 0）
		struct FrameTime
		{
			bool advance = false;
			float gameDt = 0.0f;
		};

		bool IsPlaying() const { return mPlaying; }
		bool IsPaused() const { return mPaused; }
		void SetPaused(bool paused) { mPaused = paused; }
		void StepFrame() { mStepRequested = true; }

		// プレイを始める（一時停止を解き、ゲームの時間を 0 に戻す）
		void Start();
		void Stop();

		FrameTime AdvanceTime(float dt);

	private:
		bool mPlaying = false;
		bool mPaused = false;
		bool mStepRequested = false;
	};
}
