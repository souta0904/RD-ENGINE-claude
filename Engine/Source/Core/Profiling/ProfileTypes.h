#pragma once
#include <cstdint>
#include <vector>

namespace rd
{
	// CPU の計測区間 1 つ（name は文字列リテラル、時刻は QueryPerformanceCounter の値）
	//  name を std::string にしないのは、毎フレーム何千個も作るので、文字列のコピーを避けるため。
	//  文字列リテラルはプログラムの終わりまで消えないので、ポインタだけ持てば足りる
	//  時刻は「ティック」という単位の整数（秒に直すには Profiler::TicksToMs を使う）
	struct ProfileEvent
	{
		const char* name;
		int64_t start;
		int64_t end;
		uint16_t thread; // どのスレッドで測ったか（Profiler::GetThreadNames の番号）
		uint16_t depth;  // 入れ子の深さ（0 が一番外側）。画面で段を分けて描くのに使う
	};

	// GPU の計測区間 1 つ（フレームの最初のタイムスタンプからのミリ秒）
	struct GpuProfileEvent
	{
		const char* name;
		double startMs;
		double endMs;
		uint16_t depth;
	};

	// 1 フレーム分の記録（events は開始時刻の順。gpu は数フレーム遅れて届く）
	//  GPU は CPU より遅れて描画するので、GPU の時間が分かるのは数フレーム後になる
	struct RD_API ProfileFrame
	{
		uint64_t number = 0; // フレームの番号
		int64_t start = 0;   // フレームの始まりと終わり（ティック）
		int64_t end = 0;
		std::vector<ProfileEvent> events;
		std::vector<GpuProfileEvent> gpu;
		double gpuMs = 0.0;  // GPU がこのフレームにかかった時間

		double GetMs() const;
	};

	// 区間ごとの集計（avgMs と calls は 1 フレームあたりの平均）
	struct ProfileStat
	{
		const char* name;
		double avgMs;  // 1 フレームあたりの平均の時間
		double maxMs;  // 一番長かったフレームの時間
		double calls;  // 1 フレームあたりの平均の呼ばれた回数
		bool gpu;      // GPU の区間か
	};
}
