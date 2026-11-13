#pragma once
#include <cstdint>
#include <string>

namespace rd
{
	class Renderer;

	// 自動テスト用: 指定フレームの画面を PNG に保存する
	class FrameCapture
	{
	public:
		// frame < 0 なら何もしない
		void Configure(int frame, const std::string& path)
		{
			mFrame = frame;
			mPath = path;
		}

		// 描画前: 対象フレームならキャプチャを要求する
		void BeforeRender(Renderer& renderer, uint64_t frame) const;
		// Present 後: 書き出しを行い、対象フレームだったら true（終了の合図）
		bool AfterPresent(Renderer& renderer, uint64_t frame) const;

	private:
		bool IsTarget(uint64_t frame) const { return mFrame >= 0 && int64_t(frame) == mFrame; }

		int mFrame = -1;
		std::string mPath;
	};
}
