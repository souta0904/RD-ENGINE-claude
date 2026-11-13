#pragma once

namespace rd
{
	class Editor;
	class GraphicsDevice;
	class Renderer;
	class Scene;
	class Window;

	// 1 フレームの描画の組み立て
	//  カメラの決定 → シーンの描画 → バックバッファへの合成（エディタ UI か、ゲーム画面のコピー）
	//  デバイスの BeginFrame / EndFrame の間で呼ぶ。
	class FrameComposer
	{
	public:
		// editor は nullptr 可（エディタなし）
		void Initialize(Window& window, GraphicsDevice& device, Renderer& renderer, Editor* editor);
		void Render(Scene& scene, float realTime);

	private:
		void ComposeBackBuffer();

		Window* mWindow = nullptr;
		GraphicsDevice* mDevice = nullptr;
		Renderer* mRenderer = nullptr;
		Editor* mEditor = nullptr;
	};
}
