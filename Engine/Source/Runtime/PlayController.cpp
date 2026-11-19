#include "RdPch.h"
#include "Runtime/PlayController.h"
#include "Core/Time/Time.h"
#include "Scene/Scene.h"

namespace rd
{
#if RD_EDITOR
	bool PlayController::Begin(const Scene& scene)
	{
		if (mPlaying) { return false; }
		mSnapshot = scene.Serialize().dump();
		mSnapshotPath = scene.GetPath();
		mPlaying = true;
		mPaused = false;
		Time::sTotal = 0.0;
		return true;
	}

	// プレイ中にゲームが別の場面へ切り替えていても、プレイを始めた場面（パスも）に戻す
	std::unique_ptr<Scene> PlayController::End()
	{
		if (!mPlaying) { return nullptr; }
		mPlaying = false;
		mPaused = false;
		auto scene = std::make_unique<Scene>();
		scene->Deserialize(Json::parse(mSnapshot));
		scene->SetPath(mSnapshotPath);
		return scene;
	}
#endif

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
