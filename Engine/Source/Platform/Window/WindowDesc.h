#pragma once
#include <cstdint>
#include <string>

namespace rd
{
	// ウィンドウの置き場所（x, y, width, height は枠を含む「通常表示のときの」位置と大きさ）
	//  最大化やフルスクリーンの間も通常表示の値を持つので、次の起動でそれらを解除しても元の大きさに戻れる
	struct WindowPlacement
	{
		int x = 0;
		int y = 0;
		int width = 0;
		int height = 0;
		bool maximized = false;
		bool fullscreen = false;
	};

	// ウィンドウの作成設定（fullscreen はボーダーレス、fullscreenHotkeys は Alt+Enter / F11 での切り替え）
	//  hasPlacement なら placement の位置と大きさで作る（width / height より優先。最大化などは下の 2 つで決める）
	struct WindowDesc
	{
		std::string title = "RD ENGINE";
		uint32_t width = 1600;
		uint32_t height = 900;
		bool resizable = true;
		bool maximized = false;
		bool fullscreen = false;
		bool fullscreenHotkeys = true;
		bool hasPlacement = false;
		WindowPlacement placement;
	};
}
