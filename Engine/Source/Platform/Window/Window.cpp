#include "RdPch.h"
#include "Platform/Window/Window.h"
#include "Core/Util/StringUtil.h"
#include "Platform/Input/Input.h"
#include <shellapi.h>

namespace rd
{
	namespace
	{
		constexpr wchar_t kClassName[] = L"RdEngineWindow";
		constexpr LPARAM kAltDown = 1 << 29;
		constexpr LPARAM kRepeat = 1 << 30;
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
		mWindowedStyle = style;
		mFullscreenHotkeys = desc.fullscreenHotkeys;
		mHwnd = CreateWindowExW(0, kClassName, ToWide(desc.title).c_str(), style,
			CW_USEDEFAULT, CW_USEDEFAULT, rc.right - rc.left, rc.bottom - rc.top,
			nullptr, nullptr, hinst, this);
		if (!mHwnd)
		{
			Logger::Error(LogCat::Platform, "Failed to create window");
			return false;
		}
		// 保存された場所を使う。そこにモニタがなければ（外したなど）画面の外に出てしまうので、既定の場所で開く
		bool placed = false;
		if (desc.hasPlacement)
		{
			const WindowPlacement& p = desc.placement;
			RECT saved = { p.x, p.y, p.x + p.width, p.y + p.height };
			if (MonitorFromRect(&saved, MONITOR_DEFAULTTONULL))
			{
				WINDOWPLACEMENT wp = { sizeof(WINDOWPLACEMENT) };
				wp.showCmd = desc.maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
				wp.rcNormalPosition = saved;
				placed = SetWindowPlacement(mHwnd, &wp) != FALSE;
			}
		}
		if (!placed) { ShowWindow(mHwnd, desc.maximized ? SW_SHOWMAXIMIZED : SW_SHOW); }
		if (desc.fullscreen) { SetFullscreen(true); }
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

	void Window::SetFullscreen(bool fullscreen)
	{
		if (!mHwnd || fullscreen == mFullscreen) { return; }

		if (fullscreen)
		{
			// 戻すときのために、最大化かどうかも含めて今の配置を覚えておく
			mWindowedPlacement.length = sizeof(WINDOWPLACEMENT);
			GetWindowPlacement(mHwnd, &mWindowedPlacement);

			MONITORINFO mi = { sizeof(MONITORINFO) };
			GetMonitorInfoW(MonitorFromWindow(mHwnd, MONITOR_DEFAULTTONEAREST), &mi);
			const RECT& r = mi.rcMonitor;

			mFullscreen = true;
			SetWindowLongW(mHwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
			SetWindowPos(mHwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top,
				SWP_FRAMECHANGED | SWP_NOOWNERZORDER | SWP_SHOWWINDOW);
		}
		else
		{
			mFullscreen = false;
			SetWindowLongW(mHwnd, GWL_STYLE, mWindowedStyle | WS_VISIBLE);
			SetWindowPlacement(mHwnd, &mWindowedPlacement);
			SetWindowPos(mHwnd, nullptr, 0, 0, 0, 0,
				SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOOWNERZORDER);
		}
		Logger::Info(LogCat::Platform, "Fullscreen: {}", mFullscreen ? "on" : "off");
	}

	WindowPlacement Window::GetPlacement()
	{
		CapturePlacement();
		return mPlacement;
	}

	// ×ボタンで閉じるとエンジンの終了処理より先にウィンドウが消えるので、WM_DESTROY でも呼んで値を残しておく
	void Window::CapturePlacement()
	{
		if (!mHwnd || !IsWindow(mHwnd)) { return; }
		// フルスクリーンの間は画面全体の大きさになっているので、入る前に覚えた配置を使う
		WINDOWPLACEMENT wp = mWindowedPlacement;
		if (!mFullscreen)
		{
			wp.length = sizeof(WINDOWPLACEMENT);
			if (!GetWindowPlacement(mHwnd, &wp)) { return; }
		}
		const RECT& r = wp.rcNormalPosition;
		mPlacement.x = r.left;
		mPlacement.y = r.top;
		mPlacement.width = r.right - r.left;
		mPlacement.height = r.bottom - r.top;
		// 最小化のまま閉じたときは、最小化する前に最大化だったかを見る
		mPlacement.maximized = wp.showCmd == SW_SHOWMAXIMIZED || (wp.showCmd == SW_SHOWMINIMIZED && (wp.flags & WPF_RESTORETOMAXIMIZED));
		mPlacement.fullscreen = mFullscreen;
	}

	void Window::EnableFileDrop()
	{
		DragAcceptFiles(mHwnd, TRUE);
	}

	std::vector<std::wstring> Window::TakeDroppedFiles(POINT& outPoint)
	{
		outPoint = mDropPoint;
		return std::exchange(mDroppedFiles, {});
	}

	// WM_DROPFILES の中身（落とされたパスの一覧と位置）を取り出して、次のフレームで使えるように取っておく
	void Window::HandleFileDrop(WPARAM dropHandle)
	{
		HDROP drop = reinterpret_cast<HDROP>(dropHandle);
		// 2 番目の引数に 0xFFFFFFFF を渡すとファイルの数が返る
		UINT count = DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
		for (UINT i = 0; i < count; ++i)
		{
			// バッファなしで呼ぶと、終端を除いた文字数が返る
			UINT length = DragQueryFileW(drop, i, nullptr, 0);
			std::wstring path(size_t(length), L'\0');
			DragQueryFileW(drop, i, path.data(), length + 1);
			mDroppedFiles.push_back(std::move(path));
		}
		DragQueryPoint(drop, &mDropPoint);
		DragFinish(drop);
	}

	// 作るときに渡した this を GWLP_USERDATA に覚えさせ、以後のメッセージをメンバ関数へ渡す
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
		case WM_KEYDOWN:
			// Ctrl や Alt と一緒のとき（エディタの Ctrl+Alt+F11 = ゲームのコードのビルド）は切り替えない
			if (mFullscreenHotkeys && wParam == VK_F11 && !(lParam & kRepeat) && GetKeyState(VK_CONTROL) >= 0 && GetKeyState(VK_MENU) >= 0)
			{
				ToggleFullscreen();
				return 0;
			}
			break;
		case WM_SYSKEYDOWN:
			if (mFullscreenHotkeys && wParam == VK_RETURN && (lParam & kAltDown) && !(lParam & kRepeat))
			{
				ToggleFullscreen();
				return 0;
			}
			// Alt などのシステムキーでメニューに入って止まらないようにする（Alt+F4 だけは通す）
			if (wParam == VK_F4 && (lParam & kAltDown)) { break; }
			return 0;
		case WM_SYSCHAR:
			return 0;
		case WM_DROPFILES:
			HandleFileDrop(wParam);
			return 0;
		case WM_DESTROY:
			CapturePlacement();
			PostQuitMessage(0);
			return 0;
		}
		return DefWindowProcW(mHwnd, msg, wParam, lParam);
	}
}
