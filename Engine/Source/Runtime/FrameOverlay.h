#pragma once
#include <cstdint>

namespace rd
{
	class DebugDraw;
	struct RenderCamera;

	// フレームの描画に割り込む層（エディタが実装する）
	//  FrameComposer はこのインターフェースしか知らないので、Runtime は Editor に依存しない
	//  GetViewportSize: ゲーム画面を描く大きさ / OverrideCamera: カメラを差し替えるなら true
	//  DrawGizmos: シーンに重ねる線 / RenderUI: ゲーム画面の代わりにバックバッファへ UI を描く
	class FrameOverlay
	{
	public:
		virtual ~FrameOverlay() = default;
		virtual void GetViewportSize(uint32_t& width, uint32_t& height) const = 0;
		virtual bool OverrideCamera(float aspect, RenderCamera& camera) const = 0;
		virtual void DrawGizmos(DebugDraw& draw) = 0;
		virtual void RenderUI() = 0;
	};
}
