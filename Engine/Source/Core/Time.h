#pragma once
#include <cstdint>

namespace rd
{
	// ゲーム時間（エンジンが毎フレーム更新する）
	class Time
	{
	public:
		static float DeltaTime() { return sDelta; }// timeScale 適用済み
		static float UnscaledDeltaTime() { return sUnscaledDelta; }
		static float FixedDeltaTime() { return sFixedDelta; }
		static float TimeScale() { return sTimeScale; }
		static double TotalTime() { return sTotal; }// プレイ開始からの時間
		static double RealTime() { return sRealTime; }// 起動からの時間
		static uint64_t FrameCount() { return sFrameCount; }

		static void SetTimeScale(float scale) { sTimeScale = scale < 0.0f ? 0.0f : scale; }
		static void SetFixedDeltaTime(float dt) { sFixedDelta = dt; }

	private:
		friend class Engine;// sRealTime
		friend class PlayController;// ゲーム時間の進行
		static inline float sDelta = 0.0f;
		static inline float sUnscaledDelta = 0.0f;
		static inline float sFixedDelta = 1.0f / 60.0f;
		static inline float sTimeScale = 1.0f;
		static inline double sTotal = 0.0;
		static inline double sRealTime = 0.0;
		static inline uint64_t sFrameCount = 0;
	};
}
