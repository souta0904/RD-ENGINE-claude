#include "RdPch.h"
#include "PlayController.h"
#include "Core/Time.h"
#include "Scene/Scene.h"

namespace rd
{
	bool PlayController::Begin(const Scene& scene)
	{
		if (mPlaying) { return false; }
		mSnapshot = scene.Serialize().dump();
		mPlaying = true;
		mPaused = false;
		Time::sTotal = 0.0;
		return true;
	}

	std::unique_ptr<Scene> PlayController::End(const Scene& current)
	{
		if (!mPlaying) { return nullptr; }
		mPlaying = false;
		mPaused = false;
		// プレイ前の状態へ戻す（パスは現在のものを引き継ぐ）
		std::string path = current.GetPath();
		auto scene = std::make_unique<Scene>();
		scene->Deserialize(Json::parse(mSnapshot));
		scene->SetPath(path);
		return scene;
	}

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
