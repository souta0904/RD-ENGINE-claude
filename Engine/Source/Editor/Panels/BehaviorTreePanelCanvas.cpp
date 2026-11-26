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
	// ------------------------------------------------------------------ 描画（キャンバス・ノード・つなぎ線・作成のメニュー）

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
}
