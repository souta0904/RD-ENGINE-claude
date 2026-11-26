#include "RdPch.h"
#include "Editor/Panels/BehaviorTreePanel.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorIcons.h"
#include "Editor/Style/EditorStyle.h"
#include "Scene/Actor/Actor.h"
#include <ImGui/imgui_node_editor.h>
#include <ImGui/imgui_stdlib.h>
#include <algorithm>

namespace ed = ax::NodeEditor;

namespace rd
{
	namespace
	{
		using bt::NodeKind;

		// 番号の作り方: ノード = id*64、入口 = id*64+1、出口 = id*64+2+出口の番号、つなぎ線 = 子の id*64+63
		//  （子の親は 1 つだけなので、つなぎ線は子の番号で決まる）
		constexpr uint64_t kStride = 64;
		constexpr int kMaxSlots = 60;
		ed::NodeId NodeKey(uint32_t id) { return ed::NodeId(uint64_t(id) * kStride); }
		ed::PinId InputPin(uint32_t id) { return ed::PinId(uint64_t(id) * kStride + 1); }
		ed::PinId OutputPin(uint32_t id, int slot) { return ed::PinId(uint64_t(id) * kStride + 2 + uint64_t(slot)); }
		ed::LinkId LinkKey(uint32_t child) { return ed::LinkId(uint64_t(child) * kStride + 63); }
		uint32_t IdOf(uint64_t key) { return uint32_t(key / kStride); }
		int PinSlot(uint64_t pin) { return int(pin % kStride) - 2; } // -1 なら入口

		ImVec2 Add(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
		ImU32 ToU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
		ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }

		// ノードの色（種類ごと）
		ImVec4 KindColor(const bt::NodeTypeInfo* info, const std::string& type)
		{
			if (!info) { return EditorStyle::kError; }
			switch (info->kind)
			{
			case NodeKind::Root: return ImVec4(0.62f, 0.64f, 0.70f, 1.0f);
			case NodeKind::Composite: return EditorStyle::kAccent;
			case NodeKind::Decorator: return ImVec4(0.72f, 0.56f, 0.90f, 1.0f);
			case NodeKind::Branch:
			case NodeKind::Switch: return ImVec4(0.95f, 0.66f, 0.36f, 1.0f);
			case NodeKind::Leaf: return type == bt::builtin::kCheck ? ImVec4(0.93f, 0.82f, 0.38f, 1.0f) : ImVec4(0.50f, 0.74f, 1.00f, 1.0f);
			}
			return EditorStyle::kAccent;
		}

		const char* KindLabel(const bt::NodeTypeInfo* info)
		{
			if (!info) { return "Unknown"; }
			switch (info->kind)
			{
			case NodeKind::Root: return "Root";
			case NodeKind::Composite: return "Composite";
			case NodeKind::Decorator: return "Decorator";
			case NodeKind::Branch:
			case NodeKind::Switch: return "Flow";
			case NodeKind::Leaf: return info->category.c_str();
			}
			return "";
		}

		// 出口の名前（Branch は True / False、Switch は選び方の値の名前）
		std::string SlotLabel(const bt::NodeData& node, const bt::NodeTypeInfo* info, int slot)
		{
			if (!info) { return {}; }
			if (info->kind == NodeKind::Branch) { return Tr(slot == 0 ? "True" : "False"); }
			if (info->kind == NodeKind::Switch)
			{
				if (const bt::SelectorInfo* sel = bt::Registry::FindSelector(bt::GetString(node.params, "Value")))
				{
					if (slot < int(sel->cases.size())) { return Tr(sel->cases[slot]); }
				}
				return std::to_string(slot);
			}
			return {};
		}

		std::string ValueText(const Json& v)
		{
			if (v.is_string()) { return Tr(v.get<std::string>()); }
			if (v.is_boolean()) { return Tr(v.get<bool>() ? "On" : "Off"); }
			if (v.is_number_float()) { return std::format("{:g}", v.get<float>()); }
			if (v.is_number()) { return std::to_string(v.get<int64_t>()); }
			return {};
		}

		// ノードに出す設定値の要約（例: "IsTargetFound"・"1 s"・"Vision: Caution"）
		std::string Summary(const bt::NodeData& node, const bt::NodeTypeInfo* info)
		{
			if (!info) { return {}; }
			std::string out;
			for (const bt::ParamDef& p : info->params)
			{
				auto it = node.params.find(p.name);
				if (it == node.params.end()) { continue; }
				std::string part;
				switch (p.type)
				{
				case bt::ParamType::Condition:
				{
					std::string name = it->is_string() ? it->get<std::string>() : std::string();
					part = name.empty() ? std::string(Tr("(no condition)")) : std::string(Tr(name));
					if (bt::GetBool(node.params, "Invert")) { part += std::string(" ") + Tr("(inverted)"); }
					break;
				}
				case bt::ParamType::Selector:
				{
					std::string name = it->is_string() ? it->get<std::string>() : std::string();
					part = name.empty() ? std::string(Tr("(no value)")) : std::string(Tr(name));
					break;
				}
				case bt::ParamType::Bool:
					if (p.name == "Invert") { continue; }
					if (info->kind == NodeKind::Root) { part = it->get<bool>() ? Tr("Repeat") : Tr("Once"); break; }
					part = std::format("{}: {}", Tr(p.name), ValueText(*it));
					break;
				default:
					if (node.type == bt::builtin::kWait) { part = std::format("{} s", ValueText(*it)); break; }
					if (node.type == bt::builtin::kLog) { part = "\"" + ValueText(*it) + "\""; break; }
					part = std::format("{}: {}", Tr(p.name), ValueText(*it));
					break;
				}
				if (!out.empty()) { out += "\n"; }
				out += part;
			}
			return out;
		}

		// ノードの幅は文字の大きさで決まる（DrawNode）ので、並べる間隔も文字の大きさに合わせる
		void AutoLayout(bt::TreeAsset& asset)
		{
			const float fs = ImGui::GetFontSize();
			asset.AutoLayout(fs * 12.0f, fs * 7.5f);
		}

		bool Contains(const std::string& text, const std::string& filter)
		{
			return filter.empty() || ToLower(text).find(ToLower(filter)) != std::string::npos;
		}
	}

	BehaviorTreePanel::BehaviorTreePanel(Editor& editor)
		: EditorPanel(editor, "Behavior Tree", false)
	{
		ed::Config config;
		config.SettingsName = nullptr;           // 位置はアセットに持つので imgui.ini には書かない
		config.RasterizeTextForZoom = false;     // ImGui 1.92 より前では使えない
		mContext = ed::CreateEditor(&config);
		ed::SetCurrentEditor(mContext);
		ed::Style& style = ed::GetStyle();
		style.NodePadding = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		style.NodeRounding = 6.0f;
		style.NodeBorderWidth = 1.0f;
		style.HoveredNodeBorderWidth = 2.0f;
		style.SelectedNodeBorderWidth = 2.5f;
		style.PinRounding = 3.0f;
		style.LinkStrength = 60.0f;
		style.SourceDirection = ImVec2(0.0f, 1.0f);
		style.TargetDirection = ImVec2(0.0f, -1.0f);
		style.FlowDuration = 0.6f;
		style.FlowMarkerDistance = 18.0f;
		style.FlowSpeed = 220.0f;
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
		style.Colors[ed::StyleColor_PinRect] = WithAlpha(EditorStyle::kAccent, 0.35f);
		style.Colors[ed::StyleColor_PinRectBorder] = WithAlpha(EditorStyle::kAccent, 0.6f);
		style.Colors[ed::StyleColor_Flow] = EditorStyle::kWarning;
		style.Colors[ed::StyleColor_FlowMarker] = EditorStyle::kWarning;
		ed::SetCurrentEditor(nullptr);
	}

	BehaviorTreePanel::~BehaviorTreePanel()
	{
		if (mContext) { ed::DestroyEditor(mContext); }
	}

	// ------------------------------------------------------------------ ファイル

	void BehaviorTreePanel::Open(const std::string& path)
	{
		RequestFocus();
		if (path == mPath && mLoaded) { return; }
		if (mDirty)
		{
			mPendingOpen = path;
			mOpenSavePrompt = true;
			return;
		}
		LoadFile(path);
	}

	void BehaviorTreePanel::LoadFile(const std::string& path)
	{
		bt::TreeAsset asset;
		if (!asset.Load(path))
		{
			Logger::Error(LogCat::Editor, "Behavior tree: failed to open {}", path);
			return;
		}
		// 位置が全部 0（手で書いた・コードから書き出した）なら木の形に並べる
		bool unplaced = asset.nodes.size() > 1 && std::all_of(asset.nodes.begin(), asset.nodes.end(),
			[](const bt::NodeData& d) { return d.position.x == 0.0f && d.position.y == 0.0f; });
		// 並べる間隔は文字の大きさで決めるので、描くとき（最初のフレームの前は文字の大きさが 0）にする
		mLayoutPending = unplaced;
		mAsset = std::move(asset);
		mPath = path;
		mLoaded = true;
		mDirty = unplaced;
		mChanged = false;
		mUndo.clear();
		mRedo.clear();
		mCommitted = mAsset.ToJson().dump();
		mFlash.clear();
		mPushPositions = true;
		mNavigateFrames = 2;
		if (mContext)
		{
			ed::SetCurrentEditor(mContext);
			ed::ClearSelection();
			ed::SetCurrentEditor(nullptr);
		}
	}

	bool BehaviorTreePanel::Save()
	{
		if (!mLoaded) { return false; }
		CommitIfSettled();
		if (!mAsset.Save(mPath))
		{
			Logger::Error(LogCat::Editor, "Behavior tree: failed to save {}", mPath);
			return false;
		}
		mDirty = false;
		Logger::Info(LogCat::Editor, "Saved behavior tree: {}", mPath);
		return true;
	}

	// ------------------------------------------------------------------ Undo

	void BehaviorTreePanel::CommitIfSettled()
	{
		if (!mChanged) { return; }
		std::string now = mAsset.ToJson().dump();
		mChanged = false;
		if (now == mCommitted) { return; }
		mUndo.push_back(std::move(mCommitted));
		if (mUndo.size() > 200) { mUndo.erase(mUndo.begin()); }
		mRedo.clear();
		mCommitted = std::move(now);
		mDirty = true;
	}

	void BehaviorTreePanel::ApplySnapshot(const std::string& json)
	{
		mAsset.FromJson(Json::parse(json));
		mCommitted = json;
		mChanged = false;
		mDirty = true;
		mPushPositions = true;
	}

	void BehaviorTreePanel::UndoEdit()
	{
		CommitIfSettled();
		if (mUndo.empty()) { return; }
		std::string prev = std::move(mUndo.back());
		mUndo.pop_back();
		mRedo.push_back(mCommitted);
		ApplySnapshot(prev);
	}

	void BehaviorTreePanel::RedoEdit()
	{
		CommitIfSettled();
		if (mRedo.empty()) { return; }
		std::string next = std::move(mRedo.back());
		mRedo.pop_back();
		mUndo.push_back(mCommitted);
		ApplySnapshot(next);
	}

	bool BehaviorTreePanel::HandleShortcuts()
	{
		if (!mFocused || !mLoaded) { return false; }
		ImGuiIO& io = ImGui::GetIO();
		auto pressed = [](ImGuiKey key) { return ImGui::IsKeyPressed(key, false); };
		if (io.KeyCtrl && pressed(ImGuiKey_Z)) { UndoEdit(); }
		if (io.KeyCtrl && pressed(ImGuiKey_Y)) { RedoEdit(); }
		if (io.KeyCtrl && pressed(ImGuiKey_S)) { Save(); }
		if (io.KeyCtrl && pressed(ImGuiKey_D))
		{
			ed::SetCurrentEditor(mContext);
			DuplicateSelection();
			ed::SetCurrentEditor(nullptr);
		}
		if (!io.KeyCtrl && pressed(ImGuiKey_F)) { mNavigateFrames = 1; }
		return true;
	}

	// ------------------------------------------------------------------ 編集

	bool BehaviorTreePanel::CanConnect(uint32_t parent, uint32_t child) const
	{
		if (parent == child) { return false; }
		const bt::NodeData* c = mAsset.Find(child);
		if (!c || c->type == bt::builtin::kRoot) { return false; }
		// 子が親の祖先なら輪になる
		auto& asset = const_cast<bt::TreeAsset&>(mAsset);
		for (const bt::NodeData* p = asset.Find(parent); p; p = asset.FindParent(p->id))
		{
			if (p->id == child) { return false; }
		}
		return true;
	}

	void BehaviorTreePanel::Detach(uint32_t child)
	{
		int slot = -1;
		bt::NodeData* parent = mAsset.FindParent(child, &slot);
		if (!parent) { return; }
		if (bt::TreeAsset::GetFixedSlotCount(*parent) < 0) { parent->children.erase(parent->children.begin() + slot); }
		else { parent->children[slot] = 0; }
		MarkChanged();
	}

	void BehaviorTreePanel::Connect(uint32_t parentId, int slot, uint32_t child)
	{
		if (!CanConnect(parentId, child)) { return; }
		Detach(child);
		bt::NodeData* parent = mAsset.Find(parentId);
		if (!parent) { return; }
		int fixed = bt::TreeAsset::GetFixedSlotCount(*parent);
		if (fixed < 0)
		{
			parent->children.push_back(child);
			mAsset.SortChildrenByPosition();
		}
		else if (slot >= 0 && slot < int(parent->children.size())) { parent->children[slot] = child; }
		MarkChanged();
	}

	uint32_t BehaviorTreePanel::CreateNode(const std::string& type, const Vector2& position)
	{
		uint32_t id = mAsset.AddNode(type, position).id;
		ed::SetNodePosition(NodeKey(id), ImVec2(position.x, position.y));
		MarkChanged();
		return id;
	}

	std::vector<uint32_t> BehaviorTreePanel::GetSelectedIds() const
	{
		std::vector<uint32_t> ids;
		int count = ed::GetSelectedObjectCount();
		if (count <= 0) { return ids; }
		std::vector<ed::NodeId> nodes(static_cast<size_t>(count));
		int n = ed::GetSelectedNodes(nodes.data(), count);
		for (int i = 0; i < n; ++i) { ids.push_back(IdOf(nodes[i].Get())); }
		return ids;
	}

	// 呼ぶ側で ed::SetCurrentEditor(mContext) にしておく
	void BehaviorTreePanel::DuplicateSelection()
	{
		std::vector<uint32_t> ids = GetSelectedIds();
		std::vector<uint32_t> created;
		for (uint32_t id : ids)
		{
			const bt::NodeData* src = mAsset.Find(id);
			if (!src || src->type == bt::builtin::kRoot) { continue; }
			bt::NodeData copy = *src;
			bt::NodeData& d = mAsset.AddNode(copy.type, Vector2(copy.position.x + 30.0f, copy.position.y + 30.0f));
			d.comment = copy.comment;
			d.params = copy.params;
			mAsset.Normalize(d);
			ed::SetNodePosition(NodeKey(d.id), ImVec2(d.position.x, d.position.y));
			created.push_back(d.id);
		}
		if (!created.empty())
		{
			ed::ClearSelection();
			for (uint32_t id : created) { ed::SelectNode(NodeKey(id), true); }
			MarkChanged();
		}
	}

	void BehaviorTreePanel::DeleteSelection()
	{
		for (uint32_t id : GetSelectedIds())
		{
			const bt::NodeData* d = mAsset.Find(id);
			if (!d || d->type == bt::builtin::kRoot) { continue; }
			ed::DeleteNode(NodeKey(id));
		}
	}

	// ------------------------------------------------------------------ 描画

	void BehaviorTreePanel::OnGui()
	{
		mFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		// タブが隠れている間は大きさが分からないので、見えたときに全体を表示し直す
		if (ImGui::IsWindowAppearing() && mNavigateFrames == 0) { mNavigateFrames = 2; }
		DrawSavePrompt();
		DrawToolbar();
		if (!mLoaded)
		{
			EditorGui::EmptyState(Icon::File, Tr("Open a behavior tree (.rdbt) from the Project panel, or create one with Create > Behavior Tree"));
			return;
		}
		if (mLayoutPending)
		{
			AutoLayout(mAsset);
			mLayoutPending = false;
			mPushPositions = true;
			mNavigateFrames = 2;
		}
		ImVec2 avail = ImGui::GetContentRegionAvail();
		float sideWidth = std::min(std::max(260.0f, ImGui::GetFontSize() * 20.0f), avail.x * 0.45f);
		DrawCanvas(std::max(50.0f, avail.x - sideWidth), avail.y);
		ImGui::SameLine(0.0f, 0.0f);
		DrawSidebar(sideWidth, avail.y);
		// ドラッグ中・入力中は確定しない（離したときに 1 回の変更として Undo に積む）
		if (!ImGui::IsMouseDown(ImGuiMouseButton_Left) && !ImGui::IsAnyItemActive()) { CommitIfSettled(); }
	}

	void BehaviorTreePanel::DrawToolbar()
	{
		EditorGui::BeginToolbar("##bttoolbar");
		ImGui::BeginDisabled(!mLoaded);
		if (EditorGui::IconTextButton("##save", Icon::File, Tr("Save"), "Ctrl+S")) { Save(); }
		ImGui::SameLine();
		if (EditorGui::IconTextButton("##layout", Icon::Menu, Tr("Auto Layout")))
		{
			AutoLayout(mAsset);
			mPushPositions = true;
			mNavigateFrames = 2;
			MarkChanged();
		}
		ImGui::SameLine();
		if (EditorGui::IconTextButton("##fit", Icon::Search, Tr("Frame All"), "F")) { mNavigateFrames = 1; }
		ImGui::EndDisabled();
		EditorGui::ToolbarSeparator();
		ImGui::SameLine();
		if (mLoaded)
		{
			ImGui::TextUnformatted(GetFileName(mPath).c_str());
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", mPath.c_str()); }
			if (mDirty)
			{
				ImGui::SameLine(0.0f, 2.0f);
				ImGui::TextColored(EditorStyle::kWarning, "*");
			}
			if (!mDebugOwner.empty())
			{
				ImGui::SameLine();
				EditorGui::ToolbarSeparator();
				ImGui::SameLine();
				ImGui::TextColored(EditorStyle::kPlay, "%s", EditorLocale::Format("Running: {}", mDebugOwner).c_str());
			}
		}
		EditorGui::EndToolbar();
	}

	const bt::TreeInstance* BehaviorTreePanel::FindDebugInstance() const
	{
		if (!mEditor.IsPlaying()) { return nullptr; }
		const bt::TreeInstance* first = nullptr;
		Actor* selected = mEditor.GetSelected();
		for (const bt::TreeInstance* inst : bt::TreeInstance::GetAll())
		{
			if (!inst->IsValid() || inst->GetPath() != mPath) { continue; }
			if (selected && inst->GetOwner() == selected) { return inst; }
			if (!first) { first = inst; }
		}
		return first;
	}

	void BehaviorTreePanel::DrawCanvas(float width, float height)
	{
		ed::SetCurrentEditor(mContext);
		ed::Begin("##BehaviorTree", ImVec2(width, height));
		ImVec2 mouseCanvas = ImGui::GetMousePos();

		const bt::TreeInstance* debug = FindDebugInstance();
		mDebugOwner = (debug && debug->GetOwner()) ? debug->GetOwner()->GetName() : std::string();
		const double now = ImGui::GetTime();

		for (bt::NodeData& node : mAsset.nodes)
		{
			if (mPushPositions) { ed::SetNodePosition(NodeKey(node.id), ImVec2(node.position.x, node.position.y)); }
			DrawNode(node, debug, now);
		}
		mPushPositions = false;
		DrawLinks(debug);
		HandleCreateAndDelete();
		DrawContextMenus(mouseCanvas);

		if (mNavigateFrames > 0 && --mNavigateFrames == 0) { ed::NavigateToContent(0.25f); }

		// ドラッグで動かした位置をアセットへ戻す（Composite の子の順番も横の位置で決め直す）
		bool moved = false;
		for (bt::NodeData& node : mAsset.nodes)
		{
			ImVec2 p = ed::GetNodePosition(NodeKey(node.id));
			if (!std::isfinite(p.x) || !std::isfinite(p.y)) { continue; }
			if (std::abs(p.x - node.position.x) > 0.01f || std::abs(p.y - node.position.y) > 0.01f)
			{
				node.position = Vector2(p.x, p.y);
				moved = true;
			}
		}
		if (moved)
		{
			mAsset.SortChildrenByPosition();
			MarkChanged();
		}
		ed::End();
		ed::SetCurrentEditor(nullptr);
	}

	void BehaviorTreePanel::DrawNode(bt::NodeData& node, const bt::TreeInstance* debug, double now)
	{
		const bt::NodeTypeInfo* info = bt::Registry::FindType(node.type);
		const ImVec4 color = KindColor(info, node.type);
		const float fs = ImGui::GetFontSize();
		const float pad = fs * 0.55f;
		const bool isRoot = info && info->kind == NodeKind::Root;
		const int fixed = bt::TreeAsset::GetFixedSlotCount(node);
		const int outputs = !info ? 0 : (fixed < 0 ? 1 : std::min(fixed, kMaxSlots));
		const float width = std::max(fs * 10.5f, float(outputs) * fs * 4.2f);

		// 動いている様子（プレイ中）: 動いている = 黄色の太い枠、終わったところ = 成功は緑・失敗は赤を少しの間
		ImVec4 border(0, 0, 0, 0);
		float borderWidth = 0.0f;
		if (debug)
		{
			if (const bt::Node* rn = debug->FindNode(node.id))
			{
				NodeFlash& f = mFlash[node.id];
				if (rn->GetTickCount() != f.ticks)
				{
					f.ticks = rn->GetTickCount();
					f.time = now;
				}
				if (rn->IsRunning())
				{
					border = EditorStyle::kWarning;
					borderWidth = 3.0f;
				}
				else
				{
					float fade = 1.0f - float(now - f.time) / 0.8f;
					if (fade > 0.0f)
					{
						border = WithAlpha(rn->GetLastStatus() == bt::Status::Success ? EditorStyle::kSuccess : EditorStyle::kError, fade);
						borderWidth = 2.5f;
					}
				}
			}
		}
		if (borderWidth > 0.0f)
		{
			ed::PushStyleColor(ed::StyleColor_NodeBorder, border);
			ed::PushStyleVar(ed::StyleVar_NodeBorderWidth, borderWidth);
		}

		ed::BeginNode(NodeKey(node.id));
		ImGui::PushID(int(node.id));
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		// 入口（上の辺の真ん中）
		const float pinH = fs * 0.5f;
		if (!isRoot)
		{
			ed::BeginPin(InputPin(node.id), ed::PinKind::Input);
			ed::PinPivotAlignment(ImVec2(0.5f, 0.0f));
			ed::PinPivotSize(ImVec2(0.0f, 0.0f));
			ImGui::Dummy(ImVec2(width, pinH));
			ImVec2 a = ImGui::GetItemRectMin();
			ImVec2 b = ImGui::GetItemRectMax();
			ed::PinRect(ImVec2(a.x + width * 0.3f, a.y), ImVec2(b.x - width * 0.3f, b.y));
			dl->AddRectFilled(ImVec2(a.x + width * 0.38f, a.y + 2.0f), ImVec2(b.x - width * 0.38f, a.y + pinH * 0.55f), ToU32(WithAlpha(color, 0.7f)), 2.0f);
			ed::EndPin();
		}
		else { ImGui::Dummy(ImVec2(width, pinH * 0.5f)); }

		// 見出し: 種類の分類（小さく）と名前
		ImGui::SetCursorScreenPos(ImVec2(origin.x + pad, ImGui::GetCursorScreenPos().y));
		ImGui::BeginGroup();
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 1.0f));
		if (!isRoot) { ImGui::TextColored(WithAlpha(color, 0.9f), "%s", Tr(KindLabel(info))); }
		// コメントも訳の表を通す（表になければそのまま）
		const char* title = node.comment.empty() ? Tr(node.type) : Tr(node.comment);
		ImGui::TextUnformatted(title);
		if (!node.comment.empty()) { ImGui::TextDisabled("%s", Tr(node.type)); }
		if (!info) { ImGui::TextColored(EditorStyle::kError, "%s", Tr("Unknown node type")); }
		std::string summary = Summary(node, info);
		if (!summary.empty())
		{
			ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kTextDim);
			ImGui::PushTextWrapPos(origin.x + width - pad);
			ImGui::TextUnformatted(summary.c_str());
			ImGui::PopTextWrapPos();
			ImGui::PopStyleColor();
		}
		ImGui::PopStyleVar();
		ImGui::EndGroup();
		ImGui::Dummy(ImVec2(width, pad * 0.6f));

		// 出口（下の辺に並べる）
		if (outputs > 0)
		{
			const float y = ImGui::GetCursorScreenPos().y;
			const float slotW = width / float(outputs);
			const bool labeled = info && (info->kind == NodeKind::Branch || info->kind == NodeKind::Switch);
			const float h = labeled ? fs * 1.35f : pinH;
			for (int s = 0; s < outputs; ++s)
			{
				ImGui::SetCursorScreenPos(ImVec2(origin.x + slotW * float(s), y));
				ed::BeginPin(OutputPin(node.id, s), ed::PinKind::Output);
				ed::PinPivotAlignment(ImVec2(0.5f, 1.0f));
				ed::PinPivotSize(ImVec2(0.0f, 0.0f));
				ImGui::Dummy(ImVec2(slotW, h));
				ImVec2 a = ImGui::GetItemRectMin();
				ImVec2 b = ImGui::GetItemRectMax();
				ed::PinRect(a, b);
				bool connected = s < int(node.children.size()) && node.children[s] != 0;
				if (labeled)
				{
					dl->AddRectFilled(ImVec2(a.x + 2.0f, a.y), ImVec2(b.x - 2.0f, b.y - 2.0f), ToU32(WithAlpha(color, connected ? 0.30f : 0.14f)), 3.0f);
					std::string label = SlotLabel(node, info, s);
					ImVec2 ts = ImGui::CalcTextSize(label.c_str());
					dl->AddText(ImVec2(a.x + (slotW - ts.x) * 0.5f, a.y + (h - 2.0f - ts.y) * 0.5f), ToU32(EditorStyle::kText), label.c_str());
				}
				else
				{
					dl->AddRectFilled(ImVec2(a.x + slotW * 0.38f, b.y - pinH * 0.55f), ImVec2(b.x - slotW * 0.38f, b.y - 2.0f), ToU32(WithAlpha(color, 0.7f)), 2.0f);
				}
				ed::EndPin();
			}
		}
		else { ImGui::Dummy(ImVec2(width, pinH * 0.5f)); }

		ImGui::PopID();
		ed::EndNode();
		if (borderWidth > 0.0f)
		{
			ed::PopStyleVar();
			ed::PopStyleColor();
		}

		// 上の色の帯と、Composite の子の順番の印
		if (ImDrawList* bg = ed::GetNodeBackgroundDrawList(NodeKey(node.id)))
		{
			ImVec2 p = ed::GetNodePosition(NodeKey(node.id));
			ImVec2 size = ed::GetNodeSize(NodeKey(node.id));
			bg->AddRectFilled(p, ImVec2(p.x + size.x, p.y + 3.0f), ToU32(color), 6.0f, ImDrawFlags_RoundCornersTop);
			int slot = -1;
			const bt::NodeData* parent = mAsset.FindParent(node.id, &slot);
			if (parent && bt::TreeAsset::GetFixedSlotCount(*parent) < 0 && parent->children.size() > 1)
			{
				std::string n = std::to_string(slot + 1);
				ImVec2 c(p.x + size.x - fs * 0.2f, p.y + fs * 0.2f);
				float r = fs * 0.55f;
				bg->AddCircleFilled(c, r, ToU32(EditorStyle::kAccentDim));
				bg->AddCircle(c, r, ToU32(EditorStyle::kAccent), 0, 1.0f);
				ImVec2 ts = ImGui::CalcTextSize(n.c_str());
				bg->AddText(ImVec2(c.x - ts.x * 0.5f, c.y - ts.y * 0.5f), ToU32(EditorStyle::kText), n.c_str());
			}
		}
	}

	void BehaviorTreePanel::DrawLinks(const bt::TreeInstance* debug)
	{
		for (const bt::NodeData& node : mAsset.nodes)
		{
			const int fixed = bt::TreeAsset::GetFixedSlotCount(node);
			for (size_t i = 0; i < node.children.size(); ++i)
			{
				uint32_t child = node.children[i];
				if (!child || !mAsset.Find(child)) { continue; }
				int slot = fixed < 0 ? 0 : int(i);
				ImVec4 color = WithAlpha(KindColor(bt::Registry::FindType(node.type), node.type), 0.75f);
				float thickness = 2.0f;
				bool running = false;
				if (debug)
				{
					if (const bt::Node* rn = debug->FindNode(child)) { running = rn->IsRunning(); }
				}
				if (running)
				{
					color = EditorStyle::kWarning;
					thickness = 3.5f;
				}
				ed::Link(LinkKey(child), OutputPin(node.id, slot), InputPin(child), color, thickness);
				// 動き始めた子へのつなぎ線に流れを出す
				NodeFlash& f = mFlash[child];
				if (running && !f.wasRunning) { ed::Flow(LinkKey(child)); }
				f.wasRunning = running;
			}
		}
	}

	void BehaviorTreePanel::HandleCreateAndDelete()
	{
		if (ed::BeginCreate(EditorStyle::kAccent, 2.0f))
		{
			ed::PinId a, b;
			if (ed::QueryNewLink(&a, &b) && a.Get() != 0 && b.Get() != 0)
			{
				uint64_t pa = a.Get(), pb = b.Get();
				// 出口 → 入口 の向きにそろえる
				if (PinSlot(pa) < 0) { std::swap(pa, pb); }
				bool ok = PinSlot(pa) >= 0 && PinSlot(pb) < 0 && CanConnect(IdOf(pa), IdOf(pb));
				if (!ok) { ed::RejectNewItem(EditorStyle::kError, 2.0f); }
				else if (ed::AcceptNewItem(EditorStyle::kSuccess, 2.5f)) { Connect(IdOf(pa), PinSlot(pa), IdOf(pb)); }
			}
			ed::PinId pin;
			if (ed::QueryNewNode(&pin) && pin.Get() != 0)
			{
				if (ed::AcceptNewItem())
				{
					mCreateFromPin = pin.Get();
					mCreatePos = ImGui::GetMousePos();
					mCreateFilter.clear();
					mFocusCreateFilter = true;
					ed::Suspend();
					ImGui::OpenPopup("##btcreate");
					ed::Resume();
				}
			}
			// End は Begin が true を返したときだけ呼ぶ
			ed::EndCreate();
		}

		if (ed::BeginDelete())
		{
			ed::LinkId link;
			while (ed::QueryDeletedLink(&link))
			{
				if (ed::AcceptDeletedItem()) { Detach(IdOf(link.Get())); }
			}
			ed::NodeId nodeId;
			while (ed::QueryDeletedNode(&nodeId))
			{
				uint32_t id = IdOf(nodeId.Get());
				const bt::NodeData* d = mAsset.Find(id);
				if (!d || d->type == bt::builtin::kRoot)
				{
					ed::RejectDeletedItem();
					continue;
				}
				if (ed::AcceptDeletedItem())
				{
					mAsset.RemoveNode(id);
					mFlash.erase(id);
					MarkChanged();
				}
			}
			ed::EndDelete();
		}
	}

	void BehaviorTreePanel::DrawCreateMenuItems()
	{
		if (mFocusCreateFilter)
		{
			ImGui::SetKeyboardFocusHere();
			mFocusCreateFilter = false;
		}
		EditorGui::SearchField("##btfilter", mCreateFilter, Tr("Search"), ImGui::GetFontSize() * 14.0f);
		ImGui::Separator();

		// 分類ごとに並べる（組み込みが先、ゲームの登録したものが後）
		std::vector<std::string> categories;
		for (const bt::NodeTypeInfo& t : bt::Registry::GetTypes())
		{
			if (t.kind == NodeKind::Root) { continue; }
			if (std::find(categories.begin(), categories.end(), t.category) == categories.end()) { categories.push_back(t.category); }
		}
		// 入口から引き出したときは子を持てる種類だけ
		const bool needParent = mCreateFromPin && PinSlot(mCreateFromPin) < 0;
		std::string chosen;
		for (const std::string& cat : categories)
		{
			bool header = false;
			for (const bt::NodeTypeInfo& t : bt::Registry::GetTypes())
			{
				if (t.kind == NodeKind::Root || t.category != cat) { continue; }
				if (needParent && t.kind == NodeKind::Leaf) { continue; }
				if (!Contains(t.name, mCreateFilter) && !Contains(Tr(t.name), mCreateFilter)) { continue; }
				if (!header)
				{
					EditorGui::SectionLabel(Tr(cat));
					header = true;
				}
				ImGui::PushStyleColor(ImGuiCol_Text, KindColor(&t, t.name));
				ImGui::Bullet();
				ImGui::PopStyleColor();
				ImGui::SameLine();
				if (ImGui::Selectable(Tr(t.name))) { chosen = t.name; }
				if (ImGui::IsItemHovered() && !t.description.empty()) { ImGui::SetTooltip("%s", Tr(t.description)); }
			}
		}
		if (chosen.empty()) { return; }

		uint32_t id = CreateNode(chosen, Vector2(mCreatePos.x, mCreatePos.y));
		if (mCreateFromPin)
		{
			uint32_t other = IdOf(mCreateFromPin);
			int slot = PinSlot(mCreateFromPin);
			if (slot >= 0) { Connect(other, slot, id); }
			else { Connect(id, 0, other); }
		}
		mCreateFromPin = 0;
		ed::ClearSelection();
		ed::SelectNode(NodeKey(id));
		ImGui::CloseCurrentPopup();
	}

	void BehaviorTreePanel::DrawContextMenus(const ImVec2& mouseCanvas)
	{
		ed::Suspend();
		ed::NodeId nodeId;
		ed::LinkId linkId;
		if (ed::ShowNodeContextMenu(&nodeId))
		{
			mContextNode = IdOf(nodeId.Get());
			if (!ed::IsNodeSelected(nodeId)) { ed::SelectNode(nodeId); }
			ImGui::OpenPopup("##btnode");
		}
		else if (ed::ShowLinkContextMenu(&linkId))
		{
			mContextLink = IdOf(linkId.Get());
			ImGui::OpenPopup("##btlink");
		}
		else if (ed::ShowBackgroundContextMenu())
		{
			mCreatePos = mouseCanvas;
			mCreateFromPin = 0;
			mCreateFilter.clear();
			mFocusCreateFilter = true;
			ImGui::OpenPopup("##btcreate");
		}

		if (ImGui::BeginPopup("##btcreate"))
		{
			DrawCreateMenuItems();
			ImGui::EndPopup();
		}
		else if (mCreateFromPin && !ImGui::IsPopupOpen("##btcreate")) { mCreateFromPin = 0; }

		if (ImGui::BeginPopup("##btnode"))
		{
			const bt::NodeData* d = mAsset.Find(mContextNode);
			bool root = d && d->type == bt::builtin::kRoot;
			if (ImGui::MenuItem(Tr("Duplicate"), "Ctrl+D", false, !root)) { DuplicateSelection(); }
			if (ImGui::MenuItem(Tr("Disconnect from Parent"), nullptr, false, d && mAsset.FindParent(mContextNode)))
			{
				Detach(mContextNode);
			}
			if (ImGui::MenuItem(Tr("Delete"), "Delete", false, !root)) { DeleteSelection(); }
			ImGui::EndPopup();
		}
		if (ImGui::BeginPopup("##btlink"))
		{
			if (ImGui::MenuItem(Tr("Delete"))) { ed::DeleteLink(LinkKey(mContextLink)); }
			ImGui::EndPopup();
		}
		ed::Resume();
	}

	// ------------------------------------------------------------------ 右の欄（選んだノードの設定）

	bool BehaviorTreePanel::DrawParam(const bt::ParamDef& def, Json& value)
	{
		ImGui::PushID(def.name.c_str());
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(Tr(def.name));
		if (ImGui::IsItemHovered() && !def.tooltip.empty()) { ImGui::SetTooltip("%s", Tr(def.tooltip)); }
		ImGui::SameLine(ImGui::GetFontSize() * 6.5f);
		ImGui::SetNextItemWidth(-1.0f);
		bool changed = false;
		auto combo = [&](const std::vector<std::string>& names, bool allowEmpty) {
			std::string current = value.is_string() ? value.get<std::string>() : std::string();
			bool known = current.empty() || std::find(names.begin(), names.end(), current) != names.end();
			std::string preview = current.empty() ? std::string(Tr("(None)")) : std::string(Tr(current));
			if (!known) { preview += Tr(" - Missing"); }
			if (!known) { ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kError); }
			bool open = ImGui::BeginCombo("##v", preview.c_str());
			if (!known) { ImGui::PopStyleColor(); }
			if (open)
			{
				if (allowEmpty && ImGui::Selectable(Tr("(None)"), current.empty())) { value = ""; changed = true; }
				for (const std::string& n : names)
				{
					if (ImGui::Selectable(Tr(n), n == current)) { value = n; changed = true; }
				}
				ImGui::EndCombo();
			}
			};
		switch (def.type)
		{
		case bt::ParamType::Bool:
		{
			bool b = value.is_boolean() ? value.get<bool>() : false;
			if (ImGui::Checkbox("##v", &b)) { value = b; changed = true; }
			break;
		}
		case bt::ParamType::Int:
		{
			int i = value.is_number() ? value.get<int>() : 0;
			if (ImGui::DragInt("##v", &i, 0.1f)) { value = i; changed = true; }
			break;
		}
		case bt::ParamType::Float:
		{
			float f = value.is_number() ? value.get<float>() : 0.0f;
			if (ImGui::DragFloat("##v", &f, 0.01f, 0.0f, 0.0f, "%.3g")) { value = f; changed = true; }
			break;
		}
		case bt::ParamType::String:
		{
			std::string s = value.is_string() ? value.get<std::string>() : std::string();
			if (ImGui::InputText("##v", &s)) { value = s; changed = true; }
			break;
		}
		case bt::ParamType::Enum: combo(def.options, false); break;
		case bt::ParamType::Condition:
		{
			std::vector<std::string> names;
			for (const auto& c : bt::Registry::GetConditions()) { names.push_back(c.name); }
			combo(names, true);
			if (const bt::ConditionInfo* c = bt::Registry::FindCondition(value.is_string() ? value.get<std::string>() : ""))
			{
				if (!c->description.empty()) { ImGui::TextDisabled("%s", Tr(c->description)); }
			}
			break;
		}
		case bt::ParamType::Selector:
		{
			std::vector<std::string> names;
			for (const auto& s : bt::Registry::GetSelectors()) { names.push_back(s.name); }
			combo(names, true);
			break;
		}
		}
		ImGui::PopID();
		return changed;
	}

	void BehaviorTreePanel::DrawSidebar(float width, float height)
	{
		ImGui::PushStyleColor(ImGuiCol_ChildBg, EditorStyle::kWindowBg);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 8.0f));
		ImGui::BeginChild("##btside", ImVec2(width, height), ImGuiChildFlags_AlwaysUseWindowPadding);
		ImGui::PopStyleVar();

		ed::SetCurrentEditor(mContext);
		std::vector<uint32_t> selected = GetSelectedIds();
		ed::SetCurrentEditor(nullptr);

		if (selected.size() == 1)
		{
			if (bt::NodeData* node = mAsset.Find(selected[0]))
			{
				const bt::NodeTypeInfo* info = bt::Registry::FindType(node->type);
				ImGui::PushStyleColor(ImGuiCol_Text, KindColor(info, node->type));
				ImGui::TextUnformatted(Tr(KindLabel(info)));
				ImGui::PopStyleColor();
				ImGui::TextUnformatted(Tr(node->type));
				if (info && !info->description.empty())
				{
					ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kTextDim);
					ImGui::TextWrapped("%s", Tr(info->description));
					ImGui::PopStyleColor();
				}
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();

				ImGui::AlignTextToFramePadding();
				ImGui::TextUnformatted(Tr("Comment"));
				ImGui::SameLine(ImGui::GetFontSize() * 6.5f);
				ImGui::SetNextItemWidth(-1.0f);
				if (ImGui::InputTextWithHint("##comment", Tr("Shown instead of the type name (translated when the language table has it)"), &node->comment)) { MarkChanged(); }

				if (info)
				{
					for (const bt::ParamDef& def : info->params)
					{
						Json& v = node->params[def.name];
						if (DrawParam(def, v))
						{
							mAsset.Normalize(*node);
							MarkChanged();
						}
					}
				}
				else
				{
					ImGui::TextColored(EditorStyle::kError, "%s", Tr("This node type is not registered. Load the game code that registers it."));
				}

				// 動いている様子
				if (const bt::TreeInstance* debug = FindDebugInstance())
				{
					if (const bt::Node* rn = debug->FindNode(node->id))
					{
						ImGui::Spacing();
						ImGui::Separator();
						const char* state = rn->IsRunning() ? "Running" : (rn->GetLastStatus() == bt::Status::Success ? "Success" : "Failure");
						ImGui::TextDisabled("%s", EditorLocale::Format("State: {} (ticks: {})", Tr(state), rn->GetTickCount()).c_str());
					}
				}
				ImGui::TextDisabled("Id %u", node->id);
			}
		}
		else
		{
			EditorGui::SectionLabel(Tr("Behavior Tree"));
			ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kTextDim);
			ImGui::TextWrapped("%s", EditorLocale::Format("{} nodes", mAsset.nodes.size()).c_str());
			ImGui::Spacing();
			ImGui::TextWrapped("%s", Tr("Right-click the background to add a node."));
			ImGui::TextWrapped("%s", Tr("Drag from the bottom of a node to the top of another to connect them. Drop on empty space to add a connected node."));
			ImGui::TextWrapped("%s", Tr("Children of Sequence and Selector run from left to right (the number on the corner)."));
			ImGui::TextWrapped("%s", Tr("Press Delete to remove the selection. Right-drag pans, wheel zooms, F frames everything."));
			ImGui::TextWrapped("%s", Tr("While playing, running nodes are outlined in yellow (the selected actor's tree, or the first one found)."));
			ImGui::PopStyleColor();
			if (selected.size() > 1)
			{
				ImGui::Spacing();
				ImGui::TextUnformatted(EditorLocale::Format("{} nodes selected", selected.size()).c_str());
			}
		}
		ImGui::EndChild();
		ImGui::PopStyleColor();
	}

	void BehaviorTreePanel::DrawSavePrompt()
	{
		if (mOpenSavePrompt)
		{
			ImGui::OpenPopup(TrId("Save Changes?"));
			mOpenSavePrompt = false;
		}
		const char* const kButtons[] = { Tr("Save"), Tr("Don't Save"), Tr("Cancel") };
		std::string message = EditorLocale::Format("Save the changes to \"{}\"?", mPath);
		int choice = -1;
		if (!EditorGui::ConfirmPopup(TrId("Save Changes?"), message.c_str(), kButtons, 3, choice)) { return; }
		std::string next = std::move(mPendingOpen);
		mPendingOpen.clear();
		if (choice == 0 && !Save()) { return; }
		if (choice == 0 || choice == 1)
		{
			mDirty = false;
			LoadFile(next);
		}
	}
}
