#pragma once

namespace rd
{
#if RD_EDITOR
	class Editor;
#endif
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
		void Initialize(Window& window, GraphicsDevice& device, Renderer& renderer);
#if RD_EDITOR
		// エディタがあるときはエディタカメラ・ギズモ・エディタ UI を使う（nullptr でゲーム表示）
		void SetEditor(Editor* editor) { mEditor = editor; }
#endif
		void Render(Scene& scene, float realTime);

	private:
		void ComposeBackBuffer();

		Window* mWindow = nullptr;
		GraphicsDevice* mDevice = nullptr;
		Renderer* mRenderer = nullptr;
#if RD_EDITOR
		Editor* mEditor = nullptr;
#endif
	};
}
