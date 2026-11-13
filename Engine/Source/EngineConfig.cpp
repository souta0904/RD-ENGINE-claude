#include "RdPch.h"
#include "EngineConfig.h"
#include "Core/StringUtil.h"
#include <shellapi.h>

namespace rd
{
	void EngineConfig::ParseCommandLine()
	{
		int argc = 0;
		LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
		for (int i = 1; i < argc; ++i)
		{
			std::string arg = ToUtf8(argv[i]);
#if RD_EDITOR
			if (arg == "-noeditor") { enableEditor = false; continue; }
			if (arg == "-play") { autoPlay = true; continue; }
#endif
			if (arg == "-novsync") { vsync = false; }
			else if (arg == "-fullscreen") { window.fullscreen = true; }
			else if (arg == "-windowed") { window.fullscreen = false; }
			else if (arg == "-scene" && i + 1 < argc) { startScene = ToUtf8(argv[++i]); }
			else if (arg == "-capture" && i + 1 < argc)
			{
				captureFrame = std::stoi(ToUtf8(argv[++i]));
				if (i + 1 < argc && argv[i + 1][0] != L'-') { capturePath = ToUtf8(argv[++i]); }
			}
		}
		LocalFree(argv);
	}
}
