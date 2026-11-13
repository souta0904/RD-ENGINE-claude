#include "RdPch.h"
#include "Core/Profiler.h"
#include "Core/StringUtil.h"
#include <fstream>
#include <unordered_map>

namespace rd
{
	namespace
	{
		int64_t Now()
		{
			LARGE_INTEGER t;
			QueryPerformanceCounter(&t);
			return t.QuadPart;
		}

		int64_t Frequency()
		{
			static const int64_t freq = []
				{
					LARGE_INTEGER f;
					QueryPerformanceFrequency(&f);
					return f.QuadPart;
				}();
			return freq;
		}

		// スレッドごとの記録
		//  開いている区間のスタックは持ち主のスレッドしか触らないのでロック不要。
		//  閉じた区間はフレームの区切りでメインスレッドが回収するのでロックする。
		struct ThreadBuffer
		{
			struct Open
			{
				const char* name;
				int64_t start;
			};
			std::vector<Open> stack;
			std::mutex mutex;
			std::vector<ProfileEvent> done;
			std::string name;
			uint16_t index = 0;
		};

		struct ProfilerState
		{
			std::mutex threadsMutex;
			std::vector<std::unique_ptr<ThreadBuffer>> threads;// スレッドが終わっても残す（数は少ない）

			std::vector<ProfileFrame> history;
			ProfileFrame current;
			bool paused = false;

			size_t captureFrames = 0;// 0 なら予約なし
			std::string capturePath;
		};

		ProfilerState& State()
		{
			static ProfilerState* state = new ProfilerState();// 終了処理の順番に左右されないよう解放しない
			return *state;
		}

		thread_local ThreadBuffer* tBuffer = nullptr;

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
	}

	double ProfileFrame::GetMs() const { return Profiler::TicksToMs(end - start); }

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
			std::sort(s.current.events.begin(), s.current.events.end(), [](const ProfileEvent& a, const ProfileEvent& b)
				{
					return a.start != b.start ? a.start < b.start : a.depth < b.depth;
				});
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

	void Profiler::EndScope()
	{
		ThreadBuffer& b = GetBuffer();
		if (b.stack.empty()) { return; }
		ThreadBuffer::Open open = b.stack.back();
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

	double Profiler::TicksToMs(int64_t ticks) { return double(ticks) * 1000.0 / double(Frequency()); }

	std::vector<ProfileStat> Profiler::ComputeStats(size_t frameCount)
	{
		const auto& frames = State().history;
		size_t first = frames.size() > frameCount ? frames.size() - frameCount : 0;
		size_t n = frames.size() - first;
		if (n == 0) { return {}; }

		struct Accum
		{
			const char* name;
			bool gpu;
			double total = 0.0;
			double max = 0.0;
			double calls = 0.0;
		};
		// 同じ文字列でも翻訳単位が違うとポインタが変わるので、中身で集計する
		std::unordered_map<std::string_view, Accum> cpu, gpu;
		size_t gpuFrames = 0;
		for (size_t i = first; i < frames.size(); ++i)
		{
			std::unordered_map<std::string_view, double> frameCpu, frameGpu;
			for (auto& e : frames[i].events)
			{
				frameCpu[e.name] += TicksToMs(e.end - e.start);
				auto& a = cpu.try_emplace(e.name, Accum{ e.name, false }).first->second;
				a.calls += 1.0;
			}
			for (auto& e : frames[i].gpu)
			{
				frameGpu[e.name] += e.endMs - e.startMs;
				auto& a = gpu.try_emplace(e.name, Accum{ e.name, true }).first->second;
				a.calls += 1.0;
			}
			for (auto& [name, ms] : frameCpu)
			{
				auto& a = cpu.at(name);
				a.total += ms;
				a.max = std::max(a.max, ms);
			}
			for (auto& [name, ms] : frameGpu)
			{
				auto& a = gpu.at(name);
				a.total += ms;
				a.max = std::max(a.max, ms);
			}
			if (!frames[i].gpu.empty()) { ++gpuFrames; }
		}

		std::vector<ProfileStat> stats;
		for (auto& [name, a] : cpu) { stats.push_back({ a.name, a.total / double(n), a.max, a.calls / double(n), false }); }
		for (auto& [name, a] : gpu)
		{
			double count = double(std::max<size_t>(gpuFrames, 1));
			stats.push_back({ a.name, a.total / count, a.max, a.calls / count, true });
		}
		std::sort(stats.begin(), stats.end(), [](const ProfileStat& a, const ProfileStat& b) { return a.avgMs > b.avgMs; });
		return stats;
	}

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

	void Profiler::RequestCapture(size_t frameCount, std::string path)
	{
		ProfilerState& s = State();
		s.captureFrames = std::min(frameCount, kHistory);
		s.capturePath = std::move(path);
	}
}
