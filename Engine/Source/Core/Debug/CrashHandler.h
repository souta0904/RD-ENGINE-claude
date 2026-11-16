#pragma once
#include <functional>
#include <string>

namespace rd
{
	// クラッシュハンドラ
	//  落ちたときに Logs/ へ次のものを残す（デバッガをつないでいるときは何もしない）
	//    Crash_日時.txt : 例外の種類・場所・呼び出し履歴・環境・直前のログ
	//    Crash_日時.dmp : ミニダンプ（Visual Studio で開くと落ちた行で止まった状態を再現できる）
	//  対象: 未処理の SEH 例外、C++ の未処理例外、abort() / RD_VERIFY / Log::Fatal、純粋仮想関数の呼び出し、CRT の不正な引数
	class CrashHandler
	{
	public:
		struct Settings
		{
			std::string directory = "Logs";
			bool showDialog = true;
			bool writeDump = true;
		};
		static void Install(const Settings& settings);
		static bool IsInstalled();

		// レポートに載せる情報を足す（落ちた状態で呼ばれるので、ロックや重い処理をしないこと）
		static void SetContext(const std::string& key, std::function<std::string()> provider);

		static std::string CaptureStackTrace(int skip = 0);
	};
}
