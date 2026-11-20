#pragma once
#include <functional>
#include <string>

namespace rd
{
	// クラッシュハンドラ
	//  落ちたときに Logs/ へ次のものを残す（デバッガをつないでいるときはダイアログを出さない）
	//    Crash_日時.txt : 例外の種類・場所・呼び出し履歴・環境・直前のログ
	//    Crash_日時.dmp : ミニダンプ（Visual Studio で開くと落ちた行で止まった状態を再現できる）
	//  対象: 未処理の SEH 例外、C++ の未処理例外、abort() / RD_VERIFY / Log::Fatal、純粋仮想関数の呼び出し、CRT の不正な引数
	//
	//  SEH（構造化例外）… Windows の仕組みの例外。null ポインタの読み書き・0 での割り算・スタックオーバーフローなど、
	//  CPU が止めた操作はこちらで届く（C++ の try / catch では捕まえられない）
	//
	//  ゲームを人に遊んでもらったときに落ちても、送ってもらった .txt と .dmp で原因を調べられるようにするためのもの
	class RD_API CrashHandler
	{
	public:
		struct Settings
		{
			std::string directory = "Logs"; // レポートとダンプを置くフォルダ
			bool showDialog = true;         // 落ちたことを知らせるダイアログを出すか
			bool writeDump = true;          // ミニダンプを書くか（数 MB～数十 MB になる）
		};
		// 起動のなるべく早くに 1 回呼ぶ（2 回目以降は設定だけ変わる）
		static void Install(const Settings& settings);
		static bool IsInstalled();

		// レポートに載せる情報を足す（例: SetContext("Scene", [] { return 今のシーンの名前; })）
		// 同じ key でもう一度呼ぶと置き換わる。provider は落ちた状態で呼ばれるので、ロックや重い処理をしないこと
		static void SetContext(const std::string& key, std::function<std::string()> provider);

		// 今の呼び出し履歴を文字列で返す（落ちていなくても使える。skip は先頭から飛ばす段数）
		static std::string CaptureStackTrace(int skip = 0);
	};
}
