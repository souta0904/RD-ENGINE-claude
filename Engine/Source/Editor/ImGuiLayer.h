#pragma once
#include <Windows.h>

namespace rd
{
	// ImGui の初期化と描画（Win32 + DX12 バックエンド）
	class ImGuiLayer
	{
	public:
		void Initialize(HWND hwnd);
		void Shutdown();
		void BeginFrame();
		// 現在設定されているレンダーターゲットへ描画
		void Render();
		bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		bool IsInitialized() const { return mInitialized; }

	private:
		void SetupStyle();
		bool mInitialized = false;
	};
}
