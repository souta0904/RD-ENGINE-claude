#include "RdPch.h"
#include "Editor/Panels/RenderGraphPanel.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Render/Renderer/Renderer.h"
#include <ImGui/imgui_node_editor.h>
#include <algorithm>

namespace ed = ax::NodeEditor;

namespace rd
{
	namespace
	{
		// 番号の作り方: ノード = (パス+1)*256、入口 = ノード + 1 + 使い方の番号*2、出口 = ノード + 2 + 使い方の番号*2、
		//  つなぎ線 = ノード + 200 + 使い方の番号（線は読む側のパスと使い方で決まる）
		constexpr uint64_t kStride = 256;
		ed::NodeId NodeKey(size_t pass) { return ed::NodeId((pass + 1) * kStride); }
		ed::PinId InPin(size_t pass, size_t use) { return ed::PinId((pass + 1) * kStride + 1 + use * 2); }
		ed::PinId OutPin(size_t pass, size_t use) { return ed::PinId((pass + 1) * kStride + 2 + use * 2); }
		ed::LinkId LinkKey(size_t pass, size_t use) { return ed::LinkId((pass + 1) * kStride + 200 + use); }

		ImU32 ToU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
		ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }

		bool IsWrite(rg::Access a) { return a == rg::Access::RenderTarget || a == rg::Access::DepthWrite; }
		// 左に出す（前の中身を使う）: 読む・深度を比べる・コピー元・上に描く
		bool IsInput(const rg::GraphInfo::Use& u) { return !IsWrite(u.access) || !u.overwrite; }

		const ImVec4 kDepthColor(0.45f, 0.70f, 1.00f, 1.0f);
		const ImVec4 kImportColor(0.95f, 0.66f, 0.36f, 1.0f);

		ImVec4 ResourceColor(const rg::GraphInfo::Resource& r)
		{
			if (r.imported) { return kImportColor; }
			return r.depth ? kDepthColor : EditorStyle::kAccent;
		}

		const char* FormatName(DXGI_FORMAT f)
		{
			switch (f)
			{
			case DXGI_FORMAT_R16G16B16A16_FLOAT: return "RGBA16F";
			case DXGI_FORMAT_R11G11B10_FLOAT: return "R11G11B10F";
			case DXGI_FORMAT_R8G8B8A8_TYPELESS:
			case DXGI_FORMAT_R8G8B8A8_UNORM: return "RGBA8";
			case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB: return "RGBA8 sRGB";
			case DXGI_FORMAT_R8_UNORM: return "R8";
			case DXGI_FORMAT_D32_FLOAT:
			case DXGI_FORMAT_R32_TYPELESS: return "D32F";
			default: return "?";
			}
		}

		// pass より前で、resource に書いた最後のパス（なければ -1）
		int FindWriter(const rg::GraphInfo& info, size_t pass, uint32_t resource, size_t* useIndex)
		{
			for (size_t p = pass; p-- > 0;)
			{
				const auto& uses = info.passes[p].uses;
				for (size_t u = 0; u < uses.size(); ++u)
				{
					if (uses[u].resource == resource && IsWrite(uses[u].access))
					{
						*useIndex = u;
						return int(p);
					}
				}
			}
			return -1;
		}
	}

	RenderGraphPanel::RenderGraphPanel(Editor& editor)
		: EditorPanel(editor, "Render Graph", false)
	{
		ed::Config config;
		config.SettingsName = nullptr;       // 並べ方は毎回決めるので保存しない
		config.RasterizeTextForZoom = false;
		mContext = ed::CreateEditor(&config);
		ed::SetCurrentEditor(mContext);
		ed::Style& style = ed::GetStyle();
		style.NodePadding = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.NodeRounding = 6.0f;
		style.NodeBorderWidth = 1.0f;
		style.HoveredNodeBorderWidth = 2.0f;
		style.SelectedNodeBorderWidth = 2.5f;
		style.LinkStrength = 80.0f;
		style.Colors[ed::StyleColor_Bg] = EditorStyle::kBaseBg;
		style.Colors[ed::StyleColor_Grid] = ImVec4(1.0f, 1.0f, 1.0f, 0.035f);
		style.Colors[ed::StyleColor_NodeBg] = EditorStyle::kWindowBg;
		style.Colors[ed::StyleColor_NodeBorder] = ImVec4(1.0f, 1.0f, 1.0f, 0.10f);
		style.Colors[ed::StyleColor_HovNodeBorder] = WithAlpha(EditorStyle::kAccentHover, 0.8f);
		style.Colors[ed::StyleColor_SelNodeBorder] = EditorStyle::kAccent;
		style.Colors[ed::StyleColor_NodeSelRect] = EditorStyle::kSelection;
		style.Colors[ed::StyleColor_NodeSelRectBorder] = WithAlpha(EditorStyle::kAccent, 0.6f);
		style.Colors[ed::StyleColor_HovLinkBorder] = EditorStyle::kAccentHover;
		style.Colors[ed::StyleColor_SelLinkBorder] = EditorStyle::kAccent;
		ed::SetCurrentEditor(nullptr);
	}

	RenderGraphPanel::~RenderGraphPanel()
	{
		if (mContext) { ed::DestroyEditor(mContext); }
	}

	void RenderGraphPanel::OnGui()
	{
		// 描くたびに作り直されるので、写しを持って使う
		const rg::GraphInfo info = Renderer::Get().GetGraphInfo();
		if (ImGui::IsWindowAppearing() && mNavigateFrames == 0) { mNavigateFrames = 2; }
		DrawToolbar(info);
		if (info.passes.empty())
		{
			EditorGui::EmptyState(Icon::Info, Tr("Nothing has been rendered yet"));
			return;
		}
		// パスの並びや飛ばしたものが変わったら並べ直す（Render Path の切り替え・ブルームのオンオフなど）
		std::string shape;
		for (const auto& p : info.passes) { shape += p.name + (p.culled ? "-" : "+") + ";"; }
		if (shape != mShape)
		{
			mShape = shape;
			Layout(info);
			mPushPositions = true;
			mNavigateFrames = 2;
			mSelectedPass = -1;
		}
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float sideWidth = std::min(std::max(280.0f, ImGui::GetFontSize() * 22.0f), avail.x * 0.45f);
		DrawCanvas(info, std::max(50.0f, avail.x - sideWidth), avail.y);
		ImGui::SameLine(0.0f, 0.0f);
		DrawSidebar(info, sideWidth, avail.y);
	}

	void RenderGraphPanel::DrawToolbar(const rg::GraphInfo& info)
	{
		EditorGui::BeginToolbar("##rgtoolbar");
		if (EditorGui::IconTextButton("##fit", Icon::Search, Tr("Frame All"), "F")) { mNavigateFrames = 1; }
		EditorGui::ToolbarSeparator();
		ImGui::SameLine();
		RenderSettings& settings = Renderer::Get().GetSettings();
		ImGui::TextUnformatted(Tr("Render Path"));
		ImGui::SameLine();
		if (EditorGui::ToggleButton(Tr("Forward"), settings.path == RenderPath::Forward)) { settings.path = RenderPath::Forward; }
		ImGui::SameLine(0.0f, 2.0f);
		if (EditorGui::ToggleButton(Tr("Deferred"), settings.path == RenderPath::Deferred)) { settings.path = RenderPath::Deferred; }
		EditorGui::ToolbarSeparator();
		ImGui::SameLine();
		size_t culled = std::count_if(info.passes.begin(), info.passes.end(), [](auto& p) { return p.culled; });
		size_t transient = std::count_if(info.resources.begin(), info.resources.end(), [](auto& r) { return !r.imported && r.physical >= 0; });
		ImGui::TextColored(EditorStyle::kTextDim, "%s", EditorLocale::Format("{} passes ({} skipped)   {} textures in {} allocations",
			info.passes.size() - culled, culled, transient, info.physicalCount).c_str());
		EditorGui::EndToolbar();
	}

	// 列 = 頼っているパスの深さ（読むテクスチャを書いたパスの 1 つ右）。同じ列は上から登録した順に並べる
	void RenderGraphPanel::Layout(const rg::GraphInfo& info)
	{
		const float fs = ImGui::GetFontSize();
		std::vector<int> column(info.passes.size(), 0);
		for (size_t p = 0; p < info.passes.size(); ++p)
		{
			const auto& uses = info.passes[p].uses;
			for (size_t u = 0; u < uses.size(); ++u)
			{
				if (!IsInput(uses[u])) { continue; }
				size_t wu = 0;
				int writer = FindWriter(info, p, uses[u].resource, &wu);
				if (writer >= 0) { column[p] = std::max(column[p], column[writer] + 1); }
			}
		}
		// 横に長くなりすぎないよう、kColumnsPerRow 列ごとに折り返して下の段へ続ける
		constexpr int kColumnsPerRow = 6;
		std::vector<float> stack(info.passes.size() + 1, 0.0f);
		std::vector<float> height(info.passes.size(), 0.0f);
		for (size_t p = 0; p < info.passes.size(); ++p)
		{
			// 高さはテクスチャの行の数でおおよそ決まる
			height[p] = fs * (3.5f + 1.4f * float(info.passes[p].uses.size()));
		}
		int maxColumn = 0;
		for (int c : column) { maxColumn = std::max(maxColumn, c); }
		std::vector<float> rowTop(size_t(maxColumn / kColumnsPerRow + 2), 0.0f);
		for (size_t p = 0; p < info.passes.size(); ++p)
		{
			int row = column[p] / kColumnsPerRow;
			stack[column[p]] += height[p];
			rowTop[row + 1] = std::max(rowTop[row + 1], stack[column[p]] + fs * 3.0f);
		}
		for (size_t r = 1; r < rowTop.size(); ++r) { rowTop[r] += rowTop[r - 1]; }
		std::fill(stack.begin(), stack.end(), 0.0f);
		mPositions.assign(info.passes.size(), ImVec2(0.0f, 0.0f));
		for (size_t p = 0; p < info.passes.size(); ++p)
		{
			int c = column[p];
			int row = c / kColumnsPerRow;
			mPositions[p] = ImVec2(float(c % kColumnsPerRow) * fs * 15.0f, rowTop[row] + stack[c]);
			stack[c] += height[p];
		}
	}

	void RenderGraphPanel::DrawCanvas(const rg::GraphInfo& info, float width, float height)
	{
		ed::SetCurrentEditor(mContext);
		ed::Begin("##RenderGraph", ImVec2(width, height));
		const float fs = ImGui::GetFontSize();
		const float nodeWidth = fs * 12.0f;
		const float pad = fs * 0.55f;
		int hoveredResource = -1;

		for (size_t p = 0; p < info.passes.size(); ++p)
		{
			const auto& pass = info.passes[p];
			if (mPushPositions && p < mPositions.size()) { ed::SetNodePosition(NodeKey(p), mPositions[p]); }
			const float alpha = pass.culled ? 0.35f : 1.0f;
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
			ed::BeginNode(NodeKey(p));
			ImGui::PushID(int(p));
			const ImVec2 origin = ImGui::GetCursorScreenPos();
			ImDrawList* dl = ImGui::GetWindowDrawList();

			ImGui::Dummy(ImVec2(nodeWidth, pad * 0.6f));
			ImGui::SetCursorScreenPos(ImVec2(origin.x + pad, ImGui::GetCursorScreenPos().y));
			ImGui::TextUnformatted(pass.name.c_str());
			if (pass.culled)
			{
				ImGui::SetCursorScreenPos(ImVec2(origin.x + pad, ImGui::GetCursorScreenPos().y));
				ImGui::TextColored(EditorStyle::kTextDim, "%s", Tr("(skipped)"));
			}
			ImGui::Dummy(ImVec2(nodeWidth, pad * 0.3f));

			// テクスチャ 1 つごとに 1 行: 入口（左）は前の中身を使うもの、出口（右）は書くもの
			for (size_t u = 0; u < pass.uses.size(); ++u)
			{
				const auto& use = pass.uses[u];
				const auto& res = info.resources[use.resource];
				const ImVec4 color = ResourceColor(res);
				const float rowY = ImGui::GetCursorScreenPos().y;
				const float rowH = ImGui::GetTextLineHeight();
				ImGui::Dummy(ImVec2(nodeWidth, rowH));
				if (ImGui::IsItemHovered()) { hoveredResource = int(use.resource); }
				std::string label = res.name;
				if (use.access == rg::Access::DepthRead) { label += std::string(" (") + Tr("test") + ")"; }
				else if (use.access == rg::Access::CopySource) { label += std::string(" (") + Tr("copy") + ")"; }
				else if (IsWrite(use.access) && use.overwrite) { label += std::string(" (") + Tr("clear") + ")"; }
				const bool highlighted = mHoveredResource == int(use.resource);
				dl->AddText(ImVec2(origin.x + pad * 1.6f, rowY), ToU32(highlighted ? EditorStyle::kText : WithAlpha(color, 0.95f)), label.c_str());

				auto pin = [&](ed::PinId id, ed::PinKind kind, float x)
				{
					ImGui::SetCursorScreenPos(ImVec2(x - pad * 0.5f, rowY));
					ed::BeginPin(id, kind);
					ed::PinPivotAlignment(ImVec2(0.5f, 0.5f));
					ed::PinPivotSize(ImVec2(0.0f, 0.0f));
					ImGui::Dummy(ImVec2(pad, rowH));
					ImVec2 c((ImGui::GetItemRectMin().x + ImGui::GetItemRectMax().x) * 0.5f, rowY + rowH * 0.5f);
					dl->AddCircleFilled(c, fs * 0.22f, ToU32(color));
					ed::EndPin();
				};
				if (IsInput(use)) { pin(InPin(p, u), ed::PinKind::Input, origin.x + pad * 0.5f); }
				if (IsWrite(use.access)) { pin(OutPin(p, u), ed::PinKind::Output, origin.x + nodeWidth - pad * 0.5f); }
				ImGui::SetCursorScreenPos(ImVec2(origin.x, rowY + rowH + 2.0f));
			}
			ImGui::Dummy(ImVec2(nodeWidth, pad * 0.6f));
			ImGui::PopID();
			ed::EndNode();
			ImGui::PopStyleVar();

			// 上の色の帯（飛ばしたものは灰色、出力を持つ・飛ばさないものは目立たせる）
			if (ImDrawList* bg = ed::GetNodeBackgroundDrawList(NodeKey(p)))
			{
				ImVec2 pos = ed::GetNodePosition(NodeKey(p));
				ImVec2 size = ed::GetNodeSize(NodeKey(p));
				ImVec4 band = pass.culled ? EditorStyle::kTextDim : pass.sideEffect ? EditorStyle::kWarning : EditorStyle::kAccent;
				bg->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + 3.0f), ToU32(band), 6.0f, ImDrawFlags_RoundCornersTop);
			}
		}
		mPushPositions = false;

		// 前の中身を使う所へ、そのテクスチャを最後に書いたパスからつなぐ
		for (size_t p = 0; p < info.passes.size(); ++p)
		{
			const auto& uses = info.passes[p].uses;
			for (size_t u = 0; u < uses.size(); ++u)
			{
				if (!IsInput(uses[u])) { continue; }
				size_t wu = 0;
				int writer = FindWriter(info, p, uses[u].resource, &wu);
				if (writer < 0) { continue; }
				const auto& res = info.resources[uses[u].resource];
				bool dim = info.passes[p].culled || info.passes[writer].culled;
				bool highlighted = mHoveredResource == int(uses[u].resource);
				ImVec4 color = WithAlpha(ResourceColor(res), dim ? 0.2f : highlighted ? 1.0f : 0.6f);
				ed::Link(LinkKey(p, u), OutPin(size_t(writer), wu), InPin(p, u), color, highlighted ? 3.0f : 2.0f);
			}
		}

		if (mNavigateFrames > 0 && --mNavigateFrames == 0) { ed::NavigateToContent(0.2f); }
		if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F, false) && !ImGui::GetIO().WantTextInput)
		{
			mNavigateFrames = 1;
		}

		ed::NodeId selected[1];
		mSelectedPass = ed::GetSelectedNodes(selected, 1) > 0 ? int(selected[0].Get() / kStride) - 1 : -1;
		ed::End();
		ed::SetCurrentEditor(nullptr);
		if (hoveredResource >= 0) { mHoveredResource = hoveredResource; }
		else if (ImGui::IsWindowHovered(ImGuiHoveredFlags_ChildWindows)) { mHoveredResource = -1; }
	}

	void RenderGraphPanel::DrawSidebar(const rg::GraphInfo& info, float width, float height)
	{
		ImGui::BeginChild("##rgside", ImVec2(width, height), ImGuiChildFlags_None, ImGuiWindowFlags_None);
		ImGui::Indent(6.0f);
		if (mSelectedPass >= 0 && mSelectedPass < int(info.passes.size()))
		{
			const auto& pass = info.passes[mSelectedPass];
			EditorGui::SectionLabel(pass.name.c_str());
			if (pass.culled) { ImGui::TextColored(EditorStyle::kTextDim, "%s", Tr("Skipped: nothing that reaches the output uses its result")); }
			for (const auto& use : pass.uses)
			{
				ImGui::BulletText("%s  %s%s", info.resources[use.resource].name.c_str(), Tr(rg::ToString(use.access)),
					IsWrite(use.access) && use.overwrite ? (std::string(" (") + Tr("clear") + ")").c_str() : "");
			}
			ImGui::Spacing();
		}

		EditorGui::SectionLabel(Tr("Textures"));
		const ImGuiTableFlags flags = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp;
		if (ImGui::BeginTable("##rgres", 4, flags))
		{
			ImGui::TableSetupColumn(Tr("Name"), ImGuiTableColumnFlags_WidthStretch, 2.2f);
			ImGui::TableSetupColumn(Tr("Size"), ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableSetupColumn(Tr("Format"), ImGuiTableColumnFlags_WidthStretch, 1.4f);
			ImGui::TableSetupColumn(Tr("Memory"), ImGuiTableColumnFlags_WidthStretch, 1.0f);
			ImGui::TableHeadersRow();
			for (size_t i = 0; i < info.resources.size(); ++i)
			{
				const auto& r = info.resources[i];
				ImGui::TableNextRow();
				if (mHoveredResource == int(i)) { ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg1, ToU32(EditorStyle::kSelection)); }
				ImGui::TableNextColumn();
				ImGui::TextColored(ResourceColor(r), "%s", r.name.c_str());
				if (ImGui::IsItemHovered()) { mHoveredResource = int(i); }
				ImGui::TableNextColumn();
				ImGui::Text("%ux%u", r.width, r.height);
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(FormatName(r.format));
				ImGui::TableNextColumn();
				if (r.imported) { ImGui::TextColored(kImportColor, "%s", Tr("external")); }
				else if (r.physical < 0) { ImGui::TextDisabled("%s", Tr("unused")); }
				else { ImGui::Text("#%d", r.physical); }
			}
			ImGui::EndTable();
		}
		ImGui::Spacing();
		ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + width - 16.0f);
		ImGui::TextColored(EditorStyle::kTextDim, "%s", Tr("Textures with the same memory number share one allocation: their lifetimes do not overlap."));
		ImGui::TextColored(EditorStyle::kTextDim, "%s", Tr("Left dots: uses what is already there. Right dots: writes. (clear) means the pass overwrites everything."));
		ImGui::TextColored(EditorStyle::kTextDim, "%s", Tr("Shows the main view as it was last drawn."));
		ImGui::PopTextWrapPos();
		ImGui::Unindent(6.0f);
		ImGui::EndChild();
	}
}
