#include "RdPch.h"
#include "Editor/Panels/LogPanel.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Editor/Core/EditorLocale.h"

namespace rd
{
	namespace
	{
		struct LogLine
		{
			const LogRecord* record;
			int count;
		};

		Icon LevelIcon(LogLevel level)
		{
			if (level >= LogLevel::Error) { return Icon::Error; }
			if (level == LogLevel::Warning) { return Icon::Warning; }
			return Icon::Info;
		}

		ImVec4 LevelColor(LogLevel level)
		{
			if (level >= LogLevel::Error) { return EditorStyle::kError; }
			if (level == LogLevel::Warning) { return EditorStyle::kWarning; }
			if (level == LogLevel::Info) { return EditorStyle::kInfo; }
			return EditorStyle::kTextDim;
		}
	}

	LogPanel::LogPanel(Editor& editor) : EditorPanel(editor, "Log") {}

	void LogPanel::Update(float)
	{
		bool playing = mEditor.IsPlaying();
		if (playing && !mWasPlaying && mClearOnPlay) { Log::Clear(); }
		mWasPlaying = playing;
	}

	bool LogPanel::LevelToggle(const char* id, Icon icon, int count, bool& show, const char* tooltip)
	{
		std::string label = std::to_string(count);
		ImGui::PushStyleColor(ImGuiCol_Button, show ? EditorStyle::kHeaderBg : ImVec4(0, 0, 0, 0));
		bool pressed = EditorGui::IconTextButton(id, icon, label.c_str(), tooltip);
		ImGui::PopStyleColor();
		if (pressed) { show = !show; }
		return pressed;
	}

	void LogPanel::DrawToolbar(int infoCount, int warnCount, int errorCount)
	{
		EditorGui::BeginToolbar("toolbar");
		if (ImGui::Button(Tr("Clear"))) { Log::Clear(); }
		ImGui::SameLine();
		ImGui::Checkbox(Tr("Collapse"), &mCollapse);
		ImGui::SameLine();
		ImGui::Checkbox(Tr("Clear on Play"), &mClearOnPlay);
		ImGui::SameLine();
		ImGui::Checkbox(Tr("Debug"), &mShowDebug);
		EditorGui::ToolbarSeparator();
		float right = 220.0f + 3.0f * 64.0f;
		ImGui::SameLine(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowContentRegionMax().x - right));
		EditorGui::SearchField("##filter", mFilter, Tr("Search"), 200.0f);
		ImGui::SameLine();
		LevelToggle("info", Icon::Info, infoCount, mShowInfo, Tr("Show messages"));
		ImGui::SameLine(0.0f, 2.0f);
		LevelToggle("warn", Icon::Warning, warnCount, mShowWarn, Tr("Show warnings"));
		ImGui::SameLine(0.0f, 2.0f);
		LevelToggle("error", Icon::Error, errorCount, mShowError, Tr("Show errors"));
		EditorGui::EndToolbar();
	}

	// ログの履歴はコールバックの間ロックされるので、表示する行をまず集めてから描く
	void LogPanel::OnGui()
	{
		int infoCount = 0, warnCount = 0, errorCount = 0;
		std::vector<LogLine> lines;
		std::vector<LogRecord> copies;
		Log::ForEach([&](const LogRecord& r)
			{
				if (r.level >= LogLevel::Error) { ++errorCount; }
				else if (r.level == LogLevel::Warning) { ++warnCount; }
				else if (r.level == LogLevel::Info) { ++infoCount; }
				bool show = r.level <= LogLevel::Debug ? mShowDebug
					: r.level == LogLevel::Info ? mShowInfo
					: r.level == LogLevel::Warning ? mShowWarn
					: mShowError;
				if (!show) { return; }
				if (!mFilter.empty() && !ContainsNoCase(r.message, mFilter) && !ContainsNoCase(r.category, mFilter)) { return; }
				copies.push_back(r);
			});
		for (const auto& r : copies)
		{
			if (mCollapse)
			{
				bool merged = false;
				for (auto& line : lines)
				{
					if (line.record->level == r.level && line.record->message == r.message) { ++line.count; merged = true; break; }
				}
				if (merged) { continue; }
			}
			lines.push_back({ &r, 1 });
		}

		DrawToolbar(infoCount, warnCount, errorCount);
		ImGui::BeginChild("log", ImVec2(0, 0), ImGuiChildFlags_None, ImGuiWindowFlags_HorizontalScrollbar);
		float iconSize = EditorStyle::IconSize() * 0.9f;
		int row = 0;
		for (const auto& line : lines)
		{
			const LogRecord& e = *line.record;
			ImVec2 p = ImGui::GetCursorScreenPos();
			float h = ImGui::GetTextLineHeightWithSpacing();
			if (row++ & 1) { ImGui::GetWindowDrawList()->AddRectFilled(p, ImVec2(p.x + ImGui::GetContentRegionAvail().x, p.y + h), ImGui::GetColorU32(ImGuiCol_TableRowBgAlt)); }
			EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(p.x, p.y + (h - iconSize) * 0.5f), iconSize, LevelIcon(e.level));
			ImGui::Dummy(ImVec2(iconSize, 0.0f));
			ImGui::SameLine();
			ImGui::TextDisabled("%8.2f %-8s", e.time, e.category);
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, LevelColor(e.level));
			ImGui::TextUnformatted(e.message.c_str());
			ImGui::PopStyleColor();
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s  %s:%u", ToString(e.level), e.location.file_name(), unsigned(e.location.line())); }
			if (line.count > 1)
			{
				ImGui::SameLine();
				ImGui::TextDisabled("(%d)", line.count);
			}
		}
		if (lines.empty()) { EditorGui::EmptyHint(Tr("No messages.")); }
		// 新しいログが来たら一番下へ
		if (Log::GetVersion() != mVersion)
		{
			mVersion = Log::GetVersion();
			ImGui::SetScrollHereY(1.0f);
		}
		ImGui::EndChild();
	}
}
