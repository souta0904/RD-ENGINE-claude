#include "RdPch.h"
#include "Core/Profiling/Profiler.h"
#include "Core/Util/StringUtil.h"
#include <fstream>

namespace rd
{
	namespace
	{
		// QueryPerformanceCounter … Windows で一番細かく時間を測れる時計（1 ティックはおよそ 0.1 マイクロ秒）
		// 値そのものに意味はなく、2 つの値の差だけを使う。1 秒あたりのティック数は QueryPerformanceFrequency で分かる
		int64_t Now()
		{
			LARGE_INTEGER t;
			QueryPerformanceCounter(&t);
			return t.QuadPart;
		}

		int64_t QueryFrequency()
		{
			LARGE_INTEGER f;
			QueryPerformanceFrequency(&f);
			return f.QuadPart;
		}

		// スレッドごとの記録
		//  開いている区間のスタックは持ち主のスレッドしか触らないのでロックしない。
		//  閉じた区間はフレームの区切りでメインスレッドが集めるのでロックする。
		//  区間は入れ子になる（Update の中に Physics があり、その中に Collide がある）ので、
		//  始まった区間をスタックに積み、終わったら一番上を取り出す
		struct OpenScope
		{
			const char* name;
			int64_t start;
		};
		struct ThreadBuffer
		{
			std::vector<OpenScope> stack;
			std::mutex mutex;
			std::vector<ProfileEvent> done;
			std::string name;
			uint16_t index = 0;
		};

		// 終了処理の順番に左右されないよう、作ったまま解放しない
		struct ProfilerState
		{
			std::mutex threadsMutex;
			std::vector<std::unique_ptr<ThreadBuffer>> threads;

			std::vector<ProfileFrame> history;
			ProfileFrame current;
			bool paused = false;

			size_t captureFrames = 0;
			std::string capturePath;
		};

		ProfilerState& State()
		{
			static ProfilerState* state = new ProfilerState();
			return *state;
		}

		// thread_local … スレッドごとに別々の値を持つ変数。各スレッドが自分の ThreadBuffer を指す
		// 実体（unique_ptr）は State().threads が持ち、スレッドが終わっても消さない（記録を後から見られるように）
		thread_local ThreadBuffer* tBuffer = nullptr;

		// 今のスレッドの記録を返す。そのスレッドで初めて呼ばれたときに作って登録する
		ThreadBuffer& GetBuffer()
		{
			if (!tBuffer)
			{
				ProfilerState& s = State();
				std::lock_guard lock(s.threadsMutex);
				auto buffer = std::make_unique<ThreadBuffer>();
				buffer->index = uint16_t(s.threads.size());
				buffer->name = std::format("Thread {}", GetCurrentThreadId());
				tBuffer = buffer.get();
				s.threads.push_back(std::move(buffer));
			}
			return *tBuffer;
		}

		// JSON の文字列の中では " と \ の前に \ を付ける決まり（付けないと文字列の終わりと読まれる）
		std::string EscapeJson(std::string_view s)
		{
			std::string out;
			out.reserve(s.size());
			for (char c : s)
			{
				if (c == '"' || c == '\\') { out += '\\'; }
				out += c;
			}
			return out;
		}

		// 並べ替えの順: 始まりの早い順。同時に始まったら外側（depth が小さい）を先にする
		bool EarlierEvent(const ProfileEvent& a, const ProfileEvent& b)
		{
			return a.start != b.start ? a.start < b.start : a.depth < b.depth;
		}

		// 集計の途中の値（合計・最大・呼ばれた回数）
		struct StatAccum
		{
			const char* name;
			bool gpu;
			double total = 0.0;
			double max = 0.0;
			double calls = 0.0;
		};

		void AddFrameStat(std::unordered_map<std::string_view, StatAccum>& stats, std::string_view name, double ms)
		{
			auto& a = stats.at(name);
			a.total += ms;
			a.max = std::max(a.max, ms);
		}
	}

	double ProfileFrame::GetMs() const { return Profiler::TicksToMs(end - start); }

	// 前のフレームを締めて履歴へ入れ、新しいフレームを始める
	//  1. 各スレッドの「閉じた区間」を集める（ワーカーの区間もここで今のフレームに入る）
	//  2. 開始時刻の順に並べ替える
	//  3. 履歴へ入れる（kHistory を超えたら一番古いものを捨てる）
	//  4. 自動保存（-profile）の数を減らし、0 になったら保存する
	void Profiler::BeginFrame()
	{
		ProfilerState& s = State();
		int64_t now = Now();
		if (s.current.start != 0)
		{
			s.current.end = now;
			{
				std::lock_guard lock(s.threadsMutex);
				for (auto& t : s.threads)
				{
					std::lock_guard tlock(t->mutex);
					s.current.events.insert(s.current.events.end(), t->done.begin(), t->done.end());
					t->done.clear();
				}
			}
			std::sort(s.current.events.begin(), s.current.events.end(), EarlierEvent);
			if (!s.paused)
			{
				if (s.history.size() >= kHistory) { s.history.erase(s.history.begin()); }
				s.history.push_back(std::move(s.current));
			}

			if (s.captureFrames > 0 && --s.captureFrames == 0)
			{
				// GPU の結果は数フレーム遅れて届くので、最後の数フレームは GPU が空になる
				if (SaveTrace(s.capturePath))
				{
					Log::Info(LogCat::Core, "Profile saved: {} (open in ui.perfetto.dev or chrome://tracing)", s.capturePath);
				}
			}
		}
		uint64_t number = s.current.number + 1;
		s.current = {};
		s.current.number = number;
		s.current.start = now;
	}

	uint64_t Profiler::GetFrameNumber() { return State().current.number; }

	void Profiler::BeginScope(const char* name)
	{
		GetBuffer().stack.push_back({ name, Now() });
	}

	// スタックの一番上（一番最後に始まった区間）を閉じる。depth は取り出した後のスタックの深さ
	// 閉じた区間はメインスレッドが BeginFrame で集めるので、done へ入れるときだけロックする
	void Profiler::EndScope()
	{
		ThreadBuffer& b = GetBuffer();
		if (b.stack.empty()) { return; }
		OpenScope open = b.stack.back();
		b.stack.pop_back();
		ProfileEvent e{ open.name, open.start, Now(), b.index, uint16_t(b.stack.size()) };
		std::lock_guard lock(b.mutex);
		b.done.push_back(e);
	}

	void Profiler::SetThreadName(const char* name)
	{
		ThreadBuffer& b = GetBuffer();
		std::lock_guard lock(State().threadsMutex);
		b.name = name;
	}

	// 数フレーム遅れて届いた GPU の結果を、計測したときのフレームに入れる
	// 新しいフレームほど見つかりやすいので、履歴の後ろから探す（rbegin / rend は逆向きのイテレータ）
	// GPU の時間は、一番早い始まりから一番遅い終わりまで
	void Profiler::SubmitGpu(uint64_t cpuFrame, std::vector<GpuProfileEvent> events)
	{
		ProfilerState& s = State();
		for (auto it = s.history.rbegin(); it != s.history.rend(); ++it)
		{
			if (it->number != cpuFrame) { continue; }
			double begin = 1.0e30, end = 0.0;
			for (auto& e : events)
			{
				begin = std::min(begin, e.startMs);
				end = std::max(end, e.endMs);
			}
			it->gpuMs = events.empty() ? 0.0 : end - begin;
			it->gpu = std::move(events);
			return;
		}
	}

	void Profiler::SetPaused(bool paused) { State().paused = paused; }
	bool Profiler::IsPaused() { return State().paused; }
	const std::vector<ProfileFrame>& Profiler::GetFrames() { return State().history; }

	std::vector<std::string> Profiler::GetThreadNames()
	{
		ProfilerState& s = State();
		std::lock_guard lock(s.threadsMutex);
		std::vector<std::string> names;
		for (auto& t : s.threads) { names.push_back(t->name); }
		return names;
	}

	// ティック ÷ 1 秒あたりのティック数 = 秒。ミリ秒にするので 1000 倍する
	// 1 秒あたりのティック数は起動中に変わらないので、最初の 1 回だけ聞いて覚えておく
	double Profiler::TicksToMs(int64_t ticks)
	{
		static const int64_t frequency = QueryFrequency();
		return double(ticks) * 1000.0 / double(frequency);
	}

	// フレームごとに区間名ごとの合計を出し、その平均と最大を求める
	//  同じ文字列でも翻訳単位が違うとポインタが変わるので、ポインタではなく中身で集計する
	//  （string_view をキーにすると、文字列をコピーせずに中身で比べられる）
	//  1 フレームに同じ区間が何回もある（ループの中など）ので、まずフレームごとに足し、その合計で平均と最大を出す
	//  GPU は結果が届いていないフレームがあるので、届いたフレームの数（gpuFrames）で割る
	std::vector<ProfileStat> Profiler::ComputeStats(size_t frameCount)
	{
		const auto& frames = State().history;
		size_t first = frames.size() > frameCount ? frames.size() - frameCount : 0;
		size_t n = frames.size() - first;
		if (n == 0) { return {}; }

		std::unordered_map<std::string_view, StatAccum> cpu, gpu;
		size_t gpuFrames = 0;
		for (size_t i = first; i < frames.size(); ++i)
		{
			std::unordered_map<std::string_view, double> frameCpu, frameGpu;
			for (auto& e : frames[i].events)
			{
				frameCpu[e.name] += TicksToMs(e.end - e.start);
				cpu.try_emplace(e.name, StatAccum{ e.name, false }).first->second.calls += 1.0;
			}
			for (auto& e : frames[i].gpu)
			{
				frameGpu[e.name] += e.endMs - e.startMs;
				gpu.try_emplace(e.name, StatAccum{ e.name, true }).first->second.calls += 1.0;
			}
			for (auto& [name, ms] : frameCpu) { AddFrameStat(cpu, name, ms); }
			for (auto& [name, ms] : frameGpu) { AddFrameStat(gpu, name, ms); }
			if (!frames[i].gpu.empty()) { ++gpuFrames; }
		}

		std::vector<ProfileStat> stats;
		for (auto& [name, a] : cpu) { stats.push_back({ a.name, a.total / double(n), a.max, a.calls / double(n), false }); }
		double gpuCount = double(std::max<size_t>(gpuFrames, 1));
		for (auto& [name, a] : gpu) { stats.push_back({ a.name, a.total / gpuCount, a.max, a.calls / gpuCount, true }); }
		std::sort(stats.begin(), stats.end(), [](const ProfileStat& a, const ProfileStat& b) { return a.avgMs > b.avgMs; });
		return stats;
	}

	// Chrome のトレース形式（Trace Event Format）の JSON で保存する
	//  { "traceEvents": [ イベント, イベント, ... ] } の形で、1 つのイベントは 1 行
	//    "ph":"M" … メタデータ。tid（スレッドの番号）に表示する名前を付ける
	//    "ph":"X" … 長さのある区間。ts が始まり、dur が長さ（どちらもマイクロ秒）
	//  時刻は最初のフレームの始まり（origin）を 0 にする。GPU とフレームの帯は、
	//  実際のスレッドと番号がぶつからない tid（10000・10001）に置く
	//  最後の "{}" は、直前の行の末尾の , のせいで JSON が壊れないようにするための空のイベント
	//  std::format の中で { や } を文字として書くときは {{ と }} にする
	bool Profiler::SaveTrace(const std::string& path, size_t frameCount)
	{
		const auto& frames = State().history;
		if (frames.empty()) { return false; }
		std::error_code ec;
		std::filesystem::path p(ToWide(path));
		if (p.has_parent_path()) { std::filesystem::create_directories(p.parent_path(), ec); }
		std::ofstream out(p, std::ios::trunc);
		if (!out)
		{
			Log::Error(LogCat::Core, "Failed to write profile: {}", path);
			return false;
		}

		size_t first = frames.size() > frameCount ? frames.size() - frameCount : 0;
		int64_t origin = frames[first].start;
		auto us = [&](int64_t t) { return TicksToMs(t - origin) * 1000.0; };
		constexpr int kGpuTid = 10000;
		constexpr int kFrameTid = 10001;

		out << "{\"traceEvents\":[\n";
		auto names = GetThreadNames();
		for (size_t i = 0; i < names.size(); ++i)
		{
			out << std::format("{{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":{},\"args\":{{\"name\":\"{}\"}}}},\n", i, EscapeJson(names[i]));
		}
		out << std::format("{{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":{},\"args\":{{\"name\":\"GPU\"}}}},\n", kGpuTid);
		out << std::format("{{\"ph\":\"M\",\"name\":\"thread_name\",\"pid\":1,\"tid\":{},\"args\":{{\"name\":\"Frames\"}}}},\n", kFrameTid);

		for (size_t i = first; i < frames.size(); ++i)
		{
			const ProfileFrame& f = frames[i];
			out << std::format("{{\"ph\":\"X\",\"name\":\"Frame {}\",\"pid\":1,\"tid\":{},\"ts\":{:.3f},\"dur\":{:.3f}}},\n",
				f.number, kFrameTid, us(f.start), TicksToMs(f.end - f.start) * 1000.0);
			for (const auto& e : f.events)
			{
				out << std::format("{{\"ph\":\"X\",\"name\":\"{}\",\"pid\":1,\"tid\":{},\"ts\":{:.3f},\"dur\":{:.3f}}},\n",
					EscapeJson(e.name), e.thread, us(e.start), TicksToMs(e.end - e.start) * 1000.0);
			}
			// GPU の時刻は CPU と別の時計なので、フレームの開始にそろえて並べる
			for (const auto& g : f.gpu)
			{
				out << std::format("{{\"ph\":\"X\",\"name\":\"{}\",\"pid\":1,\"tid\":{},\"ts\":{:.3f},\"dur\":{:.3f}}},\n",
					EscapeJson(g.name), kGpuTid, us(f.start) + g.startMs * 1000.0, (g.endMs - g.startMs) * 1000.0);
			}
		}
		out << "{}\n]}\n";
		return bool(out);
	}

	// 履歴より多くは残せないので、kHistory で頭打ちにする
	void Profiler::RequestCapture(size_t frameCount, std::string path)
	{
		ProfilerState& s = State();
		s.captureFrames = std::min(frameCount, kHistory);
		s.capturePath = std::move(path);
	}
}
