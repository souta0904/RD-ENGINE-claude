#include "Engine.h"
#include "Scene/Scene.h"
#include <Windows.h>
#include <filesystem>

namespace sample
{
	void BuildSampleScene(rd::Scene& scene);
}

int WINAPI wWinMain(HINSTANCE, HINSTANCE, LPWSTR, int)
{
	// exe の場所に関係なく、プロジェクトのルート（Assets がある場所）を作業ディレクトリにする
	if (!std::filesystem::exists("Assets"))
	{
		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		for (auto dir = std::filesystem::path(exe).parent_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
		{
			if (std::filesystem::exists(dir / "Assets")) { std::filesystem::current_path(dir); break; }
		}
	}

	rd::EngineConfig config;
	config.window.title = "RD ENGINE Sandbox";
	config.window.width = 1600;
	config.window.height = 900;
	config.startScene = "Assets/Sample/Scenes/Sample.scene";
	config.buildDefaultScene = [](rd::Scene& scene)
		{
			sample::BuildSampleScene(scene);
			// 初回はファイルに保存しておく（以後はエディタで編集できる）
			if (!std::filesystem::exists("Assets/Sample/Scenes/Sample.scene"))
			{
				scene.SaveToFile("Assets/Sample/Scenes/Sample.scene");
			}
		};
	config.ParseCommandLine();

	rd::Engine engine;
	return engine.Run(config);
}
