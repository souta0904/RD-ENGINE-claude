#include "RdPch.h"
#include "Editor/Panels/ProfilerPanel.h"
#include "Core/Profiling/ProfileScope.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Editor/Core/EditorLocale.h"

namespace rd
{
	namespace
	{
		// 名前から色を決める（FNV-1a のハッシュを色相に使うので、同じ区間はいつも同じ色）
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

		bool Contains(ImVec2 mn, ImVec2 mx, ImVec2 p)
		{
			return p.x >= mn.x && p.x < mx.x && p.y >= mn.y && p.y < mx.y;
		}
	}

	ProfilerPanel::ProfilerPanel(Editor& editor) : EditorPanel(editor, "Profiler", false) {}

	void ProfilerPanel::OnGui()
	{
		DrawToolbar();
		const auto& frames = Profiler::GetFrames();
		if (frames.empty())
		{
			EditorGui::EmptyHint(Tr("No frames yet"));
			return;
		}
		DrawFrameGraph();
		if (mSelected >= int(frames.size())) { mSelected = -1; }
		// 最新のフレームは GPU の結果がまだ届いていないので、少し前を既定にする
		int selected = mSelected >= 0 ? mSelected : std::max(0, int(frames.size()) - 4);
		DrawTimeline(frames[size_t(selected)]);
		DrawStatsTable();
	}

	void ProfilerPanel::DrawToolbar()
	{
		EditorGui::BeginToolbar("toolbar");
		bool paused = Profiler::IsPaused();
		if (EditorGui::IconButton("##pause", paused ? Icon::Play : Icon::Pause, Tr(paused ? "Resume recording" : "Pause recording"), paused))
		{
			Profiler::SetPaused(!paused);
			if (paused) { mSelected = -1; }
		}
		ImGui::SameLine();
		if (ImGui::Button(Tr("Save Trace")))
		{
			std::string path = TraceFileName();
			if (Profiler::SaveTrace(path)) { Log::Info(LogCat::Editor, "Profile saved: {} (open in ui.perfetto.dev)", path); }
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", Tr("Chrome trace format. Open in ui.perfetto.dev or chrome://tracing")); }
		EditorGui::ToolbarSeparator();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%s", Tr("Average"));
		ImGui::SameLine();
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 7.0f);
		ImGui::SliderInt("##average", &mAverage, 10, int(Profiler::kHistory), Tr("%d frames"));
		EditorGui::EndToolbar();
	}

	// 棒は CPU のフレーム時間（60 FPS を超えたら赤）、青い線は GPU の時間、白い線は 60 FPS
	void ProfilerPanel::DrawFrameGraph()
	{
		const auto& frames = Profiler::GetFrames();
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float graphHeight = 70.0f;
		ImVec2 origin = ImGui::GetCursorScreenPos();
		float graphWidth = std::max(100.0f, ImGui::GetContentRegionAvail().x);
		ImGui::InvisibleButton("##frames", ImVec2(graphWidth, graphHeight));
		bool graphHovered = ImGui::IsItemHovered();
		dl->AddRectFilled(origin, ImVec2(origin.x + graphWidth, origin.y + graphHeight), ImGui::ColorConvertFloat4ToU32(EditorStyle::kFieldBg));

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
			ImU32 color = ImGui::ColorConvertFloat4ToU32(ms > budget * 1.05 ? EditorStyle::kError : EditorStyle::kSuccess);
			if (int(i) == mSelected) { color = ImGui::ColorConvertFloat4ToU32(EditorStyle::kWarning); }
			dl->AddRectFilled(ImVec2(x, toY(ms)), ImVec2(x + std::max(1.0f, barW - 1.0f), origin.y + graphHeight), color);
			if (frames[i].gpuMs > 0.0)
			{
				float y = toY(frames[i].gpuMs);
				dl->AddLine(ImVec2(x, y), ImVec2(x + barW, y), ImGui::ColorConvertFloat4ToU32(EditorStyle::kAccent), 2.0f);
			}
		}
		dl->AddLine(ImVec2(origin.x, toY(budget)), ImVec2(origin.x + graphWidth, toY(budget)), IM_COL32(255, 255, 255, 60));
		if (graphHovered)
		{
			int index = int((ImGui::GetIO().MousePos.x - origin.x) / barW) - int(offset);
			if (index >= 0 && index < int(frames.size()))
			{
				ImGui::SetTooltip(Tr("Frame %llu\nCPU %.2f ms\nGPU %.2f ms"), (unsigned long long)frames[index].number,
					frames[index].GetMs(), frames[index].gpuMs);
				if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
				{
					mSelected = index;
					Profiler::SetPaused(true);
				}
			}
		}
		ImGui::TextDisabled("%s", Tr("Bars: CPU frame time   Blue line: GPU time   White line: 60 FPS   Click a bar to inspect it"));
	}

	// スレッドごとに、区間を入れ子の深さの段に並べて描く。GPU は最後の段
	void ProfilerPanel::DrawTimeline(const ProfileFrame& frame)
	{
		ImGui::SeparatorText(EditorLocale::Format("Frame {}  CPU {:.2f} ms  GPU {:.2f} ms", frame.number, frame.GetMs(), frame.gpuMs).c_str());

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
		if (ImGui::IsWindowHovered() && ImGui::GetIO().KeyCtrl && ImGui::GetIO().MouseWheel != 0.0f)
		{
			mZoom = std::clamp(mZoom * (ImGui::GetIO().MouseWheel > 0 ? 1.25f : 0.8f), 1.0f, 64.0f);
		}
		ImDrawList* dl = ImGui::GetWindowDrawList();
		float width = (ImGui::GetContentRegionAvail().x - labelW) * mZoom;
		double frameMs = std::max(std::max(frame.GetMs(), frame.gpuMs), 0.001);
		ImVec2 start = ImGui::GetCursorScreenPos();
		float y = start.y;
		float x0 = start.x + labelW;
		const ProfileEvent* hovered = nullptr;
		const GpuProfileEvent* hoveredGpu = nullptr;
		ImVec2 mouse = ImGui::GetIO().MousePos;
		bool windowHovered = ImGui::IsWindowHovered();

		for (size_t t = 0; t < threadNames.size(); ++t)
		{
			if (!used[t]) { continue; }
			dl->AddText(ImVec2(start.x, y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), threadNames[t].c_str());
			for (auto& e : frame.events)
			{
				if (e.thread != t) { continue; }
				float a = x0 + float(Profiler::TicksToMs(e.start - frame.start) / frameMs) * width;
				float b = x0 + float(Profiler::TicksToMs(e.end - frame.start) / frameMs) * width;
				ImVec2 mn(a, y + e.depth * rowH), mx(std::max(a + 1.0f, b), y + (e.depth + 1) * rowH - 1.0f);
				DrawBar(dl, mn, mx, ScopeColor(e.name, false), e.name);
				if (windowHovered && Contains(mn, mx, mouse)) { hovered = &e; }
			}
			y += std::max<uint16_t>(maxDepth[t], 1) * rowH + 6.0f;
		}
		if (!frame.gpu.empty())
		{
			dl->AddText(ImVec2(start.x, y + 2.0f), ImGui::GetColorU32(ImGuiCol_Text), "GPU");
			for (auto& g : frame.gpu)
			{
				float a = x0 + float(g.startMs / frameMs) * width;
				float b = x0 + float(g.endMs / frameMs) * width;
				ImVec2 mn(a, y + g.depth * rowH), mx(std::max(a + 1.0f, b), y + (g.depth + 1) * rowH - 1.0f);
				DrawBar(dl, mn, mx, ScopeColor(g.name, true), g.name);
				if (windowHovered && Contains(mn, mx, mouse)) { hoveredGpu = &g; }
			}
			y += gpuDepth * rowH + 6.0f;
		}
		ImGui::Dummy(ImVec2(labelW + width, y - start.y));
		if (hovered)
		{
			ImGui::SetTooltip(Tr("%s\n%.3f ms  (at %.3f ms)\nthread: %s"), hovered->name,
				Profiler::TicksToMs(hovered->end - hovered->start), Profiler::TicksToMs(hovered->start - frame.start),
				threadNames[hovered->thread].c_str());
		}
		else if (hoveredGpu)
		{
			ImGui::SetTooltip("%s (GPU)\n%.3f ms", hoveredGpu->name, hoveredGpu->endMs - hoveredGpu->startMs);
		}
		ImGui::EndChild();
		ImGui::TextDisabled("%s", Tr("Ctrl + wheel to zoom the timeline"));
	}

	void ProfilerPanel::DrawStatsTable()
	{
		auto stats = Profiler::ComputeStats(size_t(mAverage));
		if (!ImGui::BeginTable("##stats", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders | ImGuiTableFlags_ScrollY | ImGuiTableFlags_Resizable)) { return; }
		ImGui::TableSetupScrollFreeze(0, 1);
		ImGui::TableSetupColumn(Tr("Scope"), ImGuiTableColumnFlags_WidthStretch);
		ImGui::TableSetupColumn("", ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 2.5f);
		ImGui::TableSetupColumn(Tr("Avg ms"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.5f);
		ImGui::TableSetupColumn(Tr("Max ms"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 4.5f);
		ImGui::TableSetupColumn(Tr("Calls"), ImGuiTableColumnFlags_WidthFixed, ImGui::GetFontSize() * 3.5f);
		ImGui::TableHeadersRow();
		for (const auto& s : stats)
		{
			ImGui::TableNextRow();
			ImGui::TableNextColumn();
			ImGui::TextUnformatted(s.name);
			ImGui::TableNextColumn();
			ImGui::TextColored(s.gpu ? EditorStyle::kAccent : EditorStyle::kSuccess, s.gpu ? "GPU" : "CPU");
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", s.avgMs);
			ImGui::TableNextColumn();
			ImGui::Text("%.3f", s.maxMs);
			ImGui::TableNextColumn();
			ImGui::Text("%.1f", s.calls);
		}
		ImGui::EndTable();
	}
}
