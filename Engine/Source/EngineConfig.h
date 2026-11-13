#pragma once
#include "Platform/Window.h"
#include <functional>
#include <string>

namespace rd
{
	class Scene;

	// エンジンの起動設定
	struct EngineConfig
	{
		WindowDesc window;
#if RD_EDITOR
		bool enableEditor = true;
		bool autoPlay = false;// エディタでも起動直後にプレイ開始
#endif
		bool vsync = true;
		std::string startScene;// 起動時に開くシーン
		// startScene が存在しないときに初期シーンを組み立てる
		std::function<void(Scene&)> buildDefaultScene;

		// 自動テスト用: 指定フレームで画面を保存して終了
		int captureFrame = -1;
		std::string capturePath = "Logs/capture.png";

		// コマンドライン（-novsync -fullscreen -windowed -capture <frame> <path> -scene <path>）を反映
		//  エディタ入りのビルドでは -noeditor -play も使える
		void ParseCommandLine();
	};
}
