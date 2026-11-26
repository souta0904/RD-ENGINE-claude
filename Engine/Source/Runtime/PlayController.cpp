#include "RdPch.h"
#include "Runtime/PlayController.h"
#include "Core/Time/Time.h"

namespace rd
{
	void PlayController::Start()
	{
		mPlaying = true;
		mPaused = false;
		Time::sTotal = 0.0;
	}

	void PlayController::Stop()
	{
		mPlaying = false;
		mPaused = false;
	}

	// 一時停止中でも Step を頼まれたら固定時間 1 回分だけ進める
	PlayController::FrameTime PlayController::AdvanceTime(float dt)
	{
		FrameTime t;
		t.advance = mPlaying && (!mPaused || mStepRequested);
		t.gameDt = t.advance ? (mStepRequested ? Time::FixedDeltaTime() : dt) * Time::TimeScale() : 0.0f;
		mStepRequested = false;
		Time::sUnscaledDelta = dt;
		Time::sDelta = t.gameDt;
		if (t.advance) { Time::sTotal += t.gameDt; }
		++Time::sFrameCount;
		return t;
	}
}
