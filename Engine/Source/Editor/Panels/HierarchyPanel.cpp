#include "RdPch.h"
#include "Editor/Panels/HierarchyPanel.h"
#include "Asset/Model/Model.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Commands/ActorStateCommand.h"
#include "Editor/Commands/ReparentCommand.h"
#include "Editor/Editor.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Render/Material/MaterialAsset.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"
#include "Scene/Serialization/SceneSerializer.h"
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		// ヒエラルキーに落として置けるもの（プレハブとモデル）
		bool IsPlaceablePayload(const ImGuiPayload* p)
		{
			if (!p) { return false; }
			std::string ext = GetExtension(static_cast<const char*>(p->Data));
			return ext == ".prefab" || Model::IsSupportedExtension(ext);
		}
	}

	HierarchyPanel::HierarchyPanel(Editor& editor) : EditorPanel(editor, "Hierarchy") {}

	HierarchyPanel::DropZone HierarchyPanel::GetDropZone(const ImRect& row, bool reorder)
	{
		if (!reorder) { return DropZone::Inside; }
		float y = ImGui::GetMousePos().y;
		float edge = row.GetHeight() * 0.25f;
		if (y < row.Min.y + edge) { return DropZone::Before; }
		if (y > row.Max.y - edge) { return DropZone::After; }
		return DropZone::Inside;
	}

	// 開いている親の下側へのドロップは、最初の子として入れる（Unity と同じ）
	HierarchyPanel::DropTarget HierarchyPanel::ResolveDrop(Actor* actor, DropZone zone, bool open)
	{
		if (zone == DropZone::Inside) { return { actor, -1 }; }
		if (zone == DropZone::After && open && !actor->GetChildren().empty()) { return { actor, 0 }; }
		return { actor->GetParent(), actor->GetSiblingIndex() + (zone == DropZone::After ? 1 : 0) };
	}

	bool HierarchyPanel::CanMove(const Actor* dragged, const Actor* target, const DropTarget& t)
	{
		if (!dragged || dragged == target) { return false; }
		if (t.parent && (t.parent == dragged || t.parent->IsDescendantOf(dragged))) { return false; }
		return true;
	}

	void HierarchyPanel::BeginRename(Actor* actor)
	{
		if (!actor) { return; }
		mEditor.GetSelection().Select(actor);
		mLastSelected = actor->GetId();
		mRenamingId = actor->GetId();
		mRenameBuffer = actor->GetName();
		mRenameFocus = true;
		SetOpen(true);
	}

	void HierarchyPanel::OnGui()
	{
		mOps = {};
		mDrop = {};
		mFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);
		// 選択が変わったら（ビューで選んだときなど）親を開いて、その行が見えるまでスクロールする
		if (mEditor.GetSelection().GetId() != mLastSelected)
		{
			mLastSelected = mEditor.GetSelection().GetId();
			mReveal = mLastSelected != 0;
			if (mRenamingId != mLastSelected) { mRenamingId = 0; }
		}
		HandleKeys();
		DrawToolbar();
		DrawTree();
		ApplyPendingOps();
		mReveal = false;
	}

	void HierarchyPanel::DrawToolbar()
	{
		EditorGui::BeginToolbar("toolbar");
		if (EditorGui::IconButton("##create", Icon::Plus, Tr("Create"))) { ImGui::OpenPopup("CreateActor"); }
		if (ImGui::BeginPopup("CreateActor"))
		{
			mEditor.GetActions().DrawCreateMenu(nullptr);
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		EditorGui::SearchField("##search", mFilter, Tr("Search"));
		EditorGui::EndToolbar();
	}

	void HierarchyPanel::DrawTree()
	{
		Scene& scene = mEditor.GetScene();
		const bool filtering = !mFilter.empty();
		mRows.clear();
		mRowCount = 0;

		ImGui::BeginChild("tree");
		// ドラッグしている間に上下の端へ近づいたらスクロールする
		if (ImGui::GetDragDropPayload() && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
		{
			float y = ImGui::GetMousePos().y;
			float top = ImGui::GetWindowPos().y, bottom = top + ImGui::GetWindowHeight();
			float band = ImGui::GetFrameHeight();
			float speed = 600.0f * ImGui::GetIO().DeltaTime;
			if (y < top + band) { ImGui::SetScrollY(ImGui::GetScrollY() - speed); }
			else if (y > bottom - band) { ImGui::SetScrollY(ImGui::GetScrollY() + speed); }
		}

		// 行の間を空けない（縞模様とドロップの位置をつなげる）
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));
		std::string sceneLabel = (scene.GetName().empty() ? std::string(Tr("Untitled")) : scene.GetName()) + (mEditor.GetUndo().GetHistory().IsDirty() ? "*" : "");
		bool sceneOpen = ImGui::TreeNodeEx("##scene", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding, "%s", sceneLabel.c_str());
		AcceptRootDrop();
		DrawSceneRow();
		if (sceneOpen)
		{
			if (filtering)
			{
				std::vector<Actor*> matches;
				scene.ForEachActor([&](Actor* a) { if (ContainsNoCase(a->GetName(), mFilter)) { matches.push_back(a); } });
				for (Actor* a : matches) { DrawNode(a, true); }
				if (matches.empty()) { EditorGui::EmptyHint(Tr("  No results")); }
			}
			else
			{
				auto roots = scene.GetRootActors();
				for (Actor* root : roots) { DrawNode(root, false); }
			}
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();

		// 残りの空いている所: クリックで選択を外す、ドロップでルートへ、右クリックで作成
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::InvisibleButton("##empty", ImVec2(std::max(avail.x, 1.0f), std::max(avail.y, ImGui::GetFrameHeight() * 2.0f)));
		if (ImGui::IsItemClicked()) { mEditor.GetSelection().Select(nullptr); mRenamingId = 0; }
		AcceptRootDrop();
		if (ImGui::BeginPopupContextItem("##emptyMenu"))
		{
			mEditor.GetActions().DrawCreateMenu(nullptr);
			if (mEditor.GetActions().HasClipboard())
			{
				ImGui::Separator();
				if (ImGui::MenuItem(Tr("Paste"), "Ctrl+V")) { mEditor.Defer([this] { mEditor.GetActions().PasteClipboard(); }); }
			}
			ImGui::EndPopup();
		}
		DrawDropIndicator();
		ImGui::EndChild();
	}

	void HierarchyPanel::DrawSceneRow()
	{
		Scene& scene = mEditor.GetScene();
		if (ImGui::BeginPopupContextItem())
		{
			mEditor.GetActions().DrawCreateMenu(nullptr);
			ImGui::Separator();
			if (ImGui::MenuItem(Tr("Save Scene"), "Ctrl+S")) { mEditor.Defer([this] { mEditor.GetDocument().SaveScene(false); }); }
			ImGui::EndPopup();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s\n%s", scene.GetPath().empty() ? Tr("(not saved)") : scene.GetPath().c_str(), EditorLocale::Format("{} actors", scene.GetActorCount()).c_str());
		}
	}

	void HierarchyPanel::AcceptRootDrop()
	{
		if (!ImGui::BeginDragDropTarget()) { return; }
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID"))
		{
			mOps.move = mEditor.GetScene().FindActor(*static_cast<const ActorId*>(p->Data));
			mOps.moveParent = nullptr;
			mOps.moveIndex = -1;
		}
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH"); IsPlaceablePayload(p))
		{
			mOps.instantiate = static_cast<const char*>(p->Data);
		}
		ImGui::EndDragDropTarget();
	}

	void HierarchyPanel::DrawDropIndicator()
	{
		if (!mDrop.active) { return; }
		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImU32 col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
		if (mDrop.zone == DropZone::Inside)
		{
			dl->AddRect(ImVec2(mDrop.row.Min.x + 1.0f, mDrop.row.Min.y), ImVec2(mDrop.row.Max.x - 1.0f, mDrop.row.Max.y), col, 3.0f, 0, 2.0f);
		}
		else
		{
			float y = mDrop.zone == DropZone::Before ? mDrop.row.Min.y : mDrop.row.Max.y;
			dl->AddCircle(ImVec2(mDrop.x + 3.0f, y), 3.0f, col, 0, 1.5f);
			dl->AddLine(ImVec2(mDrop.x + 6.0f, y), ImVec2(mDrop.row.Max.x, y), col, 2.0f);
		}
	}

	// 同じ親の中で後ろへ動かすときは、自分が抜けた分だけ前に詰まる
	void HierarchyPanel::ApplyPendingOps()
	{
		Scene& scene = mEditor.GetScene();
		if (mOps.move)
		{
			Actor* actor = mOps.move;
			Actor* parent = mOps.moveParent;
			int index = mOps.moveIndex;
			const bool sameParent = actor->GetParent() == parent;
			const int oldIndex = actor->GetSiblingIndex();
			if (sameParent && index > oldIndex) { --index; }
			const int last = int((parent ? parent->GetChildren() : scene.GetRootActors()).size()) - 1;
			const bool unchanged = sameParent && (index < 0 ? oldIndex == last : oldIndex == index);
			if (!unchanged)
			{
				auto command = std::make_unique<ReparentCommand>(EditorLocale::Format(sameParent ? "Reorder {}" : "Reparent {}", actor->GetName()), actor, parent, index);
				command->Redo(scene);
				mEditor.GetSelection().Select(actor);
				mEditor.GetUndo().Push(std::move(command));
				// 閉じた親へ入れたときも見えるよう、次のフレームで親を開く
				mLastSelected = 0;
			}
		}
		if (!mOps.instantiate.empty())
		{
			mEditor.GetActions().InstantiateAsset(mOps.instantiate, mOps.instParent, mOps.instIndex);
		}
		if (mOps.rename && !mRenameBuffer.empty() && mRenameBuffer != mOps.rename->GetName())
		{
			Json before = SceneSerializer::SerializeActorState(mOps.rename);
			mOps.rename->SetName(mRenameBuffer);
			mEditor.GetUndo().Push(std::make_unique<ActorStateCommand>(Tr("Rename"), mOps.rename->GetId(), std::move(before), SceneSerializer::SerializeActorState(mOps.rename)));
		}
		if (mOps.toggleActive)
		{
			// 選択は変えずに切り替える
			Actor* a = mOps.toggleActive;
			Json before = SceneSerializer::SerializeActorState(a);
			a->SetActive(!a->IsActiveSelf());
			auto command = std::make_unique<ActorStateCommand>(Tr(a->IsActiveSelf() ? "Activate" : "Deactivate"), a->GetId(), std::move(before), SceneSerializer::SerializeActorState(a));
			command->SetSelection(mEditor.GetSelection().GetId());
			mEditor.GetUndo().Push(std::move(command));
		}
	}

	void HierarchyPanel::DrawNode(Actor* actor, bool flat)
	{
		const ActorId id = actor->GetId();
		const bool hasChildren = !flat && !actor->GetChildren().empty();
		const bool selected = id == mEditor.GetSelection().GetId();

		ImGui::PushID(reinterpret_cast<void*>(id));
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImVec2 rowPos = ImGui::GetCursorScreenPos();
		const float rowH = ImGui::GetFrameHeight();
		const ImRect row(ImVec2(window->WorkRect.Min.x, rowPos.y), ImVec2(window->WorkRect.Max.x, rowPos.y + rowH));

		if (mRowCount++ & 1) { window->DrawList->AddRectFilled(row.Min, row.Max, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt)); }

		if (!flat)
		{
			if (mReveal)
			{
				if (Actor* sel = mEditor.GetSelection().Get(); sel && sel->IsDescendantOf(actor)) { ImGui::SetNextItemOpen(true); }
			}
			if (mToggleId == id)
			{
				ImGui::SetNextItemOpen(mToggleOpen);
				mToggleId = 0;
			}
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
			ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;
		if (!hasChildren) { flags |= ImGuiTreeNodeFlags_Leaf; }
		if (flat) { flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen; }
		if (selected) { flags |= ImGuiTreeNodeFlags_Selected; }
		// フォーカスがないときの選択は灰色（Unity と同じ）
		//  フォーカスがあるときは、左から右へ薄れていく差し色のグラデーションと、左端の細い帯を先に敷く（ImGui の背景は透明にする）
		//  マウスを乗せた行の背景も ImGui には描かせず、ふわっと出し入れする
		const ImGuiID rowKey = ImGui::GetID("##node");
		const float hover = EditorStyle::HoverFade(rowKey, 20.0f);
		const float reveal = EditorStyle::Animate(rowKey ^ 0x51u, selected ? 1.0f : 0.0f, 16.0f);
		if (hover > 0.0f && !(selected && mFocused))
		{
			ImVec4 bg = EditorStyle::kHover;
			bg.w *= hover * 0.85f;
			window->DrawList->AddRectFilled(row.Min, row.Max, ImGui::ColorConvertFloat4ToU32(bg));
		}
		if (selected && mFocused) { EditorStyle::DrawSelection(window->DrawList, row.Min, row.Max, reveal); }
		ImGui::PushStyleColor(ImGuiCol_Header, selected && !mFocused ? EditorStyle::kHeaderBg : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selected && !mFocused ? EditorStyle::kHover : ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
		const bool open = ImGui::TreeNodeEx("##node", flags, "") && !flat;
		ImGui::PopStyleColor(2);
		EditorStyle::SetHovered(rowKey, ImGui::IsItemHovered());
		const bool treeOpen = open && hasChildren;

		if (selected && mReveal) { ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY); }
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		{
			mEditor.GetSelection().Select(actor);
			if (mRenamingId != id) { mRenamingId = 0; }
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		{
			mEditor.Defer([this, id] { mEditor.GetActions().FocusOn(mEditor.GetScene().FindActor(id)); });
		}

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("ACTOR_ID", &id, sizeof(id));
			EditorIcons::Inline(EditorIcons::ForActor(actor));
			ImGui::SameLine();
			ImGui::TextUnformatted(actor->GetName().c_str());
			ImGui::EndDragDropSource();
		}
		DrawNodeDropTarget(actor, row, flat, treeOpen, rowPos.x);
		DrawNodeContextMenu(actor);
		DrawNodeLabel(actor, row, flat, rowPos.x);

		mRows.push_back({ id, actor->GetParent() ? actor->GetParent()->GetId() : 0, treeOpen, hasChildren });

		if (open)
		{
			auto children = actor->GetChildren();
			for (Actor* child : children) { DrawNode(child, false); }
			ImGui::TreePop();
		}
		ImGui::PopID();
	}

	void HierarchyPanel::DrawNodeDropTarget(Actor* actor, const ImRect& row, bool flat, bool treeOpen, float rowX)
	{
		if (!ImGui::BeginDragDropTarget()) { return; }
		const ImGuiDragDropFlags acceptFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
		const DropZone zone = GetDropZone(row, !flat);
		const DropTarget target = ResolveDrop(actor, zone, treeOpen);
		const bool asFirstChild = zone == DropZone::After && target.parent == actor;
		auto showIndicator = [&]()
			{
				mDrop.active = true;
				mDrop.row = row;
				mDrop.zone = zone;
				mDrop.x = rowX + (asFirstChild ? ImGui::GetStyle().IndentSpacing : 0.0f);
			};
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID", acceptFlags))
		{
			Actor* dragged = mEditor.GetScene().FindActor(*static_cast<const ActorId*>(p->Data));
			if (CanMove(dragged, actor, target))
			{
				showIndicator();
				if (p->IsDelivery())
				{
					mOps.move = dragged;
					mOps.moveParent = target.parent;
					mOps.moveIndex = target.index;
				}
			}
		}
		if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH", acceptFlags))
		{
			if (IsPlaceablePayload(p))
			{
				showIndicator();
				if (p->IsDelivery())
				{
					mOps.instantiate = static_cast<const char*>(p->Data);
					mOps.instParent = target.parent;
					mOps.instIndex = target.index;
				}
			}
			// マテリアルは行の上に落とすと、そのアクターの MeshRenderer の最初の枠に入れる（Unity と同じ）
			else if (MaterialAsset::IsMaterialPath(static_cast<const char*>(p->Data)) && actor->GetComponent<MeshRenderer>())
			{
				ImGui::GetWindowDrawList()->AddRect(row.Min, row.Max, ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f, 0, 2.0f);
				if (p->IsDelivery())
				{
					ActorId id = actor->GetId();
					std::string path = static_cast<const char*>(p->Data);
					mEditor.Defer([this, id, path]
						{
							if (Actor* a = mEditor.GetScene().FindActor(id))
							{
								if (auto mr = a->GetComponent<MeshRenderer>()) { mEditor.GetActions().AssignMaterial(*mr, 0, path); }
							}
						});
				}
			}
		}
		ImGui::EndDragDropTarget();
	}

	void HierarchyPanel::DrawNodeContextMenu(Actor* actor)
	{
		if (!ImGui::BeginPopupContextItem()) { return; }
		Scene& scene = mEditor.GetScene();
		if (mEditor.GetSelection().GetId() != actor->GetId()) { mEditor.GetSelection().Select(actor); }
		if (ImGui::MenuItem(Tr("Rename"), "F2")) { BeginRename(actor); }
		if (ImGui::MenuItem(Tr("Copy"), "Ctrl+C")) { mEditor.GetActions().CopySelected(); }
		if (ImGui::MenuItem(Tr("Paste"), "Ctrl+V", false, mEditor.GetActions().HasClipboard())) { mEditor.Defer([this] { mEditor.GetActions().PasteClipboard(); }); }
		if (ImGui::MenuItem(Tr("Duplicate"), "Ctrl+D")) { mEditor.Defer([this] { mEditor.GetActions().DuplicateSelected(); }); }
		if (ImGui::MenuItem(Tr("Delete"), "Del")) { mEditor.Defer([this] { mEditor.GetActions().DeleteSelected(); }); }
		ImGui::Separator();
		if (ImGui::BeginMenu(Tr("Create Child")))
		{
			mEditor.GetActions().DrawCreateMenu(actor);
			ImGui::EndMenu();
		}
		ImGui::Separator();
		const int sibling = actor->GetSiblingIndex();
		const int count = int((actor->GetParent() ? actor->GetParent()->GetChildren() : scene.GetRootActors()).size());
		auto moveTo = [&](Actor* parent, int index) { mOps.move = actor; mOps.moveParent = parent; mOps.moveIndex = index; };
		if (ImGui::MenuItem(Tr("Move Up"), nullptr, false, sibling > 0)) { moveTo(actor->GetParent(), sibling - 1); }
		if (ImGui::MenuItem(Tr("Move Down"), nullptr, false, sibling < count - 1)) { moveTo(actor->GetParent(), sibling + 2); }
		if (ImGui::MenuItem(Tr("Move to Top"), nullptr, false, sibling > 0)) { moveTo(actor->GetParent(), 0); }
		if (ImGui::MenuItem(Tr("Move to Bottom"), nullptr, false, sibling < count - 1)) { moveTo(actor->GetParent(), -1); }
		if (ImGui::MenuItem(Tr("Unparent"), nullptr, false, actor->GetParent() != nullptr)) { moveTo(nullptr, -1); }
		ImGui::Separator();
		if (ImGui::MenuItem(Tr("Focus"), Tr("F / Double-click"))) { mEditor.Defer([this, actor] { mEditor.GetActions().FocusOn(actor); }); }
		if (ImGui::MenuItem(Tr("Create Prefab")))
		{
			std::string dir = mEditor.GetProject().GetCurrentFolder();
			mEditor.Defer([this, actor, dir] { mEditor.GetActions().CreatePrefab(actor, dir); });
		}
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", EditorLocale::Format("Save to {}/\n(or drag the actor into the Project panel)", mEditor.GetProject().GetCurrentFolder()).c_str()); }
		ImGui::EndPopup();
	}

	// アイコンと名前を自分で描く。行にマウスがあるか無効のときは、右端に有効 / 無効を切り替える目を出す
	void HierarchyPanel::DrawNodeLabel(Actor* actor, const ImRect& row, bool flat, float rowX)
	{
		const ImGuiStyle& style = ImGui::GetStyle();
		const ActorId id = actor->GetId();
		const float rowH = row.GetHeight();
		const float iconX = rowX + ImGui::GetTreeNodeToLabelSpacing();
		const float iconSize = EditorStyle::IconSize();
		const float nameX = iconX + iconSize + style.ItemInnerSpacing.x;
		const float textY = row.Min.y + style.FramePadding.y;
		const bool activeSelf = actor->IsActiveSelf();
		const bool active = actor->IsActive();
		ImDrawList* dl = ImGui::GetWindowDrawList();

		ImU32 iconColor = EditorIcons::DefaultColor(EditorIcons::ForActor(actor));
		if (!active) { iconColor = (iconColor & 0x00FFFFFF) | 0x66000000; }
		EditorIcons::Draw(dl, ImVec2(iconX, textY), iconSize, EditorIcons::ForActor(actor), iconColor);

		const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && row.Contains(ImGui::GetMousePos()) && !ImGui::GetDragDropPayload();
		const bool showEye = hovered || !activeSelf;
		float nameRight = row.Max.x - (showEye ? rowH : 0.0f) - style.FramePadding.x;

		if (mRenamingId == id)
		{
			ImGui::SetCursorScreenPos(ImVec2(nameX - style.FramePadding.x, row.Min.y));
			ImGui::SetNextItemWidth(std::max(40.0f, nameRight - nameX));
			if (mRenameFocus)
			{
				ImGui::SetKeyboardFocusHere();
				mRenameFocus = false;
				mRenameWait = 3;
			}
			ImGui::InputText("##rename", &mRenameBuffer, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
			if (ImGui::IsItemDeactivated())
			{
				if (!ImGui::IsKeyPressed(ImGuiKey_Escape)) { mOps.rename = actor; }
				mRenamingId = 0;
			}
			else if (!ImGui::IsItemActive() && mRenameWait > 0 && --mRenameWait == 0)
			{
				// 入力欄が有効にならなかった
				mRenamingId = 0;
			}
		}
		else
		{
			ImU32 textCol = ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			dl->PushClipRect(ImVec2(nameX, row.Min.y), ImVec2(nameRight, row.Max.y), true);
			dl->AddText(ImVec2(nameX, textY), textCol, actor->GetName().c_str());
			if (flat && actor->GetParent())
			{
				// 検索の結果では親の名前も添える
				float w = ImGui::CalcTextSize(actor->GetName().c_str()).x;
				std::string parentName = "  (" + actor->GetParent()->GetName() + ")";
				dl->AddText(ImVec2(nameX + w, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), parentName.c_str());
			}
			dl->PopClipRect();
		}

		if (showEye)
		{
			ImGui::SetCursorScreenPos(ImVec2(row.Max.x - rowH, row.Min.y));
			if (ImGui::InvisibleButton("##active", ImVec2(rowH, rowH))) { mOps.toggleActive = actor; }
			bool eyeHovered = ImGui::IsItemHovered();
			if (eyeHovered) { ImGui::SetTooltip("%s", Tr(activeSelf ? "Deactivate" : "Activate")); }
			ImU32 color = eyeHovered ? ImGui::GetColorU32(ImGuiCol_Text) : ImGui::GetColorU32(ImGuiCol_TextDisabled);
			if (!activeSelf) { color = ImGui::ColorConvertFloat4ToU32(EditorStyle::kError); }
			float s = rowH * 0.8f;
			EditorIcons::Draw(dl, ImVec2(row.Max.x - rowH * 0.5f - s * 0.5f, row.Min.y + (rowH - s) * 0.5f), s, activeSelf ? Icon::Eye : Icon::EyeOff, color);
		}
	}

	// 矢印キーは ImGui のキーボード操作と取り合わないよう、このパネルが持つ
	void HierarchyPanel::HandleKeys()
	{
		if (!mFocused || mRenamingId != 0 || ImGui::GetIO().WantTextInput) { return; }
		const ImGuiID owner = ImGui::GetID("##hierarchyKeys");
		for (ImGuiKey key : { ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow }) { ImGui::SetKeyOwner(key, owner); }
		auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, owner, ImGuiInputFlags_Repeat); };

		if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) { BeginRename(mEditor.GetSelection().Get()); return; }
		if (mRows.empty()) { return; }

		int index = -1;
		for (int i = 0; i < int(mRows.size()); ++i)
		{
			if (mRows[i].id == mEditor.GetSelection().GetId()) { index = i; break; }
		}
		Scene& scene = mEditor.GetScene();
		auto selectRow = [&](int i)
			{
				if (i >= 0 && i < int(mRows.size())) { mEditor.GetSelection().Select(scene.FindActor(mRows[i].id)); }
			};
		if (index < 0)
		{
			if (pressed(ImGuiKey_DownArrow)) { selectRow(0); }
			if (pressed(ImGuiKey_UpArrow)) { selectRow(int(mRows.size()) - 1); }
			return;
		}
		const Row& row = mRows[index];
		if (pressed(ImGuiKey_UpArrow)) { selectRow(index - 1); }
		else if (pressed(ImGuiKey_DownArrow)) { selectRow(index + 1); }
		else if (pressed(ImGuiKey_LeftArrow))
		{
			if (row.open) { mToggleId = row.id; mToggleOpen = false; }
			else if (row.parent) { mEditor.GetSelection().Select(scene.FindActor(row.parent)); }
		}
		else if (pressed(ImGuiKey_RightArrow))
		{
			if (row.hasChildren && !row.open) { mToggleId = row.id; mToggleOpen = true; }
			else if (row.open) { selectRow(index + 1); }
		}
	}
}
