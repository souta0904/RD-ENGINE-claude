#include "RdPch.h"
#include "GameLauncher.h"
#include "Core/Util/StringUtil.h"
#include "Engine.h"
#include "Runtime/GameModule.h"
#include "Scene/Scene.h"
#include <shellapi.h>

namespace rd
{
	void SetWorkingDirectoryToProjectRoot()
	{
		namespace fs = std::filesystem;
		if (fs::exists("Assets")) { return; }
		wchar_t exe[MAX_PATH];
		GetModuleFileNameW(nullptr, exe, MAX_PATH);
		for (auto dir = fs::path(exe).parent_path(); dir.has_parent_path() && dir != dir.parent_path(); dir = dir.parent_path())
		{
			if (fs::exists(dir / "Assets")) { fs::current_path(dir); break; }
		}
	}

	int RunGame(ConfigureGameFunc configure, const std::string& gameName)
	{
		SetWorkingDirectoryToProjectRoot();
		EngineConfig config;
		config.gameName = gameName;
		if (configure) { configure(config); }
		config.ParseCommandLine();
		Engine engine;
		return engine.Run(config);
	}

#if RD_ENGINE_DLL
	namespace
	{
		std::string FindGameArgument()
		{
			int argc = 0;
			LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
			std::string name;
			for (int i = 1; i + 1 < argc; ++i)
			{
				if (std::wstring_view(argv[i]) == L"-game") { name = ToUtf8(argv[i + 1]); break; }
			}
			LocalFree(argv);
			return name;
		}

		int ShowLaunchError(const std::string& message)
		{
			MessageBoxW(nullptr, ToWide(message).c_str(), L"RD ENGINE", MB_OK | MB_ICONERROR);
			return -1;
		}
	}

	int RunEditorLauncher()
	{
		std::string name = FindGameArgument();
		if (name.empty())
		{
			return ShowLaunchError("Specify the game to open: RdEditor.exe -game <name>\n(for example -game Sandbox)");
		}
		std::string error;
		if (!GameModule::Load(name, error))
		{
			return ShowLaunchError(std::format("Failed to load the game module \"{}\".\n{}", name, error));
		}
		return RunGame(GameModule::GetConfigure(), name);
	}
#endif
}
