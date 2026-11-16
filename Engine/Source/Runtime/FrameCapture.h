#pragma once
#include <cstdint>
#include <string>

namespace rd
{
	class Renderer;

	// 自動テスト用に、指定したフレームの画面を PNG に保存する（frame < 0 なら何もしない）
	//  BeforeRender で保存を頼み、Present の後の AfterPresent で書き出す。対象のフレームだったら true（終了の合図）
	class FrameCapture
	{
	public:
		void Configure(int frame, const std::string& path)
		{
			mFrame = frame;
			mPath = path;
		}

		void BeforeRender(Renderer& renderer, uint64_t frame) const;
		bool AfterPresent(Renderer& renderer, uint64_t frame) const;

	private:
		bool IsTarget(uint64_t frame) const { return mFrame >= 0 && int64_t(frame) == mFrame; }

		int mFrame = -1;
		std::string mPath;
	};
}
