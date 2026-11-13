#include "RdPch.h"
#include "Editor/Editor.h"
#include "Core/Profiler.h"
#include <ImGui/imgui.h>

namespace rd
{
	namespace
	{
		// 名前から決まる色（同じ区間は毎回同じ色）
		ImU32 ScopeColor(const char* name, bool gpu)
		{
			uint32_t h = 2166136261u;
			for (const char* p = name; *p; ++p) { h = (h ^ uint8_t(*p)) * 16777619u; }
			float hue = float(h % 360) / 360.0f;
			float r, g, b;
			ImGui::ColorConvertHSVtoRGB(hue, gpu ? 0.35f : 0.5f, gpu ? 0.75f : 0.85f, r, g, b);
			return ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
		}

		std::string TraceFileName()
		{
			SYSTEMTIME t;
			GetLocalTime(&t);
			return std::format("Logs/Profile_{:04}{:02}{:02}_{:02}{:02}{:02}.json", t.wYear, t.wMonth, t.wDay, t.wHour, t.wMinute, t.wSecond);
		}

		// 区間の帯を 1 本描く
		void DrawBar(ImDrawList* dl, ImVec2 min, ImVec2 max, ImU32 color, const char* label)
		{
			dl->AddRectFilled(min, max, color);
			dl->AddRect(min, max, IM_COL32(0, 0, 0, 90));
			float width = max.x - min.x;
			ImVec2 size = ImGui::CalcTextSize(label);
			if (width > size.x + 6.0f)
			{
				dl->AddText(ImVec2(min.x + 3.0f, min.y + (max.y - min.y - size.y) * 0.5f), IM_COL32(0, 0, 0, 255), label);
			}
		}
	}

	void Editor::DrawProfiler()
	{
		ImGui::SetNextWindowSize(ImVec2(900.0f, 460.0f), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Profiler", &mShowProfiler))
		{
			ImGui::End();
			return;
		}
		const auto& frames = Profiler::GetFrames();

		// ---------------- 操作 ----------------
		bool paused = Profiler::IsPaused();
		if (ImGui::Checkbox("Pause", &paused))
		{
			Profiler::SetPaused(paused);
			if (!paused) { mProfilerSelected = -1; }
		}
		ImGui::SameLine();
		if (ImGui::Button("Save Trace"))
		{
			std::string path = TraceFileName();
			if (Profiler::SaveTrace(path)) { Log::Info(LogCat::Editor, "Profile saved: {} (open in ui.perfetto.dev)", path); }
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Chrome trace format. Open in ui.perfetto.dev or chrome://tracing"); }
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		ImGui::SliderInt("Average frames", &mProfilerAverage, 10, int(Profiler::kHistory));
		if (frames.empty())
		{
			ImGui::TextDisabled("No frames yet");
			ImGui::End();
			return;
		}

		// ---------------- フレームのグラフ（クリックで選択） ----------------
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float graphHeight = 70.0f;
		ImVec2 origin = ImGui::GetCursorScreenPos();
		float graphWidth = std::max(100.0f, ImGui::GetContentRegionAvail().x);
		ImGui::InvisibleButton("##frames", ImVec2(graphWidth, graphHeight));
		bool graphHovered = ImGui::IsItemHovered();
		dl->AddRectFilled(origin, ImVec2(origin.x + graphWidth, origin.y + graphHeight), IM_COL32(25, 25, 28, 255));

		const double budget = 1000.0 / 60.0;
		double maxMs = budget * 1.5;
		for (auto& f : frames) { maxMs = std::max(maxMs, std::max(f.GetMs(), f.gpuMs)); }
		float barW = graphWidth / float(Profiler::kHistory);
		size_t offset = Profiler::kHistory - frames.size();
		auto toY = [&](double ms) { return origin.y + graphHeight - float(ms / maxMs) * graphHeight; };
		for (size_t i = 0; i < frames.size(); ++i)
		{
			float x = origin.x + float(i + offset) * barW;
			double ms = frames[i].GetMs();
			ImU32 color = ms > budget * 1.05 ? IM_COL32(220, 90, 70, 255) : IM_COL32(90, 170, 110, 255);
			if (int(i) == mProfilerSelected) { color = IM_COL32(240, 220, 90, 255); }
			dl->AddRectFilled(ImVec2(x, toY(ms)), ImVec2(x + std::max(1.0f, barW - 1.0f), origin.y + graphHeight), color);
			if (frames[i].gpuMs > 0.0)
			{
				float y = toY(frames[i].gpuMs);
				dl->AddLine(ImVec2(x, y), ImVec2(x + barW, y), IM_COL32(110, 160, 255, 255), 2.0f);
			}
		}
		// 60 FPS の線
		dl->AddLine(ImVec2(origin.x, toY(budget)), ImVec2(origin.x + graphWidth, toY(budget)), IM_COL32(255, 255, 255, 60));
		if (graphHovered)
		{
			int index = int((ImGui::GetIO().MousePos.x - origin.x) / barW) - int(offset);
			if (index >= 0 && index < int(frames.size()))
			{
				ImGui::SetTooltip("Frame %llu\nCPU %.2f ms\nGPU %.2f ms", (unsigned long long)frames[index].number,
					frames[index].GetMs(), frames[index].gpuMs);
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					// 選んだフレームを見ている間に流れないよう止める
					mProfilerSelected = index;
					Profiler::SetPaused(true);
				}
			}
		}
		ImGui::TextDisabled("Bars: CPU frame time   Blue line: GPU time   White line: 60 FPS   Click a bar to inspect it");

		// ---------------- 選んだフレームのタイムライン ----------------
		if (mProfilerSelected >= int(frames.size())) { mProfilerSelected = -1; }
		// 最新のフレームは GPU の結果がまだ届いていないので、1 つ手前を既定にする
		int selected = mProfilerSelected >= 0 ? mProfilerSelected : std::max(0, int(frames.size()) - 4);
		const ProfileFrame& frame = frames[size_t(selected)];
		ImGui::SeparatorText(std::format("Frame {}  CPU {:.2f} ms  GPU {:.2f} ms", frame.number, frame.GetMs(), frame.gpuMs).c_str());

		auto threadNames = Profiler::GetThreadNames();
		uint16_t maxDepth[256] = {};
		std::vector<bool> used(threadNames.size(), false);
		for (auto& e : frame.events)
		{
			if (e.thread < used.size()) { used[e.thread] = true; }
			if (e.thread < 256) { maxDepth[e.thread] = std::max<uint16_t>(maxDepth[e.thread], uint16_t(e.depth + 1)); }
		}
		uint16_t gpuDepth = 0;
		for (auto& g : frame.gpu) { gpuDepth = std::max<uint16_t>(gpuDepth, uint16_t(g.depth + 1)); }

		const float rowH = ImGui::GetTextLineHeight() + 4.0f;
		const float labelW = 90.0f;
		ImGui::BeginChild("##timeline", ImVec2(0, std::min(320.0f, ImGui::GetContentRegionAvail().y * 0.55f)), ImGuiChildFlags_Border,
			ImGuiWindowFlags_HorizontalScrollbar);
		{
			// Ctrl + ホイールで横に拡大
			if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
			{
				mProfilerZoom = std::clamp(mProfilerZoom * (ImGui::GetIO().MouseWheel > 0 ? 1.25f : 0.8f), 1.0f, 64.0f);
			}
			ImDrawList* tdl = ImGui::GetWindowDrawList();
			float width = (ImGui::GetContentRegionAvail().x - labelW) * mProfilerZoom;
			double frameMs = std::max(frame.GetMs(), frame.gpuMs);
			frameMs = std::max(frameMs, 0.001);
			ImVec2 start = ImGui::GetCursorScreenPos();
			float y = start.y;
			float x0 = start.x + labelW;
			const ProfileEvent* hovered = nullptr;
			const GpuProfileEvent* hoveredGpu = nullptr;
			ImVec2 mouse = ImGui::GetIO().MousePos;

			for (size_t t = 0; t < threadNames.size(); ++t)
			{
				if (!used[t]) { continue; }
				tdl->AddText(ImVec2(start.x, y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), threadNames[t].c_str());
				for (auto& e : frame.events)
				{
					if (e.thread != t) { continue; }
					float a = x0 + float(Profiler::TicksToMs(e.start - frame.start) / frameMs) * width;
					float b = x0 + float(Profiler::TicksToMs(e.end - frame.start) / frameMs) * width;
					ImVec2 mn(a, y + e.depth * rowH), mx(std::max(a + 1.0f, b), y + (e.depth + 1) * rowH - 1.0f);
					DrawBar(tdl, mn, mx, ScopeColor(e.name, false), e.name);
					if (ImGui::IsWindowHovered() && mouse.x >= mn.x && mouse.x < mx.x && mouse.y >= mn.y && mouse.y < mx.y) { hovered = &e; }
				}
				y += std::max<uint16_t>(maxDepth[t], 1) * rowH + 6.0f;
			}
			if (!frame.gpu.empty())
			{
				tdl->AddText(ImVec2(start.x, y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), "GPU");
				for (auto& g : frame.gpu)
				{
					float a = x0 + float(g.startMs / frameMs) * width;
					float b = x0 + float(g.endMs / frameMs) * width;
					ImVec2 mn(a, y + g.depth * rowH), mx(std::max(a + 1.0f, b), y + (g.depth + 1) * rowH - 1.0f);
					DrawBar(tdl, mn, mx, ScopeColor(g.name, true), g.name);
					if (ImGui::IsWindowHovered() && mouse.x >= mn.x && mouse.x < mx.x && mouse.y >= mn.y && mouse.y < mx.y) { hoveredGpu = &g; }
				}
				y += gpuDepth * rowH + 6.0f;
			}
			ImGui::Dummy(ImVec2(labelW + width, y - start.y));
			if (hovered)
			{
				ImGui::SetTooltip("%s\n%.3f ms  (at %.3f ms)\nthread: %s", hovered->name,
					Profiler::TicksToMs(hovered->end - hovered->start), Profiler::TicksToMs(hovered->start - frame.start),
					threadNames[hovered->thread].c_str());
			}
			else if (hoveredGpu)
			{
				ImGui::SetTooltip("%s (GPU)\n%.3f ms", hoveredGpu->name, hoveredGpu->endMs - hoveredGpu->startMs);
			}
		}
		ImGui::EndChild();
		ImGui::TextDisabled("Ctrl + wheel to zoom the timeline");

		// ---------------- 集計 ----------------
		auto stats = Profiler::ComputeStats(size_t(mProfilerAverage));
		if (ImGui::BeginTable("##stats", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable))
		{
			ImGui::TableSetupScrollFreeze(0, 1);
			ImGui::TableSetupColumn("Scope", ImGuiTableColumnFlags_WidthStretch);
			ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, 34.0f);
			ImGui::TableSetupColumn("Avg ms", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableSetupColumn("Max ms", ImGuiTableColumnFlags_WidthFixed, 64.0f);
			ImGui::TableSetupColumn("Calls", ImGuiTableColumnFlags_WidthFixed, 52.0f);
			ImGui::TableHeadersRow();
			for (const auto& s : stats)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(s.name);
				ImGui::TableNextColumn();
				ImGui::TextColored(s.gpu ? ImVec4(0.45f, 0.65f, 1.0f, 1.0f) : ImVec4(0.6f, 0.85f, 0.6f, 1.0f), s.gpu ? "GPU" : "CPU");
				ImGui::TableNextColumn();
				ImGui::Text("%.3f", s.avgMs);
				ImGui::TableNextColumn();
				ImGui::Text("%.3f", s.maxMs);
				ImGui::TableNextColumn();
				ImGui::Text("%.1f", s.calls);
			}
			ImGui::EndTable();
		}
		ImGui::End();
	}
}
