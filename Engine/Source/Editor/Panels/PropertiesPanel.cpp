#include "RdPch.h"
#include "Editor/Panels/PropertiesPanel.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Asset/Texture/Texture.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Editor.h"
#include "Editor/Inspector/ImGuiInspector.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Scene/Component/ComponentRegistry.h"
#include "Scene/Scene.h"
#include <ImGui/imgui_stdlib.h>

namespace rd
{
	namespace
	{
		Json SaveComponent(Component* c)
		{
			Json j = Json::object();
			j["Type"] = c->GetTypeName();
			JsonWriter w(j);
			c->Reflect(w);
			return j;
		}

		void LoadComponent(Component* c, const Json& j)
		{
			JsonReader r(j);
			c->Reflect(r);
			c->OnValidate();
		}
	}

	PropertiesPanel::PropertiesPanel(Editor& editor) : EditorPanel(editor, "Properties") {}

	void PropertiesPanel::OnGui()
	{
		Actor* actor = mEditor.GetSelected();
		if (!actor)
		{
			const std::string& asset = mEditor.GetProject().GetSelectedFile();
			if (!asset.empty()) { DrawAssetInfo(asset); }
			else { EditorGui::EmptyHint("Select an actor in the Hierarchy or the Scene view to edit it."); }
			return;
		}

		mOps = {};
		mOps.label = "Edit " + actor->GetName();
		ImGuiInspector inspector(&mEditor);
		bool commit = false;
		DrawActorHeader(actor, commit);
		DrawComponents(actor, inspector, commit);
		commit |= inspector.AnyCommitted();
		ApplyPendingOps(actor, commit);
		DrawAddComponent();
		if (!mOps.add.empty())
		{
			actor->AddComponent(mOps.add);
			mOps.label = "Add " + mOps.add;
			commit = true;
		}
		if (commit) { mEditor.CommitActorEdit(mOps.label); }
	}

	// アイコン・有効 / 無効・名前を 1 行に、その下にタグとレイヤー
	void PropertiesPanel::DrawActorHeader(Actor* actor, bool& commit)
	{
		EditorIcons::Inline(EditorIcons::ForActor(actor));
		ImGui::SameLine();
		bool active = actor->IsActiveSelf();
		if (ImGui::Checkbox("##active", &active)) { actor->SetActive(active); commit = true; }
		ImGui::SameLine();
		std::string name = actor->GetName();
		ImGui::SetNextItemWidth(-1.0f);
		if (ImGui::InputText("##name", &name)) { actor->SetName(name); }
		if (ImGui::IsItemDeactivatedAfterEdit()) { commit = true; }

		ImGuiInspector header(&mEditor);
		std::string tag = actor->GetTag();
		if (header.Property("Tag", tag)) { actor->SetTag(tag); }
		uint32_t layer = actor->GetLayer();
		if (header.Property("Layer", layer, { .speed = 0.1f, .min = 0.0f, .max = 31.0f })) { actor->SetLayer(layer); }
		commit |= header.AnyCommitted();
		ImGui::TextDisabled("ID %llu", static_cast<unsigned long long>(actor->GetId()));
		ImGui::Spacing();
	}

	void PropertiesPanel::DrawComponents(Actor* actor, ImGuiInspector& inspector, bool& commit)
	{
		ImGui::PushID("Transform");
		bool open = ImGui::CollapsingHeader("      Transform", ImGuiTreeNodeFlags_DefaultOpen);
		ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
		float iconSize = EditorStyle::IconSize();
		EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(min.x + ImGui::GetTreeNodeToLabelSpacing(), min.y + (max.y - min.y - iconSize) * 0.5f), iconSize, Icon::Move);
		if (open) { actor->GetTransform().Reflect(inspector); }
		ImGui::PopID();

		mDropLine = false;
		const auto& components = actor->GetAllComponents();
		const int count = int(components.size());
		for (int index = 0; index < count; ++index)
		{
			Component* c = components[size_t(index)].get();
			ImGui::PushID(c);
			if (DrawComponentHeader(c, index, count, commit))
			{
				inspector.ResetFrameState();
				ImGui::Indent(4.0f);
				c->Reflect(inspector);
				c->OnInspectorGUI();
				ImGui::Unindent(4.0f);
				if (inspector.AnyChanged()) { c->OnValidate(); }
				commit |= inspector.AnyCommitted();
				inspector.ResetFrameState();
			}
			ImGui::PopID();
		}
		if (mDropLine)
		{
			ImGui::GetWindowDrawList()->AddLine(ImVec2(mDropLineX0, mDropLineY), ImVec2(mDropLineX1, mDropLineY), ImGui::GetColorU32(ImGuiCol_DragDropTarget), 2.0f);
		}
	}

	// 見出しをドラッグして並べ替える（上半分へのドロップで前、下半分で後ろ）。右端に有効 / 無効、左にアイコン
	bool PropertiesPanel::DrawComponentHeader(Component* c, int index, int count, bool& commit)
	{
		std::string label = std::string("      ") + c->GetTypeName();
		bool open = ImGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
		const ImVec2 headerMin = ImGui::GetItemRectMin(), headerMax = ImGui::GetItemRectMax();
		float iconSize = EditorStyle::IconSize();
		EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(headerMin.x + ImGui::GetTreeNodeToLabelSpacing(), headerMin.y + (headerMax.y - headerMin.y - iconSize) * 0.5f),
			iconSize, EditorIcons::ForComponent(c));

		if (ImGui::BeginDragDropSource())
		{
			ImGui::SetDragDropPayload("COMPONENT_INDEX", &index, sizeof(index));
			EditorIcons::Inline(EditorIcons::ForComponent(c));
			ImGui::SameLine();
			ImGui::Text("%s", c->GetTypeName());
			ImGui::EndDragDropSource();
		}
		if (ImGui::BeginDragDropTarget())
		{
			const ImGuiDragDropFlags flags = ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect;
			if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("COMPONENT_INDEX", flags))
			{
				const int from = *static_cast<const int*>(p->Data);
				const bool after = ImGui::GetMousePos().y > (headerMin.y + headerMax.y) * 0.5f;
				int to = index + (after ? 1 : 0);
				// 自分が抜けた分だけ前に詰まる
				if (to > from) { --to; }
				if (from >= 0 && from < count && to != from)
				{
					mDropLine = true;
					mDropLineY = after ? headerMax.y : headerMin.y;
					mDropLineX0 = headerMin.x;
					mDropLineX1 = headerMax.x;
					if (p->IsDelivery())
					{
						mOps.move = c->GetActor()->GetAllComponents()[size_t(from)].get();
						mOps.moveIndex = to;
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
		DrawComponentMenu(c, index, count);

		ImGui::SameLine(ImGui::GetContentRegionAvail().x - 4.0f);
		bool enabled = c->IsEnabled();
		if (ImGui::Checkbox("##enabled", &enabled)) { c->SetEnabled(enabled); commit = true; }
		return open;
	}

	void PropertiesPanel::DrawComponentMenu(Component* c, int index, int count)
	{
		if (!ImGui::BeginPopupContextItem("comp")) { return; }
		if (ImGui::MenuItem("Remove Component")) { mOps.remove = c; }
		if (ImGui::MenuItem("Reset")) { mOps.reset = c; }
		ImGui::Separator();
		if (ImGui::MenuItem("Copy Component")) { mComponentClipboard = SaveComponent(c); }
		bool sameType = mComponentClipboard.is_object() && mComponentClipboard.value("Type", "") == c->GetTypeName();
		if (ImGui::MenuItem("Paste Component Values", nullptr, false, sameType)) { mOps.paste = c; }
		if (ImGui::MenuItem("Paste Component As New", nullptr, false, mComponentClipboard.is_object())) { mOps.paste = c; mOps.pasteAsNew = true; }
		ImGui::Separator();
		if (ImGui::MenuItem("Move Up", nullptr, false, index > 0)) { mOps.move = c; mOps.moveIndex = index - 1; }
		if (ImGui::MenuItem("Move Down", nullptr, false, index < count - 1)) { mOps.move = c; mOps.moveIndex = index + 1; }
		if (ImGui::MenuItem("Move to Top", nullptr, false, index > 0)) { mOps.move = c; mOps.moveIndex = 0; }
		if (ImGui::MenuItem("Move to Bottom", nullptr, false, index < count - 1)) { mOps.move = c; mOps.moveIndex = count - 1; }
		ImGui::EndPopup();
	}

	void PropertiesPanel::ApplyPendingOps(Actor* actor, bool& commit)
	{
		if (mOps.remove)
		{
			mOps.label = std::format("Remove {}", mOps.remove->GetTypeName());
			actor->RemoveComponent(mOps.remove);
			mEditor.GetScene().FlushPendingDestroy();
			commit = true;
		}
		if (mOps.move && actor->SetComponentIndex(mOps.move, mOps.moveIndex))
		{
			mOps.label = std::format("Move {}", mOps.move->GetTypeName());
			commit = true;
		}
		// 新品のインスタンスの値で上書きする
		if (mOps.reset)
		{
			if (auto fresh = ComponentRegistry::Create(mOps.reset->GetTypeName()))
			{
				LoadComponent(mOps.reset, SaveComponent(fresh.get()));
				mOps.label = std::format("Reset {}", mOps.reset->GetTypeName());
				commit = true;
			}
		}
		if (mOps.paste)
		{
			std::string type = mComponentClipboard.value("Type", "");
			Component* target = mOps.paste;
			if (mOps.pasteAsNew) { target = actor->AddComponent(type); }
			if (target)
			{
				LoadComponent(target, mComponentClipboard);
				mOps.label = std::format("Paste {}", type);
				commit = true;
			}
		}
	}

	void PropertiesPanel::DrawAddComponent()
	{
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		float w = ImGui::GetContentRegionAvail().x;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + w * 0.15f);
		ImGui::PushStyleColor(ImGuiCol_Button, EditorStyle::kAccentDim);
		if (ImGui::Button("Add Component", ImVec2(w * 0.7f, 0))) { ImGui::OpenPopup("AddComponent"); mAddFilter.clear(); }
		ImGui::PopStyleColor();
		if (!ImGui::BeginPopup("AddComponent")) { return; }
		if (ImGui::IsWindowAppearing()) { ImGui::SetKeyboardFocusHere(); }
		EditorGui::SearchField("##filter", mAddFilter, "Search", 240.0f);
		std::string category;
		for (const auto& entry : ComponentRegistry::GetEntries())
		{
			if (!mAddFilter.empty() && !ContainsNoCase(entry.name, mAddFilter)) { continue; }
			if (entry.category != category)
			{
				category = entry.category;
				EditorGui::SectionLabel(category.c_str());
			}
			if (ImGui::Selectable(entry.name.c_str()))
			{
				mOps.add = entry.name;
				ImGui::CloseCurrentPopup();
			}
		}
		ImGui::EndPopup();
	}

	// アセットの種類・大きさと、テクスチャなら縦横の大きさ、モデルなら中身の数を出す（絵はプレビューパネル）
	void PropertiesPanel::DrawAssetInfo(const std::string& path)
	{
		std::string ext = GetExtension(path);
		EditorIcons::Inline(EditorIcons::ForFile(ext, false));
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(GetFileName(path).c_str());
		ImGui::TextDisabled("%s", path.c_str());
		std::error_code ec;
		auto size = std::filesystem::file_size(ToWide(path), ec);
		if (!ec) { ImGui::TextDisabled("%.1f KB", double(size) / 1024.0); }
		ImGui::Separator();

		Icon icon = EditorIcons::ForFile(ext, false);
		if (icon == Icon::Image)
		{
			auto tex = AssetManager::Get().LoadTexture(path);
			if (tex->IsReady()) { ImGui::Text("%u x %u", tex->GetWidth(), tex->GetHeight()); }
			else { ImGui::TextDisabled(tex->IsFailed() ? "Failed to load" : "Loading..."); }
		}
		else if (icon == Icon::Model)
		{
			auto model = AssetManager::Get().LoadModel(path);
			if (model->IsReady())
			{
				ImGui::Text("Meshes: %zu", model->GetSubMeshes().size());
				ImGui::Text("Materials: %zu", model->GetMaterials().size());
				ImGui::Text("Animations: %zu", model->GetClips().size());
				ImGui::Text("Skinned: %s", model->HasSkinning() ? "yes" : "no");
				for (const auto& clip : model->GetClips()) { ImGui::BulletText("%s (%.2fs)", clip->name.c_str(), clip->duration); }
			}
			else { ImGui::TextDisabled(model->IsFailed() ? "Failed to load" : "Loading..."); }
		}
		else if (icon == Icon::Prefab || icon == Icon::Scene)
		{
			EditorGui::EmptyHint(icon == Icon::Prefab ? "Drag into the Hierarchy or the Scene view to place it." : "Double-click in the Project panel to open it.");
		}
	}
}
