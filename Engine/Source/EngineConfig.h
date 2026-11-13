#pragma once
#include "Core/CrashHandler.h"
#include "Core/Log.h"
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

		// 起動から指定フレーム分のプロファイルを保存する（0 なら保存しない）
		size_t profileFrames = 0;
		std::string profilePath = "Logs/Profile.json";

		LogSettings log;
		// 落ちたときにレポートとダンプを残す（Logs/Crash_日時.txt / .dmp）
		bool crashHandler = true;
		CrashHandler::Settings crash;

		// コマンドライン（-novsync -fullscreen -windowed -capture <frame> <path> -scene <path>
		//  -loglevel trace|debug|info|warn|error -profile <frames> [path] -nocrashdialog）を反映
		//  エディタ入りのビルドでは -noeditor -play も使える
		void ParseCommandLine();
	};
}
