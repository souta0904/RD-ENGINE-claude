#pragma once
#include <functional>
#include <string>

namespace rd
{
	// クラッシュハンドラ
	//  落ちたときに次のものを Logs/ に残す（デバッガ接続中は何もしない。デバッガで止まる方が便利なので）
	//    Crash_日時.txt  : 例外の種類・場所・呼び出し履歴（関数名とファイル:行）・環境・直前のログ
	//    Crash_日時.dmp  : ミニダンプ（Visual Studio で開くと落ちた行で止まった状態を再現できる）
	//  対象: 未処理の SEH 例外（アクセス違反・ゼロ除算・スタックオーバーフロー）、C++ の未処理例外、
	//        abort() / RD_VERIFY / Log::Fatal、純粋仮想関数の呼び出し、CRT の不正な引数
	class CrashHandler
	{
	public:
		struct Settings
		{
			std::string directory = "Logs";
			bool showDialog = true;// 落ちたことをメッセージボックスで知らせる（自動テストでは false）
			bool writeDump = true;
		};
		static void Install(const Settings& settings);
		static bool IsInstalled();

		// レポートに載せる情報を追加する（例: "Scene" → 開いているシーン名）。落ちたときに呼ばれる
		//  落ちた状態で呼ばれるので、ロックを取ったり大きな処理をしたりしないこと
		static void SetContext(const std::string& key, std::function<std::string()> provider);

		// 今のスレッドの呼び出し履歴（関数名とファイル:行。skip は飛ばす数）
		static std::string CaptureStackTrace(int skip = 0);
	};
}
