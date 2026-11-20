#pragma once
#include <Windows.h>

namespace rd
{
	// ImGui の初期化と描画（Win32 + DX12 のバックエンド。Render は今のレンダーターゲットへ描く）
	class RD_API ImGuiLayer
	{
	public:
		void Initialize(HWND hwnd);
		void Shutdown();
		void BeginFrame();
		void Render();
		bool HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		bool IsInitialized() const { return mInitialized; }

	private:
		void LoadFonts(float size);
		bool mInitialized = false;
	};
}
