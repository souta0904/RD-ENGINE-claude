#pragma once
#include <atomic>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>

namespace rd
{
	class Editor;

	// エディタを開いたまま、ゲームのコードをビルドして読み直す（Unreal のホットリロードにあたる）
	//
	//  - Compile（ツールバーのボタン / Ctrl+Alt+F11）: ゲームのプロジェクトだけを MSBuild でビルドする。
	//    裏のスレッドで動かすので、ビルド中もエディタは止まらない。出力は Log パネルに出す
	//  - Visual Studio でビルドしても、できた DLL が新しくなったことに気づいて読み直す
	//    （デバッグ中の Visual Studio はビルドさせてくれないので、そのときはエディタの Compile を使う）
	//  - プレイ中は読み直さず、止めたときに読み直す
	//  読み直しの本体は ReloadGameCode（シーンを JSON にして、DLL を読み直し、JSON からシーンを作り直す）
	class RD_API HotReload
	{
	public:
		enum class State { Idle, Compiling, Succeeded, Failed };

		explicit HotReload(Editor& editor);
		~HotReload();
		HotReload(const HotReload&) = delete;
		HotReload& operator=(const HotReload&) = delete;

		// ゲームが DLL として読まれているか（Game 構成や Tests では使えない）
		bool IsAvailable() const;
		bool IsCompiling() const { return mCompiling; }
		State GetState() const { return mState; }
		// ツールバーに出す短い説明（"Compiling..." など）
		const char* GetStatusText() const;

		void Compile();
		// 毎フレーム呼ぶ（ビルドの終わりと、新しい DLL ができたかを見る）
		void Update();

	private:
		void RunBuild(std::wstring commandLine);
		void ScheduleReload();
		// ビルドし直したゲームの DLL を読み直し、今のシーンを新しいコードで作り直す。失敗したら今までのコードのまま
		bool ReloadGameCode(std::string& error);

		Editor& mEditor;
		std::thread mThread;
		std::atomic<bool> mCompiling = false;
		std::atomic<int> mExitCode = 0;
		// ビルドを途中で止めるときのために、MSBuild のプロセスを覚えておく
		std::mutex mProcessMutex;
		void* mProcess = nullptr;

		State mState = State::Idle;
		bool mReloadPending = false;
		bool mToldWaitingForStop = false;
		double mNextCheckTime = 0.0;
		// 読み直しに失敗した DLL（同じものを何度も読み直そうとしないため）
		std::filesystem::file_time_type mFailedBuildTime{};
	};
}
