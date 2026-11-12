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
		MessageHook mHook;
		ResizeCallback mOnResize;
	};
}
