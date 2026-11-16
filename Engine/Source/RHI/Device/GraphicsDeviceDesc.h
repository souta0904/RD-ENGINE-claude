#pragma once
#include <Windows.h>
#include <cstdint>

namespace rd
{
	// GraphicsDevice の作成設定
	struct GraphicsDeviceDesc
	{
		HWND hwnd = nullptr;
		uint32_t width = 0;
		uint32_t height = 0;
		bool vsync = true;
		bool debugLayer = RD_DEBUG;
	};
}
