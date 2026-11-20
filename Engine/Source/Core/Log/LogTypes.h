#pragma once
#include <cstdint>
#include <source_location>
#include <string>

namespace rd
{
	// ログの重要度（下ほど重い）。設定したレベルより軽いログは出さない
	//  Trace   … 細かすぎて普段は見ない（毎フレームの値など）
	//  Debug   … 開発中に見たい情報
	//  Info    … 普段の動き（起動した、読み込んだ など）
	//  Warning … おかしいが続けられる（ファイルがないので既定値を使う など）
	//  Error   … 失敗した（読み込めなかった など）
	//  Fatal   … 続けられない。出した後にプログラムを終了する
	enum class LogLevel : uint8_t
	{
		Trace,
		Debug,
		Info,
		Warning,
		Error,
		Fatal,
	};
	// "Info" などの短い名前（ファイルや出力ウィンドウに出す）
	RD_API const char* ToString(LogLevel level);

	// コンパイル時に消すレベル（Game ビルドでは Debug 以下を消す）
	//  ここより軽いログは if constexpr でコードごと消えるので、製品版では書式の処理の時間もかからない
	//  数は LogLevel の並び（0 = Trace、2 = Info）。#ifndef なので、プロジェクトの設定で上書きもできる
#ifndef RD_LOG_COMPILE_LEVEL
#if !RD_EDITOR && !RD_DEBUG
#define RD_LOG_COMPILE_LEVEL 2
#else
#define RD_LOG_COMPILE_LEVEL 0
#endif
#endif
	inline constexpr LogLevel kLogCompileLevel = LogLevel(RD_LOG_COMPILE_LEVEL);

	// ログを出した機能の名前（エディタのコンソールで絞り込むのに使う）
	//  enum ではなく名前を持つ構造体にしているのは、ゲームの側で LogCategory{ "Cat" } のように
	//  エンジンを書き換えずに新しいカテゴリを足せるようにするため
	struct LogCategory
	{
		const char* name;
	};
	// エンジンの決まったカテゴリ（Log::Info(LogCat::Render, ...) のように使う）
	//  inline constexpr … ヘッダーに書いても、どの .cpp から見ても 1 つの同じ定数になる
	namespace LogCat
	{
		inline constexpr LogCategory General{ "General" };
		inline constexpr LogCategory Core{ "Core" };
		inline constexpr LogCategory Platform{ "Platform" };
		inline constexpr LogCategory RHI{ "RHI" };
		inline constexpr LogCategory Render{ "Render" };
		inline constexpr LogCategory Asset{ "Asset" };
		inline constexpr LogCategory Audio{ "Audio" };
		inline constexpr LogCategory Physics{ "Physics" };
		inline constexpr LogCategory Scene{ "Scene" };
		inline constexpr LogCategory Editor{ "Editor" };
		inline constexpr LogCategory Game{ "Game" };
	}

	// 1 件のログ
	struct LogRecord
	{
		LogLevel level = LogLevel::Info;
		const char* category = "General";
		double time = 0.0;                                     // 起動してからの秒数
		uint16_t hour = 0, minute = 0, second = 0, millisecond = 0; // 出した時刻（PC の時計）
		uint32_t threadId = 0;                                 // 出したスレッドの番号
		bool mainThread = false;                               // メインスレッドから出したか
		std::source_location location;                         // 出したソースの場所（ファイル・行）
		std::string message;
	};

	// ログの設定（前回のログは directory/History/ へ日時付きで移し、keepHistory 個だけ残す）
	struct LogSettings
	{
		std::string directory = "Logs";
		std::string fileName = "RdEngine.log";
		uint32_t keepHistory = 10;
		LogLevel minLevel = LogLevel::Debug; // これより軽いログは出さない（実行中に Log::SetLevel で変えられる）
	};
}
