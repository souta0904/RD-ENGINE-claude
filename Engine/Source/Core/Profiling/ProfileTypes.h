#pragma once
#include <cstdint>
#include <vector>

namespace rd
{
	// CPU の計測区間 1 つ（name は文字列リテラル、時刻は QueryPerformanceCounter の値）
	struct ProfileEvent
	{
		const char* name;
		int64_t start;
		int64_t end;
		uint16_t thread;
		uint16_t depth;
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
	struct ProfileFrame
	{
		uint64_t number = 0;
		int64_t start = 0;
		int64_t end = 0;
		std::vector<ProfileEvent> events;
		std::vector<GpuProfileEvent> gpu;
		double gpuMs = 0.0;

		double GetMs() const;
	};

	// 区間ごとの集計（avgMs と calls は 1 フレームあたりの平均）
	struct ProfileStat
	{
		const char* name;
		double avgMs;
		double maxMs;
		double calls;
		bool gpu;
	};
}
