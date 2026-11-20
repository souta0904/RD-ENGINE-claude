#pragma once
#include "Platform/Window/WindowDesc.h"
#include <Windows.h>
#include <functional>
#include <string>
#include <vector>

namespace rd
{
	// Win32 のウィンドウ
	class RD_API Window
	{
	public:
		// true を返すとそのメッセージは処理済みになる
		using MessageHook = std::function<bool(HWND, UINT, WPARAM, LPARAM)>;
		using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;

		bool Create(const WindowDesc& desc);
		void Destroy();
		// 終了の要求が来たら false
		bool PumpMessages();

		void SetTitle(const std::string& title);
		void SetMessageHook(MessageHook hook) { mHook = std::move(hook); }
		void SetResizeCallback(ResizeCallback callback) { mOnResize = std::move(callback); }
		void RequestClose();

		// ボーダーレスフルスクリーン（今いるモニタ全体を覆う。大きさが変わるので ResizeCallback も呼ばれる）
		void SetFullscreen(bool fullscreen);
		void ToggleFullscreen() { SetFullscreen(!mFullscreen); }
		bool IsFullscreen() const { return mFullscreen; }

		// 今の置き場所。ウィンドウが閉じられた後は、閉じる直前の値を返す
		WindowPlacement GetPlacement();

		// エクスプローラーからファイルを落とせるようにする（エディタだけが呼ぶ）
		void EnableFileDrop();
		// 前回から今までに落とされたファイルを受け取る。outPoint は落とされた位置（ウィンドウの左上が原点）
		std::vector<std::wstring> TakeDroppedFiles(POINT& outPoint);

		HWND GetHandle() const { return mHwnd; }
		uint32_t GetWidth() const { return mWidth; }
		uint32_t GetHeight() const { return mHeight; }
		bool IsMinimized() const { return mMinimized; }
		bool IsFocused() const { return mFocused; }

	private:
		static LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		LRESULT HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam);
		void HandleFileDrop(WPARAM dropHandle);
		void CapturePlacement();

		HWND mHwnd = nullptr;
		uint32_t mWidth = 0;
		uint32_t mHeight = 0;
		bool mMinimized = false;
		bool mFocused = true;
		bool mFullscreen = false;
		bool mFullscreenHotkeys = true;
		DWORD mWindowedStyle = 0;
		WINDOWPLACEMENT mWindowedPlacement = { sizeof(WINDOWPLACEMENT) };
		WindowPlacement mPlacement;
		MessageHook mHook;
		ResizeCallback mOnResize;
		std::vector<std::wstring> mDroppedFiles;
		POINT mDropPoint = {};
	};
}
