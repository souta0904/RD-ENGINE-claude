#pragma once
#include <cstdint>
#include <string>
#include <vector>

// RD_PROFILER=0 にすると計測コードを丸ごと外す（既定は全構成で有効。1 区間あたり数十ナノ秒）
#ifndef RD_PROFILER
#define RD_PROFILER 1
#endif

namespace rd
{
	// CPU の計測区間 1 つ
	struct ProfileEvent
	{
		const char* name;// 文字列リテラル（寿命はプログラムと同じ）
		int64_t start;// QueryPerformanceCounter の値
		int64_t end;
		uint16_t thread;// Profiler::GetThreadNames() の番号
		uint16_t depth;// 同じスレッド内の入れ子の深さ
	};

	// GPU の計測区間 1 つ（フレームの最初のタイムスタンプからのミリ秒）
	struct GpuProfileEvent
	{
		const char* name;
		double startMs;
		double endMs;
		uint16_t depth;
	};

	// 1 フレーム分の記録
	struct ProfileFrame
	{
		uint64_t number = 0;
		int64_t start = 0;
		int64_t end = 0;
		std::vector<ProfileEvent> events;// 開始時刻の順
		std::vector<GpuProfileEvent> gpu;// 数フレーム遅れて届く
		double gpuMs = 0.0;

		double GetMs() const;
	};

	// 区間ごとの集計（直近のフレームから求める）
	struct ProfileStat
	{
		const char* name;
		double avgMs;// 1 フレームあたりの合計の平均
		double maxMs;
		double calls;// 1 フレームあたりの平均呼び出し回数
		bool gpu;
	};

	// フレームプロファイラ
	//  - RD_PROFILE_SCOPE("名前") で囲んだ区間の時間を、スレッドごと・フレームごとに記録する
	//  - GPU の時間は RHI がタイムスタンプで測って SubmitGpu で届ける
	//  - 直近 kHistory フレームを残す。Chrome のトレース形式（chrome://tracing / ui.perfetto.dev）で保存できる
	class Profiler
	{
	public:
		static constexpr size_t kHistory = 300;

		// フレームの区切り（メインスレッドで毎フレーム最初に呼ぶ）
		static void BeginFrame();
		static uint64_t GetFrameNumber();

		static void BeginScope(const char* name);
		static void EndScope();
		// 計測に表示するスレッド名（呼んだスレッドに付く）
		static void SetThreadName(const char* name);

		// GPU の結果（cpuFrame は計測したときの GetFrameNumber()）
		static void SubmitGpu(uint64_t cpuFrame, std::vector<GpuProfileEvent> events);

		// 一時停止中は記録を更新しない（エディタで見ている間に流れないように）
		static void SetPaused(bool paused);
		static bool IsPaused();

		// 記録（メインスレッドから読む。古い順）
		static const std::vector<ProfileFrame>& GetFrames();
		static std::vector<std::string> GetThreadNames();
		static double TicksToMs(int64_t ticks);
		// 直近 frameCount フレームの集計（時間の長い順）
		static std::vector<ProfileStat> ComputeStats(size_t frameCount);

		// 直近 frameCount フレームを Chrome のトレース形式で保存する
		static bool SaveTrace(const std::string& path, size_t frameCount = kHistory);
		// frameCount フレーム後に自動で保存する（コマンドラインの -profile 用）
		static void RequestCapture(size_t frameCount, std::string path);
	};

	// 区間を計測する（スコープを抜けると終わる）
	class ProfileScope
	{
	public:
		explicit ProfileScope(const char* name) { Profiler::BeginScope(name); }
		~ProfileScope() { Profiler::EndScope(); }
		ProfileScope(const ProfileScope&) = delete;
		ProfileScope& operator=(const ProfileScope&) = delete;
	};
}

#define RD_PROFILE_CONCAT_(a, b) a##b
#define RD_PROFILE_CONCAT(a, b) RD_PROFILE_CONCAT_(a, b)
#if RD_PROFILER
#define RD_PROFILE_SCOPE(name) ::rd::ProfileScope RD_PROFILE_CONCAT(rdProfileScope_, __LINE__)(name)
#else
#define RD_PROFILE_SCOPE(name) ((void)0)
#endif
// 関数名で計測する
#define RD_PROFILE_FUNCTION() RD_PROFILE_SCOPE(__FUNCTION__)
