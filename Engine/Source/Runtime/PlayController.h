#pragma once
#include <memory>
#include <string>

namespace rd
{
	class Scene;

	// プレイ状態（Play / Pause / Step）とゲーム時間の進行
	class PlayController
	{
	public:
		// 1 フレーム分の時間の進め方
		struct FrameTime
		{
			bool advance = false;// ゲームを進めるか
			float gameDt = 0.0f;// timeScale 適用済み（進めないときは 0）
		};

		bool IsPlaying() const { return mPlaying; }
		bool IsPaused() const { return mPaused; }
		void SetPaused(bool paused) { mPaused = paused; }
		void StepFrame() { mStepRequested = true; }

		// プレイ状態を直接設定（エディタなしの常時プレイ、シーン切り替え時の停止）
		void SetPlaying(bool playing) { mPlaying = playing; }

#if RD_EDITOR
		// プレイ開始（停止時に戻すためシーンのスナップショットを取る）。すでにプレイ中なら false
		bool Begin(const Scene& scene);
		// プレイ終了。スナップショットから復元したシーンを返す（プレイ中でなければ nullptr）
		std::unique_ptr<Scene> End(const Scene& current);
#endif

		// 実時間 dt からゲーム時間を決めて Time を更新する
		FrameTime AdvanceTime(float dt);

	private:
		bool mPlaying = false;
		bool mPaused = false;
		bool mStepRequested = false;
#if RD_EDITOR
		std::string mSnapshot;// プレイ開始時のシーン
#endif
	};
}
