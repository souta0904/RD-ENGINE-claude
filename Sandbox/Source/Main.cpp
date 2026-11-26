#include "GameLauncher.h"
#include "Scene/Scene.h"
#include "Scene/Serialization/SceneSerializer.h"
#include <filesystem>

namespace sample
{
	void BuildSampleScene(rd::Scene& scene);
}

// Sandbox の起動の設定（エディタ入りの構成では RdEditor.exe が、Game 構成では Sandbox.exe が呼ぶ）
static void ConfigureGame(rd::EngineConfig& config)
{
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
				rd::SceneSerializer::SaveToFile(scene, "Assets/Sample/Scenes/Sample.scene");
			}
		};
}

RD_GAME_MAIN(ConfigureGame)
