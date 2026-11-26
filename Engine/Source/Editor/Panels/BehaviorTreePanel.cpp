#include "RdPch.h"
#include "Editor/Panels/BehaviorTreePanel.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Editor.h"
#include "Editor/Panels/BehaviorTreeGui.h"
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
	using namespace btgui;
	using bt::NodeKind;
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
		Actor* selected = mEditor.GetSelection().Get();
		for (const bt::TreeInstance* inst : bt::TreeInstance::GetAll())
		{
			if (!inst->IsValid() || inst->GetPath() != mPath) { continue; }
			if (selected && inst->GetOwner() == selected) { return inst; }
			if (!first) { first = inst; }
		}
		return first;
	}
}
