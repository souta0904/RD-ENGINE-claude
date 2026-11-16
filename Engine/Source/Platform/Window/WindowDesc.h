#pragma once
#include <cstdint>
#include <string>

namespace rd
{
	// ウィンドウの作成設定（fullscreen はボーダーレス、fullscreenHotkeys は Alt+Enter / F11 での切り替え）
	struct WindowDesc
	{
		std::string title = "RD ENGINE";
		uint32_t width = 1600;
		uint32_t height = 900;
		bool resizable = true;
		bool maximized = false;
		bool fullscreen = false;
		bool fullscreenHotkeys = true;
	};
}
