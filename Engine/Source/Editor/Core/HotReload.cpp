#include "RdPch.h"
#include "Editor/Core/HotReload.h"
#include "Core/Jobs/JobSystem.h"
#include "Core/Time/Time.h"
#include "Core/Time/Timer.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Editor.h"
#include "Engine.h"
#include "Runtime/GameModule.h"
#include "Runtime/SceneManager.h"
#include "Scene/Scene.h"

namespace rd
{
	namespace fs = std::filesystem;

	namespace
	{
		// 新しい DLL ができたかを見る間隔（秒）
		constexpr double kCheckInterval = 0.5;

		// Visual Studio に入っている MSBuild.exe の場所（vswhere で探す。見つからなければ空）
		std::wstring FindMSBuild()
		{
			static std::wstring cached;
			if (!cached.empty()) { return cached; }

			wchar_t programFiles[MAX_PATH] = {};
			if (!GetEnvironmentVariableW(L"ProgramFiles(x86)", programFiles, MAX_PATH)) { return {}; }
			fs::path vswhere = fs::path(programFiles) / L"Microsoft Visual Studio" / L"Installer" / L"vswhere.exe";
			if (!fs::exists(vswhere)) { return {}; }

			// vswhere の出力をパイプで受け取る
			std::wstring command = L"\"" + vswhere.wstring() + L"\" -latest -prerelease -requires Microsoft.Component.MSBuild -find MSBuild\\**\\Bin\\amd64\\MSBuild.exe";
			SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
			HANDLE readPipe = nullptr, writePipe = nullptr;
			if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) { return {}; }
			SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
			STARTUPINFOW si{ sizeof(si) };
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = writePipe;
			si.hStdError = writePipe;
			PROCESS_INFORMATION pi{};
			BOOL ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
			CloseHandle(writePipe);
			std::string output;
			if (ok)
			{
				char buffer[1024];
				DWORD read = 0;
				while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0) { output.append(buffer, read); }
				WaitForSingleObject(pi.hProcess, INFINITE);
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
			CloseHandle(readPipe);

			std::string first = output.substr(0, output.find_first_of("\r\n"));
			if (!first.empty() && fs::exists(ToWide(first))) { cached = ToWide(first); }
			return cached;
		}

		// MSBuild の 1 行をログへ出す（エラーと警告はその重さで）
		void LogBuildLine(const std::string& line)
		{
			if (line.find_first_not_of(" \t") == std::string::npos) { return; }
			if (line.find(": error ") != std::string::npos || line.find(": fatal error ") != std::string::npos) { Logger::Error(LogCat::Editor, "{}", line); }
			else if (line.find(": warning ") != std::string::npos) { Logger::Warning(LogCat::Editor, "{}", line); }
			else { Logger::Info(LogCat::Editor, "{}", line); }
		}

		// MSBuild の出力を UTF-8 にする。出力先がパイプのとき MSBuild は UTF-8 で書くが、
		//  中で動くツールがコンソールの文字コード（日本語の Windows なら Shift_JIS）で書くこともあるので、
		//  UTF-8 として正しくなければコンソールの文字コードとして読む
		std::string DecodeBuildOutput(const std::string& text)
		{
			if (text.empty()) { return text; }
			if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, text.data(), int(text.size()), nullptr, 0) > 0) { return text; }
			int wideLength = MultiByteToWideChar(CP_OEMCP, 0, text.data(), int(text.size()), nullptr, 0);
			std::wstring wide(size_t(wideLength), L'\0');
			MultiByteToWideChar(CP_OEMCP, 0, text.data(), int(text.size()), wide.data(), wideLength);
			return ToUtf8(wide);
		}
	}

	HotReload::HotReload(Editor& editor) : mEditor(editor) {}

	// ビルド中にエディタを閉じたら、MSBuild を止めてからスレッドを待つ
	HotReload::~HotReload()
	{
		{
			std::lock_guard lock(mProcessMutex);
			if (mProcess) { TerminateProcess(mProcess, 1); }
		}
		if (mThread.joinable()) { mThread.join(); }
	}

	bool HotReload::IsAvailable() const
	{
		return GameModule::IsLoaded();
	}

	const char* HotReload::GetStatusText() const
	{
		switch (mState)
		{
		case State::Compiling: return "Compiling...";
		case State::Succeeded: return "Reloaded";
		case State::Failed: return "Compile failed";
		default: return "";
		}
	}

	// ゲームのプロジェクト（<ルート>/<名前>/<名前>.vcxproj）だけをビルドする。
	//  エンジンは読み込み中で書き換えられないので、参照しているプロジェクトはビルドしない（BuildProjectReferences=false）
	void HotReload::Compile()
	{
		if (!IsAvailable() || mCompiling) { return; }
		std::wstring msbuild = FindMSBuild();
		if (msbuild.empty())
		{
			Logger::Error(LogCat::Editor, "MSBuild was not found. Install Visual Studio with the C++ workload.");
			mState = State::Failed;
			return;
		}
		const std::string& name = GameModule::GetName();
		fs::path project = fs::absolute(fs::path(ToWide(name)) / (ToWide(name) + L".vcxproj"));
		if (!fs::exists(project))
		{
			Logger::Error(LogCat::Editor, "Game project not found: {}", ToUtf8(project.wstring()));
			mState = State::Failed;
			return;
		}
#if RD_DEBUG
		const wchar_t* configuration = L"Debug";
#else
		const wchar_t* configuration = L"Release";
#endif
		std::wstring command = std::format(L"\"{}\" \"{}\" -p:Configuration={} -p:Platform=x64 -p:BuildProjectReferences=false -nologo -v:minimal -m",
			msbuild, project.wstring(), configuration);

		if (mThread.joinable()) { mThread.join(); }
		Logger::Info(LogCat::Editor, "Compiling {} ({})...", name, ToUtf8(configuration));
		mState = State::Compiling;
		mCompiling = true;
		mThread = std::thread([this, command] { RunBuild(command); });
	}

	// 裏のスレッド: MSBuild を動かし、出力を 1 行ずつログへ出す
	void HotReload::RunBuild(std::wstring commandLine)
	{
		SetThreadDescription(GetCurrentThread(), L"RD Hot Reload Build");
		SECURITY_ATTRIBUTES sa{ sizeof(sa), nullptr, TRUE };
		HANDLE readPipe = nullptr, writePipe = nullptr;
		int exitCode = -1;
		if (CreatePipe(&readPipe, &writePipe, &sa, 0))
		{
			SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0);
			STARTUPINFOW si{ sizeof(si) };
			si.dwFlags = STARTF_USESTDHANDLES;
			si.hStdOutput = writePipe;
			si.hStdError = writePipe;
			PROCESS_INFORMATION pi{};
			BOOL ok = CreateProcessW(nullptr, commandLine.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi);
			CloseHandle(writePipe);
			if (ok)
			{
				{
					std::lock_guard lock(mProcessMutex);
					mProcess = pi.hProcess;
				}
				std::string pending;
				char buffer[4096];
				DWORD read = 0;
				while (ReadFile(readPipe, buffer, sizeof(buffer), &read, nullptr) && read > 0)
				{
					pending.append(buffer, read);
					size_t end;
					while ((end = pending.find('\n')) != std::string::npos)
					{
						std::string line = pending.substr(0, end);
						if (!line.empty() && line.back() == '\r') { line.pop_back(); }
						LogBuildLine(DecodeBuildOutput(line));
						pending.erase(0, end + 1);
					}
				}
				LogBuildLine(DecodeBuildOutput(pending));
				WaitForSingleObject(pi.hProcess, INFINITE);
				DWORD code = 1;
				GetExitCodeProcess(pi.hProcess, &code);
				exitCode = int(code);
				{
					std::lock_guard lock(mProcessMutex);
					mProcess = nullptr;
				}
				CloseHandle(pi.hProcess);
				CloseHandle(pi.hThread);
			}
			else
			{
				Logger::Error(LogCat::Editor, "Failed to start MSBuild (error {})", GetLastError());
			}
			CloseHandle(readPipe);
		}
		mExitCode = exitCode;
		mCompiling = false;
	}

	void HotReload::Update()
	{
		if (!IsAvailable()) { return; }

		// ビルドが終わった
		if (!mCompiling && mThread.joinable())
		{
			mThread.join();
			if (mExitCode != 0)
			{
				Logger::Error(LogCat::Editor, "Compile failed. The editor keeps running the previous game code.");
				mState = State::Failed;
			}
			else if (!GameModule::HasNewerBuild())
			{
				// コードが変わっていなければリンクされず、DLL もそのまま
				Logger::Info(LogCat::Editor, "Compile finished. No changes to reload.");
				mState = State::Idle;
			}
		}

		// Visual Studio やエディタのビルドで、新しい DLL ができたか
		double now = Time::RealTime();
		if (!mCompiling && !mReloadPending && now >= mNextCheckTime)
		{
			mNextCheckTime = now + kCheckInterval;
			if (GameModule::HasNewerBuild() && GameModule::GetBuiltWriteTime() != mFailedBuildTime)
			{
				mReloadPending = true;
				mToldWaitingForStop = false;
			}
		}

		if (!mReloadPending) { return; }
		if (mEditor.IsPlaying())
		{
			if (!mToldWaitingForStop)
			{
				Logger::Info(LogCat::Editor, "The game code was rebuilt. It will be reloaded when you stop playing.");
				mToldWaitingForStop = true;
			}
			return;
		}
		ScheduleReload();
	}

	// パネルを描き終えてから読み直す（描いている途中にシーンを作り直すと、辿っている途中のアクターが消えるため）
	void HotReload::ScheduleReload()
	{
		mReloadPending = false;
		mEditor.Defer([this]
			{
				if (mEditor.IsPlaying()) { mReloadPending = true; return; }
				auto builtTime = GameModule::GetBuiltWriteTime();
				std::string error;
				Timer timer;
				if (ReloadGameCode(error))
				{
					Logger::Info(LogCat::Editor, "Reloaded the game code ({} reloads, {:.0f} ms)", GameModule::GetReloadCount(), timer.GetTotalSeconds() * 1000.0);
					mState = State::Succeeded;
				}
				else
				{
					Logger::Error(LogCat::Editor, "Hot reload failed: {}", error);
					mFailedBuildTime = builtTime;
					mState = State::Failed;
				}
			});
	}

	// 1. 動いているジョブを待つ（古いコンポーネントを触っているかもしれない）
	// 2. 今のシーンを JSON にしておく（保存していない変更も、選んでいるアクターの ID もそのまま残る）
	// 3. 新しい DLL を読む。ゲームのコンポーネントの登録が新しい型に入れ替わる
	// 4. JSON からシーンを作り直す。足した変数には初期値が入り、消したクラスは MissingComponent になる
	//  Undo の履歴はアクターの ID と JSON で持っているので、そのまま使える
	bool HotReload::ReloadGameCode(std::string& error)
	{
		if (!GameModule::IsLoaded()) { error = "The game code is not a DLL in this build"; return false; }
		if (mEditor.IsPlaying()) { error = "Stop playing before reloading the game code"; return false; }

		JobSystem::WaitIdle();
		JobSystem::ExecuteMainThreadTasks();
		Scene& scene = mEditor.GetScene();
		Json snapshot = scene.Serialize();
		std::string path = scene.GetPath();

		if (!GameModule::Reload(error)) { return false; }

		auto fresh = std::make_unique<Scene>();
		fresh->Deserialize(snapshot);
		fresh->SetPath(path);
		mEditor.GetEngine().GetSceneManager().SetScene(std::move(fresh));
		mEditor.OnGameCodeReloaded();
		return true;
	}
}