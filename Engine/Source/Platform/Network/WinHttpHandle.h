#pragma once
#include <Windows.h>
#include <winhttp.h>

namespace rd
{
	// WinHTTP のハンドル（スコープを抜けると閉じる）
	class WinHttpHandle
	{
	public:
		WinHttpHandle() = default;
		explicit WinHttpHandle(HINTERNET handle) : h(handle) {}
		~WinHttpHandle() { if (h) { WinHttpCloseHandle(h); } }
		WinHttpHandle(const WinHttpHandle&) = delete;
		WinHttpHandle& operator=(const WinHttpHandle&) = delete;
		explicit operator bool() const { return h != nullptr; }

		HINTERNET h = nullptr;
	};
}
