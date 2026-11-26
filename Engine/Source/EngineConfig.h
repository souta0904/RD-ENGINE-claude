#pragma once
#include "Core/Debug/CrashHandler.h"
#include "Core/Log/Logger.h"
#include "Platform/Window/Window.h"
#include <functional>
#include <string>

namespace rd
{
	class Scene;

	// エンジンの起動の設定
	//  autoPlay: エディタでも起動してすぐプレイする / startScene: 起動時に開くシーン（なければ buildDefaultScene で作る）
	//  captureFrame: 自動テスト用。このフレームの画面を保存して終わる / profileFrames: 起動からこの数のフレームのプロファイルを保存する
	//  crashHandler: 落ちたときにレポートとダンプを残す（Logs/Crash_日時.txt / .dmp）
	struct RD_API EngineConfig
	{
		// ゲームの名前（エディタが読んだゲームの DLL の名前。Game 構成の exe では空）
		std::string gameName;
		WindowDesc window;
#if RD_EDITOR
		bool enableEditor = true;
		bool autoPlay = false;
#endif
		bool vsync = true;
		std::string startScene;
		std::function<void(Scene&)> buildDefaultScene;
		// コマンドラインで指定された（前回のエディタの状態より優先する）
		bool sceneFromCommandLine = false;
		bool fullscreenFromCommandLine = false;

		int captureFrame = -1;
		std::string capturePath = "Logs/capture.png";

		size_t profileFrames = 0;
		std::string profilePath = "Logs/Profile.json";

		// 描き方を Deferred で始める（Render Settings の Render Path）
		bool deferred = false;

		LogSettings log;
		bool crashHandler = true;
		CrashHandler::Settings crash;

		// コマンドラインを反映する（-novsync -fullscreen -windowed -capture <frame> <path> -scene <path>
		//  -loglevel trace|debug|info|warn|error -logfile <name> -profile <frames> [path] -nocrashdialog -renderpath forward|deferred）
		//  エディタ入りのビルドでは -noeditor -play も使える
		void ParseCommandLine();
	};
}
