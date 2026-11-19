#pragma once

namespace rd
{
	// FPS とフレーム時間（エディタの Stats などに出す）
	//  1 フレームずつの値は揺れて読めないので、0.5 秒分を貯めてその平均を出す。
	//  表示は 0.5 秒ごとに更新される
	class FrameStats
	{
	public:
		// 毎フレーム、そのフレームの秒数を渡す
		void Tick(float dt)
		{
			mAccum += dt;
			++mFrames;
			if (mAccum >= 0.5f)
			{
				// FPS = フレーム数 / 秒数、1 フレームの時間 = 秒数 / フレーム数（ミリ秒にするので 1000 倍）
				mFps = float(mFrames) / mAccum;
				mFrameMs = mAccum * 1000.0f / float(mFrames);
				mAccum = 0.0f;
				mFrames = 0;
			}
		}

		float GetFps() const { return mFps; }
		float GetFrameMs() const { return mFrameMs; }

	private:
		float mAccum = 0.0f; // 貯めている秒数
		int mFrames = 0;     // 貯めているフレーム数
		float mFps = 0.0f;
		float mFrameMs = 0.0f;
	};
}
