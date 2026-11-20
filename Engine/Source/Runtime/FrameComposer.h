#pragma once

namespace rd
{
	class FrameOverlay;
	class GraphicsDevice;
	class Renderer;
	class Scene;
	class Window;

	// 1 フレームの描画の組み立て（デバイスの BeginFrame / EndFrame の間で呼ぶ）
	//  カメラを決める → シーンを描く → バックバッファへまとめる（エディタの UI か、ゲーム画面のコピー）
	//  overlay（エディタなど）がなければゲーム画面をそのまま出す
	class RD_API FrameComposer
	{
	public:
		void Initialize(Window& window, GraphicsDevice& device, Renderer& renderer);
		void SetOverlay(FrameOverlay* overlay) { mOverlay = overlay; }
		void Render(Scene& scene, float realTime);

	private:
		void ComposeBackBuffer();

		Window* mWindow = nullptr;
		GraphicsDevice* mDevice = nullptr;
		Renderer* mRenderer = nullptr;
		FrameOverlay* mOverlay = nullptr;
	};
}
