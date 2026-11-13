#pragma once
#include <cstdint>

namespace rd
{
	class DebugDraw;
	struct RenderCamera;

	// フレームの描画に割り込む層（エディタが実装する）
	//  FrameComposer はこのインターフェースしか知らないので、Runtime が Editor に依存しない。
	class FrameOverlay
	{
	public:
		virtual ~FrameOverlay() = default;
		// ゲーム画面を描く大きさ（エディタならビューポートの大きさ）
		virtual void GetViewportSize(uint32_t& width, uint32_t& height) const = 0;
		// カメラを差し替えるなら true を返して camera に入れる
		virtual bool OverrideCamera(float aspect, RenderCamera& camera) const = 0;
		// シーンの描画に重ねる線（ギズモなど）
		virtual void DrawGizmos(DebugDraw& draw) = 0;
		// バックバッファへの最終出力（ゲーム画面の代わりに UI を描く）
		virtual void RenderUI() = 0;
	};
}
