#pragma once
#include <Windows.h>
#include <cstdint>
#include <functional>
#include <string>

namespace rd
{
	struct WindowDesc
	{
		std::string title = "RD ENGINE";
		uint32_t width = 1600;
		uint32_t height = 900;
		bool resizable = true;
		bool maximized = false;
		bool fullscreen = false;// ボーダーレスフルスクリーンで起動
		bool fullscreenHotkeys = true;// Alt+Enter / F11 で切り替え
	};

	// Win32 ウィンドウ
	class Window
	{
	public:
		// メッセージフック（true を返すとそのメッセージは処理済み）
		using MessageHook = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
		using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

		bool Create(const WindowDesc& desc);
		void Destroy();
		// メッセージを処理（終了要求があれば false）
		bool PumpMessages();

		void SetTitle(const std::string& title);
		void SetMessageHook(MessageHook hook) { mHook = std::move(hook); }
		void SetResizeCallback(ResizeCallback callback) { mOnResize = std::move(callback); }
		void RequestClose();

		// ボーダーレスフルスクリーン（ウィンドウが今いるモニタ全体を覆う）
		// サイズが変わるので WM_SIZE 経由で ResizeCallback が呼ばれ、スワップチェインも追従する
		void SetFullscreen(bool fullscreen);
		void ToggleFullscreen() { SetFullscreen(!mFullscreen); }
		bool IsFullscreen() const { return mFullscreen; }

		HWND GetHandle() const { return mHwnd; }
		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		bool IsMinimized() const { return mMinimized; }
		bool IsFocused() const { return mFocused; }

	private:
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);

		HWND mHwnd = nullptr;
		uint32_t mWidth = 0;
		uint32_t mHeight = 0;
		bool mMinimized = false;
		bool mFocused = true;
		bool mFullscreen = false;
		bool mFullscreenHotkeys = true;
		DWORD mWindowedStyle = 0;// フルスクリーンから戻すときのスタイル
		WINDOWPLACEMENT mWindowedPlacement = { sizeof(WINDOWPLACEMENT) };// 戻すときの位置・大きさ・最大化状態
		MessageHook mHook;
		ResizeCallback mOnResize;
	};
}
