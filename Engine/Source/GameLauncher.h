#pragma once
#include "EngineConfig.h"
#if !RD_ENGINE_DLL
#include <Windows.h>
#endif

// ゲームの起動
//
//  ゲームのプロジェクトは、設定を書く関数を 1 つ作って RD_GAME_MAIN に渡す:
//
//    void ConfigureGame(rd::EngineConfig& config)
//    {
//        config.window.title = "My Game";
//        config.startScene = "Assets/MyGame/Scenes/Main.scene";
//    }
//    RD_GAME_MAIN(ConfigureGame)
//
//  エディタ入りの構成（Debug / Release）では、ゲームは DLL になり、RdEditor.exe がそれを読んでこの関数を呼ぶ。
//  ゲームのコードを書き換えてビルドすると、エディタを開いたまま読み直せる（エディタの HotReload）。
//  Game 構成では、ゲームは exe になり、RD_GAME_MAIN が wWinMain を作る。
namespace rd
{
	using ConfigureGameFunc = void (*)(EngineConfig&);

	// exe の場所に関係なく、プロジェクトのルート（Assets がある場所）を作業ディレクトリにする
	RD_API void SetWorkingDirectoryToProjectRoot();
	// 設定を作ってエンジンを動かす。終わったら終了コードを返す
	RD_API int RunGame(ConfigureGameFunc configure, const std::string& gameName);
#if RD_ENGINE_DLL
	// RdEditor.exe の入口: コマンドラインの -game <名前> の DLL を読み、その設定でエンジンを動かす
	RD_API int RunEditorLauncher();
#endif
}

#if RD_ENGINE_DLL
#define RD_GAME_MAIN(ConfigureFunction)                                                     \
	extern "C" __declspec(dllexport) void RdConfigureGame(::rd::EngineConfig& config)       \
	{                                                                                       \
		ConfigureFunction(config);                                                          \
	}
#else
#define RD_GAME_MAIN(ConfigureFunction)                                                     \
	int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)                                  \
	{                                                                                       \
		return ::rd::RunGame(&ConfigureFunction, "");                                       \
	}
#endif
