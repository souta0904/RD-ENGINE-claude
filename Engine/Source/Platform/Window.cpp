#include "RdPch.h"
#include "Platform/Window.h"
#include "Platform/Input.h"
#include "Core/StringUtil.h"

namespace rd
{
	namespace
	{
		constexpr wchar_t kClassName[] = L"RdEngineWindow";
	}

	bool Window::Create(const WindowDesc& desc)
	{
		SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

		HINSTANCE hinst = GetModuleHandleW(nullptr);
		WNDCLASSEXW wc = {};
		wc.cbSize = sizeof(wc);
		wc.style = CS_HREDRAW | CS_VREDRAW;
		wc.lpfnWndProc = WndProc;
		wc.hInstance = hinst;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
		wc.lpszClassName = kClassName;
		RegisterClassExW(&wc);

		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!desc.resizable) { style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX); }

		RECT rc = { 0, 0, LONG(desc.width), LONG(desc.height) };
		AdjustWindowRect(&rc, style, FALSE);

		mWidth = desc.width;
		mHeight = desc.height;
		mHwnd = CreateWindowExW(0, kClassName, ToWide(desc.title).c_str(), style,
			CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
			nullptr, nullptr, hinst, this);
		if (!mHwnd)
		{
			Log::Error("Failed to create window");
			return false;
		}
		ShowWindow(mHwnd, desc.maximized ? SW_SHOWMAXIMIZED : SW_SHOW);
		UpdateWindow(mHwnd);

		RECT client;
		GetClientRect(mHwnd, &client);
		mWidth = uint32_t(client.right - client.left);
		mHeight = uint32_t(client.bottom - client.top);
		return true;
	}

	void Window::Destroy()
	{
		if (mHwnd)
		{
			DestroyWindow(mHwnd);
			mHwnd = nullptr;
		}
		UnregisterClassW(kClassName, GetModuleHandleW(nullptr));
	}

	bool Window::PumpMessages()
	{
		MSG msg = {};
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT) { return false; }
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
		return true;
	}

	void Window::SetTitle(const std::string& title)
	{
		SetWindowTextW(mHwnd, ToWide(title).c_str());
	}

	void Window::RequestClose()
	{
		PostMessageW(mHwnd, WM_CLOSE, 0, 0);
	}

	LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Window* self = nullptr;
		if (msg == WM_NCCREATE)
		{
			auto cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			self = static_cast<Window*>(cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
			self->mHwnd = hwnd;
		}
		else
		{
			self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		}
		if (self)
		{
			if (self->mHook && self->mHook(hwnd, msg, wParam, lParam)) { return TRUE; }
			return self->HandleMessage(msg, wParam, lParam);
		}
		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT Window::HandleMessage(UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Input::ProcessMessage(mHwnd, msg, wParam, lParam);

		switch (msg)
		{
		case WM_SIZE:
		{
			mMinimized = (wParam == SIZE_MINIMIZED);
			uint32_t w = LOWORD(lParam);
			uint32_t h = HIWORD(lParam);
			if (!mMinimized && w > 0 && h > 0 && (w != mWidth || h != mHeight))
			{
				mWidth = w;
				mHeight = h;
				if (mOnResize) { mOnResize(w, h); }
			}
			return 0;
		}
		case WM_SETFOCUS:
			mFocused = true;
			return 0;
		case WM_KILLFOCUS:
			mFocused = false;
			Input::ResetState();
			return 0;
		case WM_GETMINMAXINFO:
		{
			auto info = reinterpret_cast<MINMAXINFO*>(lParam);
			info->ptMinTrackSize = { 320, 240 };
			return 0;
		}
		case WM_SYSKEYDOWN:
			// Alt+Enter などのシステムキーで止まらないようにする
			if (wParam == VK_F4 && (lParam & (1 << 29))) { break; }
			return 0;
		case WM_SYSCHAR:
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(mHwnd, msg, wParam, lParam);
	}
}
