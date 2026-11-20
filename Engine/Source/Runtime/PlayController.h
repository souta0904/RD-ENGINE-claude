#pragma once
#include <memory>
#include <string>

namespace rd
{
	class Scene;

	// プレイの状態（Play / Pause / Step）とゲーム時間の進め方
	//  エディタではプレイを始めるときにシーンを保存しておき、止めたときにその状態へ戻す
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
		// エディタなしでの常時プレイや、シーンの切り替えでの停止に使う
		void SetPlaying(bool playing) { mPlaying = playing; }

#if RD_EDITOR
		// もうプレイ中なら false
		bool Begin(const Scene& scene);
		// 保存しておいたシーンを作り直して返す（プレイ中でなければ nullptr）
		std::unique_ptr<Scene> End();
		// プレイを始めたときのシーンのパス
		const std::string& GetSnapshotPath() const { return mSnapshotPath; }
#endif

		FrameTime AdvanceTime(float dt);

	private:
		bool mPlaying = false;
		bool mPaused = false;
		bool mStepRequested = false;
#if RD_EDITOR
		std::string mSnapshot;
		std::string mSnapshotPath;
#endif
	};
}
