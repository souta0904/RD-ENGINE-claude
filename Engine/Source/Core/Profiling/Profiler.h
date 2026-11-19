#pragma once
#include "Core/Profiling/ProfileTypes.h"
#include <string>

namespace rd
{
	// フレームプロファイラ（どの処理に何ミリ秒かかっているかを測る）
	//  - RD_PROFILE_SCOPE("名前") で囲んだ区間の時間を、スレッドごと・フレームごとに記録する
	//  - GPU の時間は RHI がタイムスタンプで測って SubmitGpu で届ける
	//  - 直近 kHistory フレームを残し、Chrome のトレース形式（ui.perfetto.dev で開ける）で保存できる
	//
	//  60 FPS なら 1 フレームは約 16.6 ミリ秒。重いフレームがあったとき、どの区間が長いかを
	//  エディタの Profiler パネルで見たり、保存して後から調べたりする
	class Profiler
	{
	public:
		// 残すフレームの数（60 FPS で 5 秒分）
		static constexpr size_t kHistory = 300;

		// メインスレッドで毎フレームの最初に呼ぶ（前のフレームを締めて、次のフレームを始める）
		static void BeginFrame();
		static uint64_t GetFrameNumber();

		// 区間の始まりと終わり（普段は RD_PROFILE_SCOPE を使い、直接は呼ばない）
		static void BeginScope(const char* name);
		static void EndScope();
		// 今のスレッドに名前を付ける（"Worker 0" など。Profiler パネルの行の名前になる）
		static void SetThreadName(const char* name);

		// cpuFrame は計測したときの GetFrameNumber()
		static void SubmitGpu(uint64_t cpuFrame, std::vector<GpuProfileEvent> events);

		// 一時停止すると、記録を止めて今の履歴を見続けられる
		static void SetPaused(bool paused);
		static bool IsPaused();

		static const std::vector<ProfileFrame>& GetFrames();
		static std::vector<std::string> GetThreadNames();
		// ティック（QueryPerformanceCounter の単位）をミリ秒にする
		static double TicksToMs(int64_t ticks);
		// 直近 frameCount フレームの集計（時間の長い順）
		static std::vector<ProfileStat> ComputeStats(size_t frameCount);

		// JSON のファイルに保存する。ui.perfetto.dev や chrome://tracing へドラッグすると時間の帯で見られる
		static bool SaveTrace(const std::string& path, size_t frameCount = kHistory);
		// frameCount フレーム後に自動で保存する（コマンドラインの -profile 用）
		static void RequestCapture(size_t frameCount, std::string path);
	};
}
