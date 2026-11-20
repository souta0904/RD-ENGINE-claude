#include "RdPch.h"
#include "Runtime/GameModule.h"
#include "Core/Util/StringUtil.h"
#include "Scene/Component/ComponentRegistry.h"

namespace rd
{
	namespace fs = std::filesystem;

	namespace
	{
		fs::path ExeDirectory()
		{
			wchar_t exe[MAX_PATH] = {};
			GetModuleFileNameW(nullptr, exe, MAX_PATH);
			return fs::path(exe).parent_path();
		}

		fs::path CopyDirectory()
		{
			return ExeDirectory() / "HotReload";
		}

		// リンカーが書き込み中なら、共有なしでは開けない
		bool IsFileReady(const fs::path& path)
		{
			HANDLE h = CreateFileW(path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
			if (h == INVALID_HANDLE_VALUE) { return false; }
			CloseHandle(h);
			return true;
		}

		// 前に起動したときのコピーを消す（別のエディタが使っているものは消せないので、そのまま残る）
		void DeleteOldCopies(const std::string& name)
		{
			std::error_code ec;
			fs::path dir = CopyDirectory();
			if (!fs::exists(dir, ec)) { return; }
			std::wstring prefix = ToWide(name) + L"-";
			for (const auto& entry : fs::directory_iterator(dir, ec))
			{
				if (entry.path().filename().wstring().starts_with(prefix)) { fs::remove(entry.path(), ec); }
			}
		}
	}

	fs::path GameModule::GetBuiltPath()
	{
		return ExeDirectory() / (ToWide(sName) + L".dll");
	}

	bool GameModule::Load(const std::string& name, std::string& error)
	{
		if (IsLoaded()) { error = "The game module is already loaded"; return false; }
		sName = name;
		DeleteOldCopies(name);
		if (!LoadCopy(1, error)) { return false; }
		sReloadCount = 0;
		return true;
	}

	bool GameModule::Reload(std::string& error)
	{
		if (!IsLoaded()) { error = "No game module is loaded"; return false; }
		// ゲームの型の登録をいったん外す。新しい DLL を読むと、その中の RD_REGISTER_COMPONENT が登録し直す。
		//  消したクラスは登録されないまま残らない（そのコンポーネントはシーンを作り直すと Missing になる）
		std::vector<ComponentRegistry::Entry> saved = ComponentRegistry::RemoveGameEntries();
		if (!LoadCopy(sReloadCount + 2, error))
		{
			ComponentRegistry::RestoreEntries(std::move(saved));
			return false;
		}
		++sReloadCount;
		return true;
	}

	fs::file_time_type GameModule::GetBuiltWriteTime()
	{
		std::error_code ec;
		auto time = fs::last_write_time(GetBuiltPath(), ec);
		return ec ? fs::file_time_type{} : time;
	}

	bool GameModule::HasNewerBuild()
	{
		if (!IsLoaded()) { return false; }
		std::error_code ec;
		fs::path built = GetBuiltPath();
		auto time = fs::last_write_time(built, ec);
		if (ec || time <= sLoadedTime) { return false; }
		return IsFileReady(built);
	}

	// ビルドの出力を HotReload/<名前>-<プロセス>-<番号>.dll へコピーして読む
	bool GameModule::LoadCopy(int module, std::string& error)
	{
		std::error_code ec;
		fs::path built = GetBuiltPath();
		if (!fs::exists(built, ec)) { error = "Not found: " + ToUtf8(built.wstring()); return false; }
		if (!IsFileReady(built)) { error = "The DLL is still being written by the build"; return false; }
		auto time = fs::last_write_time(built, ec);

		fs::create_directories(CopyDirectory(), ec);
		fs::path copy = CopyDirectory() / std::format(L"{}-{}-{}.dll", ToWide(sName), GetCurrentProcessId(), module);
		fs::copy_file(built, copy, fs::copy_options::overwrite_existing, ec);
		if (ec) { error = "Failed to copy the DLL: " + ec.message(); return false; }

		// 読み込みの途中で RD_REGISTER_COMPONENT が動くので、その間だけモジュールの番号を付ける
		ComponentRegistry::SetLoadingModule(module);
		HMODULE handle = LoadLibraryW(copy.c_str());
		ComponentRegistry::SetLoadingModule(0);
		if (!handle)
		{
			error = std::format("LoadLibrary failed (error {})", GetLastError());
			return false;
		}
		if (module == 1)
		{
			sConfigure = reinterpret_cast<ConfigureFunc>(GetProcAddress(handle, "RdConfigureGame"));
			if (!sConfigure) { error = "RdConfigureGame was not found. Write RD_GAME_MAIN(...) in the game code"; return false; }
		}
		sLoadedTime = time;
		return true;
	}
}
