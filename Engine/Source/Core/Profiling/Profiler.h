#pragma once
#include "Core/Profiling/ProfileTypes.h"
#include <string>

namespace rd
{
	// フレームプロファイラ
	//  - RD_PROFILE_SCOPE("名前") で囲んだ区間の時間を、スレッドごと・フレームごとに記録する
	//  - GPU の時間は RHI がタイムスタンプで測って SubmitGpu で届ける
	//  - 直近 kHistory フレームを残し、Chrome のトレース形式（ui.perfetto.dev で開ける）で保存できる
	class Profiler
	{
	public:
		static constexpr size_t kHistory = 300;

		// メインスレッドで毎フレームの最初に呼ぶ
		static void BeginFrame();
		static uint64_t GetFrameNumber();

		static void BeginScope(const char* name);
		static void EndScope();
		static void SetThreadName(const char* name);

		// cpuFrame は計測したときの GetFrameNumber()
		static void SubmitGpu(uint64_t cpuFrame, std::vector<GpuProfileEvent> events);

		static void SetPaused(bool paused);
		static bool IsPaused();

		static const std::vector<ProfileFrame>& GetFrames();
		static std::vector<std::string> GetThreadNames();
		static double TicksToMs(int64_t ticks);
		// 直近 frameCount フレームの集計（時間の長い順）
		static std::vector<ProfileStat> ComputeStats(size_t frameCount);

		static bool SaveTrace(const std::string& path, size_t frameCount = kHistory);
		// frameCount フレーム後に自動で保存する（コマンドラインの -profile 用）
		static void RequestCapture(size_t frameCount, std::string path);
	};
}
