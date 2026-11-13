#pragma once

namespace rd
{
	// FPS とフレーム時間の計測（0.5 秒ごとに平均を更新する）
	class FrameStats
	{
	public:
		void Tick(float dt)
		{
			mAccum += dt;
			++mFrames;
			if (mAccum >= 0.5f)
			{
				mFps = float(mFrames) / mAccum;
				mFrameMs = mAccum * 1000.0f / float(mFrames);
				mAccum = 0.0f;
				mFrames = 0;
			}
		}

		float GetFps() const { return mFps; }
		float GetFrameMs() const { return mFrameMs; }

	private:
		float mAccum = 0.0f;
		int mFrames = 0;
		float mFps = 0.0f;
		float mFrameMs = 0.0f;
	};
}
