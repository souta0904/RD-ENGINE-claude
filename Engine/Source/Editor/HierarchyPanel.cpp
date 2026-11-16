#include "RdPch.h"
#include "Editor/Editor.h"
#include "Components/Rendering/CameraComponent.h"
#include "Components/Lighting/DirectionalLight.h"
#include "Components/Lighting/PointLight.h"
#include "Components/Lighting/SpotLight.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Components/Effects/ParticleSystem.h"
#include "Core/Util/StringUtil.h"
#include "Core/Util/FileUtil.h"
#include "Engine.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>
#include <ImGui/imgui_stdlib.h>

// Unity 風のヒエラルキー
//  - 行の上 1/4 / 下 1/4 へのドロップで前後に並べ替え、真ん中へのドロップで子にする
//  - シーンの行・空いている所へのドロップでルートの末尾へ
//  - ↑↓で選択、←→で開閉、F2 で名前の変更、ダブルクリックでフォーカス
//  - 検索欄に入力すると一致したアクターだけを平らに並べる
namespace rd
{
	namespace
	{
		// ツリーの描画中に行うと走査が壊れる操作は、描き終えてからまとめて実行する
		struct HierarchyOps
		{
			Actor* move = nullptr;// 並べ替え・親子付け替え
			Actor* moveParent = nullptr;
			int moveIndex = -1;// 動かす前の並びでの挿入位置（-1 で末尾）
			std::string instantiate;// ドロップされたプレハブ
			Actor* instParent = nullptr;
			int instIndex = -1;
			Actor* remove = nullptr;
			Actor* duplicate = nullptr;
			std::string createKind;
			Actor* createParent = nullptr;
			Actor* savePrefab = nullptr;
			Actor* rename = nullptr;
			Actor* toggleActive = nullptr;
			Actor* focus = nullptr;
		};
		HierarchyOps gOps;

		enum class DropZone { Before, Inside, After };

		// ドロップ位置の表示（後の行の背景で隠れないよう、ツリーを描き終えてから描く）
		struct DropIndicator
		{
			bool active = false;
			ImRect row;
			DropZone zone = DropZone::Inside;
			float x = 0.0f;
		};
		DropIndicator gDrop;

		bool gFocused = false;// ヒエラルキーにフォーカスがあるか（選択色を変える）
		int gRenameWait = 0;// 名前の入力欄が有効になるまでのフレーム数

		struct DropTarget
		{
			Actor* parent = nullptr;
			int index = -1;
		};

		// 行の上 1/4 なら前、下 1/4 なら後ろ、真ん中なら子
		DropZone GetDropZone(const ImRect& row, bool reorder)
		{
			if (!reorder) { return DropZone::Inside; }
			float y = ImGui::GetMousePos().y;
			float edge = row.GetHeight() * 0.25f;
			if (y < row.Min.y + edge) { return DropZone::Before; }
			if (y > row.Max.y - edge) { return DropZone::After; }
			return DropZone::Inside;
		}

		DropTarget ResolveDrop(Actor* actor, DropZone zone, bool open)
		{
			if (zone == DropZone::Inside) { return { actor, -1 }; }
			// 開いている親の下側 = 最初の子として入れる（Unity と同じ）
			if (zone == DropZone::After && open && !actor->GetChildren().empty()) { return { actor, 0 }; }
			return { actor->GetParent(), actor->GetSiblingIndex() + (zone == DropZone::After ? 1 : 0) };
		}

		bool CanMove(const Actor* dragged, const Actor* target, const DropTarget& t)
		{
			if (!dragged || dragged == target) { return false; }
			if (t.parent && (t.parent == dragged || t.parent->IsDescendantOf(dragged))) { return false; }
			return true;
		}

		bool IsPrefabPayload(const ImGuiPayload* p)
		{
			return p && GetExtension(static_cast<const char*>(p->Data)) == ".prefab";
		}

		// --------------------------------------------------
		// アイコン（フォントに頼らず図形で描く）
		// --------------------------------------------------
		enum class ActorIcon { Empty, Mesh, Light, Camera, Particles };

		ActorIcon GetIcon(const Actor* a)
		{
			if (a->GetComponent<CameraComponent>()) { return ActorIcon::Camera; }
			if (a->GetComponent<DirectionalLight>() || a->GetComponent<PointLight>() || a->GetComponent<SpotLight>()) { return ActorIcon::Light; }
			if (a->GetComponent<ParticleSystem>()) { return ActorIcon::Particles; }
			if (a->GetComponent<MeshRenderer>()) { return ActorIcon::Mesh; }
			return ActorIcon::Empty;
		}

		ImU32 Rgba(float r, float g, float b, float a) { return ImGui::ColorConvertFloat4ToU32(ImVec4(r, g, b, a)); }

		void DrawIcon(ImDrawList* dl, ImVec2 p, float s, ActorIcon icon, float alpha)
		{
			float cx = p.x + s * 0.5f, cy = p.y + s * 0.5f;
			switch (icon)
			{
			case ActorIcon::Empty:
			case ActorIcon::Mesh:
			{
				// 等角の立方体
				float r = s * 0.46f, w = r * 0.866f;
				ImVec2 top(cx, cy - r), ur(cx + w, cy - r * 0.5f), lr(cx + w, cy + r * 0.5f);
				ImVec2 bot(cx, cy + r), ll(cx - w, cy + r * 0.5f), ul(cx - w, cy - r * 0.5f), c(cx, cy);
				ImU32 line;
				if (icon == ActorIcon::Mesh)
				{
					dl->AddQuadFilled(top, ur, c, ul, Rgba(0.62f, 0.78f, 1.00f, alpha));
					dl->AddQuadFilled(ul, c, bot, ll, Rgba(0.40f, 0.58f, 0.90f, alpha));
					dl->AddQuadFilled(c, ur, lr, bot, Rgba(0.28f, 0.42f, 0.72f, alpha));
					line = Rgba(0.75f, 0.85f, 1.0f, alpha);
				}
				else
				{
					line = Rgba(0.70f, 0.72f, 0.76f, alpha);
				}
				ImVec2 hex[] = { top, ur, lr, bot, ll, ul };
				dl->AddPolyline(hex, 6, line, ImDrawFlags_Closed, 1.0f);
				dl->AddLine(c, ul, line);
				dl->AddLine(c, ur, line);
				dl->AddLine(c, bot, line);
				break;
			}
			case ActorIcon::Light:
			{
				ImU32 col = Rgba(1.0f, 0.84f, 0.30f, alpha);
				dl->AddCircleFilled(ImVec2(cx, cy), s * 0.2f, col);
				for (int i = 0; i < 8; ++i)
				{
					float a = i * (3.14159265f / 4.0f);
					float ca = std::cos(a), sa = std::sin(a);
					dl->AddLine(ImVec2(cx + ca * s * 0.3f, cy + sa * s * 0.3f), ImVec2(cx + ca * s * 0.46f, cy + sa * s * 0.46f), col, 1.2f);
				}
				break;
			}
			case ActorIcon::Camera:
			{
				ImU32 col = Rgba(0.70f, 0.85f, 0.70f, alpha);
				dl->AddRectFilled(ImVec2(p.x + s * 0.06f, cy - s * 0.22f), ImVec2(p.x + s * 0.62f, cy + s * 0.24f), col, 1.5f);
				dl->AddTriangleFilled(ImVec2(p.x + s * 0.62f, cy), ImVec2(p.x + s * 0.96f, cy - s * 0.24f), ImVec2(p.x + s * 0.96f, cy + s * 0.26f), col);
				break;
			}
			case ActorIcon::Particles:
			{
				ImU32 col = Rgba(1.0f, 0.60f, 0.30f, alpha);
				dl->AddCircleFilled(ImVec2(cx - s * 0.2f, cy + s * 0.18f), s * 0.14f, col);
				dl->AddCircleFilled(ImVec2(cx + s * 0.22f, cy + s * 0.05f), s * 0.11f, col);
				dl->AddCircleFilled(ImVec2(cx - s * 0.02f, cy - s * 0.25f), s * 0.09f, col);
				break;
			}
			}
		}

		// 有効/無効の切り替えボタンの絵（目）
		void DrawEye(ImDrawList* dl, ImVec2 c, float s, bool active, bool hovered)
		{
			ImU32 col = active ? (hovered ? Rgba(0.95f, 0.95f, 0.95f, 1.0f) : Rgba(0.65f, 0.65f, 0.68f, 1.0f))
				: (hovered ? Rgba(0.85f, 0.55f, 0.50f, 1.0f) : Rgba(0.55f, 0.40f, 0.38f, 1.0f));
			float w = s * 0.34f, h = s * 0.26f;
			dl->PathLineTo(ImVec2(c.x - w, c.y));
			dl->PathBezierCubicCurveTo(ImVec2(c.x - w * 0.5f, c.y - h), ImVec2(c.x + w * 0.5f, c.y - h), ImVec2(c.x + w, c.y));
			dl->PathBezierCubicCurveTo(ImVec2(c.x + w * 0.5f, c.y + h), ImVec2(c.x - w * 0.5f, c.y + h), ImVec2(c.x - w, c.y));
			dl->PathStroke(col, ImDrawFlags_Closed, 1.2f);
			if (active) { dl->AddCircleFilled(c, s * 0.1f, col); }
			else { dl->AddLine(ImVec2(c.x - w * 0.8f, c.y + h * 1.3f), ImVec2(c.x + w * 0.8f, c.y - h * 1.3f), col, 1.4f); }
		}

		bool ContainsNoCase(const std::string& text, const std::string& pattern)
		{
			auto it = std::search(text.begin(), text.end(), pattern.begin(), pattern.end(),
				[](char a, char b) { return std::tolower(static_cast<unsigned char>(a)) == std::tolower(static_cast<unsigned char>(b)); });
			return it != text.end();
		}
	}

	// ==================================================
	// パネル
	// ==================================================
	void Editor::DrawHierarchy()
	{
		if (!ImGui::Begin("Hierarchy", &mShowHierarchy))
		{
			ImGui::End();
			return;
		}
		Scene& scene = mEngine.GetScene();
		gOps = {};
		gDrop = {};
		gFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

		// 選択が変わったら、親を開いてその行が見えるまでスクロールする（ビューポートで選んだときなど）
		if (mSelectedId != mHierarchyLastSelected)
		{
			mHierarchyLastSelected = mSelectedId;
			mHierarchyReveal = mSelectedId != 0;
			if (mRenamingId != mSelectedId) { mRenamingId = 0; }
		}
		HandleHierarchyKeys();

		// ツールバー
		if (ImGui::Button("+")) { ImGui::OpenPopup("CreateActor"); }
		if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Create"); }
		if (ImGui::BeginPopup("CreateActor"))
		{
			DrawHierarchyCreateMenu(nullptr);
			ImGui::EndPopup();
		}
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-FLT_MIN);
		ImGui::InputTextWithHint("##search", "Search", &mHierarchyFilter);
		const bool filtering = !mHierarchyFilter.empty();

		mHierarchyRows.clear();
		mHierarchyRowCount = 0;

		ImGui::BeginChild("tree");
		// ドラッグ中に上下の端へ近づいたらスクロールする
		if (ImGui::GetDragDropPayload() && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem))
		{
			float y = ImGui::GetMousePos().y;
			float top = ImGui::GetWindowPos().y, bottom = top + ImGui::GetWindowHeight();
			float band = ImGui::GetFrameHeight();
			float speed = 600.0f * ImGui::GetIO().DeltaTime;
			if (y < top + band) { ImGui::SetScrollY(ImGui::GetScrollY() - speed); }
			else if (y > bottom - band) { ImGui::SetScrollY(ImGui::GetScrollY() + speed); }
		}

		// 行の間に隙間を空けない（縞模様とドロップ位置をつなげる）
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));

		// ルートへ動かすドロップ先（シーンの行・空いている所）
		auto acceptRootDrop = [&]()
		{
			if (!ImGui::BeginDragDropTarget()) { return; }
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID"))
			{
				gOps.move = scene.FindActor(*static_cast<const ActorId*>(p->Data));
				gOps.moveParent = nullptr;
				gOps.moveIndex = -1;
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH"); IsPrefabPayload(p))
			{
				gOps.instantiate = static_cast<const char*>(p->Data);
			}
			ImGui::EndDragDropTarget();
		};

		// シーンの行（Unity の一番上の行）
		std::string sceneLabel = (scene.GetName().empty() ? std::string("Untitled") : scene.GetName()) + (mHistory.IsDirty() ? "*" : "");
		bool sceneOpen = ImGui::TreeNodeEx("##scene", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanFullWidth | ImGuiTreeNodeFlags_FramePadding, "%s", sceneLabel.c_str());
		acceptRootDrop();
		if (ImGui::BeginPopupContextItem())
		{
			DrawHierarchyCreateMenu(nullptr);
			ImGui::Separator();
			if (ImGui::MenuItem("Save Scene", "Ctrl+S")) { SaveScene(false); }
			ImGui::EndPopup();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s\n%zu actors", scene.GetPath().empty() ? "(not saved)" : scene.GetPath().c_str(), scene.GetActorCount());
		}

		if (sceneOpen)
		{
			if (filtering)
			{
				// 一致したものだけを階層順に平らに並べる
				std::vector<Actor*> matches;
				scene.ForEachActor([&](Actor* a) { if (ContainsNoCase(a->GetName(), mHierarchyFilter)) { matches.push_back(a); } });
				for (Actor* a : matches) { DrawHierarchyNode(a, true); }
				if (matches.empty()) { ImGui::TextDisabled("  No results"); }
			}
			else
			{
				auto roots = scene.GetRootActors();// コピー（描画中の変更に備える）
				for (Actor* root : roots) { DrawHierarchyNode(root, false); }
			}
			ImGui::TreePop();
		}
		ImGui::PopStyleVar();

		// 残りの空いている所: クリックで選択解除、ドロップでルートへ、右クリックで作成
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImGui::InvisibleButton("##empty", ImVec2(std::max(avail.x, 1.0f), std::max(avail.y, ImGui::GetFrameHeight() * 2.0f)));
		if (ImGui::IsItemClicked()) { Select(nullptr); mRenamingId = 0; }
		acceptRootDrop();
		if (ImGui::BeginPopupContextItem("##emptyMenu"))
		{
			DrawHierarchyCreateMenu(nullptr);
			ImGui::EndPopup();
		}

		// ドロップ位置
		if (gDrop.active)
		{
			ImDrawList* dl = ImGui::GetWindowDrawList();
			ImU32 col = ImGui::GetColorU32(ImGuiCol_DragDropTarget);
			if (gDrop.zone == DropZone::Inside)
			{
				dl->AddRect(ImVec2(gDrop.row.Min.x + 1.0f, gDrop.row.Min.y), ImVec2(gDrop.row.Max.x - 1.0f, gDrop.row.Max.y), col, 3.0f, 0, 2.0f);
			}
			else
			{
				float y = gDrop.zone == DropZone::Before ? gDrop.row.Min.y : gDrop.row.Max.y;
				dl->AddCircle(ImVec2(gDrop.x + 3.0f, y), 3.0f, col, 0, 1.5f);
				dl->AddLine(ImVec2(gDrop.x + 6.0f, y), ImVec2(gDrop.row.Max.x, y), col, 2.0f);
			}
		}
		ImGui::EndChild();

		// ---- 後回しにした操作 ----
		if (gOps.move)
		{
			Actor* actor = gOps.move;
			Actor* parent = gOps.moveParent;
			int index = gOps.moveIndex;
			const bool sameParent = actor->GetParent() == parent;
			const int oldIndex = actor->GetSiblingIndex();
			// 同じ親の中で後ろへ動かすときは、自分が抜けた分だけ前に詰まる
			if (sameParent && index > oldIndex) { --index; }
			const int last = int((parent ? parent->GetChildren() : scene.GetRootActors()).size()) - 1;
			const bool unchanged = sameParent && (index < 0 ? oldIndex == last : oldIndex == index);
			if (!unchanged)
			{
				auto command = std::make_unique<ReparentCommand>((sameParent ? "Reorder " : "Reparent ") + actor->GetName(), actor, parent, index);
				command->Redo(scene);
				Select(actor);
				PushCommand(std::move(command));
				mHierarchyLastSelected = 0;// 閉じた親へ入れたときも見えるよう、次のフレームで親を開く
			}
		}
		if (!gOps.instantiate.empty())
		{
			if (Actor* instance = scene.Instantiate(gOps.instantiate, gOps.instParent))
			{
				if (gOps.instIndex >= 0) { instance->SetParent(gOps.instParent, false, gOps.instIndex); }
				Select(instance);
				PushCommand(ActorLifetimeCommand::Created("Instantiate " + GetStem(gOps.instantiate), scene, { instance }));
			}
		}
		if (gOps.rename && !mRenameBuffer.empty() && mRenameBuffer != gOps.rename->GetName())
		{
			Json before = scene.SerializeActorState(gOps.rename);
			gOps.rename->SetName(mRenameBuffer);
			PushCommand(std::make_unique<ActorStateCommand>("Rename", gOps.rename->GetId(), std::move(before), scene.SerializeActorState(gOps.rename)));
		}
		if (gOps.toggleActive)
		{
			// 選択は変えずに切り替える
			Actor* a = gOps.toggleActive;
			Json before = scene.SerializeActorState(a);
			a->SetActive(!a->IsActiveSelf());
			auto command = std::make_unique<ActorStateCommand>(a->IsActiveSelf() ? "Activate" : "Deactivate", a->GetId(), std::move(before), scene.SerializeActorState(a));
			command->SetSelection(mSelectedId);
			PushCommand(std::move(command));
		}
		if (gOps.duplicate) { Select(gOps.duplicate); DuplicateSelected(); }
		if (gOps.remove) { Select(gOps.remove); DeleteSelected(); }
		if (!gOps.createKind.empty()) { CreateActorFromMenu(gOps.createKind, gOps.createParent); }
		if (gOps.focus) { FocusOn(gOps.focus); }
		if (gOps.savePrefab) { CreatePrefab(gOps.savePrefab, mAssetDir); }// アセットパネルで開いているフォルダへ
		mHierarchyReveal = false;
		ImGui::End();
	}

	// ==================================================
	// 1 行
	// ==================================================
	void Editor::DrawHierarchyNode(Actor* actor, bool flat)
	{
		Scene& scene = mEngine.GetScene();
		const ActorId id = actor->GetId();
		const bool hasChildren = !flat && !actor->GetChildren().empty();
		const bool selected = id == mSelectedId;
		const ImGuiStyle& style = ImGui::GetStyle();

		ImGui::PushID(reinterpret_cast<void*>(id));
		ImGuiWindow* window = ImGui::GetCurrentWindow();
		const ImVec2 rowPos = ImGui::GetCursorScreenPos();
		const float rowH = ImGui::GetFrameHeight();
		const ImRect row(ImVec2(window->WorkRect.Min.x, rowPos.y), ImVec2(window->WorkRect.Max.x, rowPos.y + rowH));

		// 縞模様
		if (mHierarchyRowCount++ & 1)
		{
			window->DrawList->AddRectFilled(row.Min, row.Max, ImGui::GetColorU32(ImGuiCol_TableRowBgAlt));
		}

		if (!flat)
		{
			if (mHierarchyReveal)
			{
				if (Actor* sel = GetSelected(); sel && sel->IsDescendantOf(actor)) { ImGui::SetNextItemOpen(true); }
			}
			if (mHierarchyToggleId == id)
			{
				ImGui::SetNextItemOpen(mHierarchyToggleOpen);
				mHierarchyToggleId = 0;
			}
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth |
			ImGuiTreeNodeFlags_FramePadding | ImGuiTreeNodeFlags_AllowOverlap;
		if (!hasChildren) { flags |= ImGuiTreeNodeFlags_Leaf; }
		if (flat) { flags |= ImGuiTreeNodeFlags_NoTreePushOnOpen; }
		if (selected) { flags |= ImGuiTreeNodeFlags_Selected; }
		// フォーカスがないときの選択は灰色（Unity と同じ）
		if (selected && !gFocused) { ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.30f, 0.30f, 0.33f, 1.0f)); }
		const bool open = ImGui::TreeNodeEx("##node", flags, "") && !flat;
		if (selected && !gFocused) { ImGui::PopStyleColor(); }
		const bool treeOpen = open && hasChildren;

		if (selected && mHierarchyReveal) { ImGui::ScrollToItem(ImGuiScrollFlags_KeepVisibleCenterY); }

		// クリックで選択、ダブルクリックでフォーカス
		if (ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		{
			Select(actor);
			if (mRenamingId != id) { mRenamingId = 0; }
		}
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen())
		{
			gOps.focus = actor;
		}

		const float iconX = rowPos.x + ImGui::GetTreeNodeToLabelSpacing();
		const float iconSize = ImGui::GetFontSize();
		const float nameX = iconX + iconSize + style.ItemInnerSpacing.x;
		const float textY = rowPos.y + style.FramePadding.y;
		const ActorIcon icon = GetIcon(actor);

		// ドラッグ（並べ替え・親子付け替え）
		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("ACTOR_ID", &id, sizeof(id));
			ImVec2 p = ImGui::GetCursorScreenPos();
			DrawIcon(ImGui::GetWindowDrawList(), p, iconSize, icon, 1.0f);
			ImGui::Dummy(ImVec2(iconSize, iconSize));
			ImGui::SameLine();
			ImGui::TextUnformatted(actor->GetName().c_str());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiDragDropFlags acceptFlags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
			const DropZone zone = GetDropZone(row, !flat);
			const DropTarget target = ResolveDrop(actor, zone, treeOpen);
			const bool asFirstChild = zone == DropZone::After && target.parent == actor;
			auto showIndicator = [&]()
			{
				gDrop.active = true;
				gDrop.row = row;
				gDrop.zone = zone;
				gDrop.x = rowPos.x + (asFirstChild ? style.IndentSpacing : 0.0f);
			};
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ACTOR_ID", acceptFlags))
			{
				Actor* dragged = scene.FindActor(*static_cast<const ActorId*>(p->Data));
				if (CanMove(dragged, actor, target))
				{
					showIndicator();
					if (p->IsDelivery())
					{
						gOps.move = dragged;
						gOps.moveParent = target.parent;
						gOps.moveIndex = target.index;
					}
				}
			}
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH", acceptFlags); IsPrefabPayload(p))
			{
				showIndicator();
				if (p->IsDelivery())
				{
					gOps.instantiate = static_cast<const char*>(p->Data);
					gOps.instParent = target.parent;
					gOps.instIndex = target.index;
				}
			}
			ImGui::EndDragDropTarget();
		}

		// 右クリックメニュー
		if (ImGui::BeginPopupContextItem())
		{
			if (mSelectedId != id) { Select(actor); }
			if (ImGui::MenuItem("Rename", "F2")) { BeginRename(actor); }
			if (ImGui::MenuItem("Duplicate", "Ctrl+D")) { gOps.duplicate = actor; }
			if (ImGui::MenuItem("Delete", "Del")) { gOps.remove = actor; }
			ImGui::Separator();
			if (ImGui::BeginMenu("Create Child"))
			{
				DrawHierarchyCreateMenu(actor);
				ImGui::EndMenu();
			}
			ImGui::Separator();
			const int sibling = actor->GetSiblingIndex();
			const int count = int((actor->GetParent() ? actor->GetParent()->GetChildren() : scene.GetRootActors()).size());
			if (ImGui::MenuItem("Move Up", nullptr, false, sibling > 0))
			{
				gOps.move = actor; gOps.moveParent = actor->GetParent(); gOps.moveIndex = sibling - 1;
			}
			if (ImGui::MenuItem("Move Down", nullptr, false, sibling < count - 1))
			{
				gOps.move = actor; gOps.moveParent = actor->GetParent(); gOps.moveIndex = sibling + 2;
			}
			if (ImGui::MenuItem("Move to Top", nullptr, false, sibling > 0))
			{
				gOps.move = actor; gOps.moveParent = actor->GetParent(); gOps.moveIndex = 0;
			}
			if (ImGui::MenuItem("Move to Bottom", nullptr, false, sibling < count - 1))
			{
				gOps.move = actor; gOps.moveParent = actor->GetParent(); gOps.moveIndex = -1;
			}
			if (ImGui::MenuItem("Unparent", nullptr, false, actor->GetParent() != nullptr))
			{
				gOps.move = actor; gOps.moveParent = nullptr; gOps.moveIndex = -1;
			}
			ImGui::Separator();
			if (ImGui::MenuItem("Focus", "F / Double-click")) { gOps.focus = actor; }
			if (ImGui::MenuItem("Create Prefab")) { gOps.savePrefab = actor; }
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("Save to %s/\n(or drag the actor into the Assets panel)", mAssetDir.c_str()); }
			ImGui::EndPopup();
		}

		// アイコンと名前
		const bool activeSelf = actor->IsActiveSelf();
		const bool active = actor->IsActive();
		ImDrawList* dl = window->DrawList;
		DrawIcon(dl, ImVec2(iconX, textY), iconSize, icon, active ? 1.0f : 0.4f);

		const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup) && row.Contains(ImGui::GetMousePos()) && !ImGui::GetDragDropPayload();
		const float eyeW = rowH;
		const bool showEye = hovered || !activeSelf;
		float nameRight = row.Max.x - (showEye ? eyeW : 0.0f) - style.FramePadding.x;

		if (mRenamingId == id)
		{
			// 名前の変更
			ImGui::SetCursorScreenPos(ImVec2(nameX - style.FramePadding.x, rowPos.y));
			ImGui::SetNextItemWidth(std::max(40.0f, nameRight - nameX));
			if (mRenameFocus)
			{
				ImGui::SetKeyboardFocusHere();
				mRenameFocus = false;
				gRenameWait = 3;
			}
			ImGui::InputText("##rename", &mRenameBuffer, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_EnterReturnsTrue);
			if (ImGui::IsItemDeactivated())
			{
				if (!ImGui::IsKeyPressed(ImGuiKey_Escape)) { gOps.rename = actor; }
				mRenamingId = 0;
			}
			else if (!ImGui::IsItemActive() && gRenameWait > 0 && --gRenameWait == 0)
			{
				mRenamingId = 0;// 入力欄が有効にならなかった
			}
		}
		else
		{
			ImU32 textCol = ImGui::GetColorU32(active ? ImGuiCol_Text : ImGuiCol_TextDisabled);
			dl->PushClipRect(ImVec2(nameX, row.Min.y), ImVec2(nameRight, row.Max.y), true);
			dl->AddText(ImVec2(nameX, textY), textCol, actor->GetName().c_str());
			if (flat && actor->GetParent())
			{
				// 検索結果では親の名前も添える
				float w = ImGui::CalcTextSize(actor->GetName().c_str()).x;
				std::string parentName = "  (" + actor->GetParent()->GetName() + ")";
				dl->AddText(ImVec2(nameX + w, textY), ImGui::GetColorU32(ImGuiCol_TextDisabled), parentName.c_str());
			}
			dl->PopClipRect();
		}

		// 右端の有効/無効の切り替え（行にマウスがあるとき、または無効のとき）
		if (showEye)
		{
			ImGui::SetCursorScreenPos(ImVec2(row.Max.x - eyeW, rowPos.y));
			if (ImGui::InvisibleButton("##active", ImVec2(eyeW, rowH))) { gOps.toggleActive = actor; }
			bool eyeHovered = ImGui::IsItemHovered();
			if (eyeHovered) { ImGui::SetTooltip(activeSelf ? "Deactivate" : "Activate"); }
			DrawEye(dl, ImVec2(row.Max.x - eyeW * 0.5f, rowPos.y + rowH * 0.5f), rowH, activeSelf, eyeHovered);
		}

		mHierarchyRows.push_back({ id, actor->GetParent() ? actor->GetParent()->GetId() : 0, treeOpen, hasChildren });

		if (open)
		{
			auto children = actor->GetChildren();
			for (Actor* child : children) { DrawHierarchyNode(child, false); }
		}
		if (open) { ImGui::TreePop(); }
		ImGui::PopID();
	}

	// ==================================================
	// 作成メニュー（parent があればその子として作る）
	// ==================================================
	void Editor::DrawHierarchyCreateMenu(Actor* parent)
	{
		auto item = [&](const char* label, const char* kind)
		{
			if (ImGui::MenuItem(label))
			{
				gOps.createKind = kind;
				gOps.createParent = parent;
			}
		};
		item("Create Empty", "Empty");
		ImGui::Separator();
		if (ImGui::BeginMenu("3D Object"))
		{
			for (const char* kind : { "Cube", "Sphere", "Plane", "Capsule" }) { item(kind, kind); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Light"))
		{
			for (const char* kind : { "Directional Light", "Point Light", "Spot Light" }) { item(kind, kind); }
			ImGui::EndMenu();
		}
		if (ImGui::BeginMenu("Effects"))
		{
			item("Particle System", "Particle System");
			ImGui::EndMenu();
		}
		item("Camera", "Camera");
	}

	// ==================================================
	// キー操作（↑↓ 選択、←→ 開閉、F2 名前の変更）
	// ==================================================
	void Editor::BeginRename(Actor* actor)
	{
		if (!actor) { return; }
		Select(actor);
		mHierarchyLastSelected = actor->GetId();// 選択の変更でキャンセルされないように
		mRenamingId = actor->GetId();
		mRenameBuffer = actor->GetName();
		mRenameFocus = true;
	}

	void Editor::HandleHierarchyKeys()
	{
		if (!gFocused || mRenamingId != 0 || ImGui::GetIO().WantTextInput) { return; }

		// 矢印キーは ImGui のナビゲーションと取り合わないよう、このパネルが持つ
		const ImGuiID owner = ImGui::GetID("##hierarchyKeys");
		for (ImGuiKey key : { ImGuiKey_UpArrow, ImGuiKey_DownArrow, ImGuiKey_LeftArrow, ImGuiKey_RightArrow }) { ImGui::SetKeyOwner(key, owner); }
		auto pressed = [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, owner, ImGuiInputFlags_Repeat); };

		if (ImGui::IsKeyPressed(ImGuiKey_F2, false)) { BeginRename(GetSelected()); return; }
		if (mHierarchyRows.empty()) { return; }

		int index = -1;
		for (int i = 0; i < int(mHierarchyRows.size()); ++i)
		{
			if (mHierarchyRows[i].id == mSelectedId) { index = i; break; }
		}
		Scene& scene = mEngine.GetScene();
		auto selectRow = [&](int i)
		{
			if (i >= 0 && i < int(mHierarchyRows.size())) { Select(scene.FindActor(mHierarchyRows[i].id)); }
		};
		if (index < 0)
		{
			if (pressed(ImGuiKey_DownArrow)) { selectRow(0); }
			if (pressed(ImGuiKey_UpArrow)) { selectRow(int(mHierarchyRows.size()) - 1); }
			return;
		}
		const HierarchyRow& row = mHierarchyRows[index];
		if (pressed(ImGuiKey_UpArrow)) { selectRow(index - 1); }
		else if (pressed(ImGuiKey_DownArrow)) { selectRow(index + 1); }
		else if (pressed(ImGuiKey_LeftArrow))
		{
			if (row.open) { mHierarchyToggleId = row.id; mHierarchyToggleOpen = false; }
			else if (row.parent) { Select(scene.FindActor(row.parent)); }
		}
		else if (pressed(ImGuiKey_RightArrow))
		{
			if (row.hasChildren && !row.open) { mHierarchyToggleId = row.id; mHierarchyToggleOpen = true; }
			else if (row.open) { selectRow(index + 1); }
		}
	}
}
