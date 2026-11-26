#include "RdPch.h"
#include "Editor/Panels/PropertiesPanel.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Asset/Texture/Texture.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Commands/MaterialAssetCommand.h"
#include "Editor/Editor.h"
#include "Editor/Inspector/ComponentEditors.h"
#include "Editor/Inspector/ImGuiInspector.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Editor/Core/FileDialog.h"
#include "Render/Material/MaterialAsset.h"
#include "Render/Material/ShaderAsset.h"
#include "Render/Renderer/Renderer.h"
#include "Scene/Component/ComponentRegistry.h"
#include "Scene/Component/MissingComponent.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"
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
			else { EditorGui::EmptyState(Icon::Actor, Tr("Select an actor in the Hierarchy or the Scene view to edit it.")); }
			return;
		}

		mOps = {};
		mOps.label = EditorLocale::Format("Edit {}", actor->GetName());
		ImGuiInspector inspector(&mEditor);
		bool commit = false;
		DrawActorHeader(actor, commit);
		DrawComponents(actor, inspector, commit);
		commit |= inspector.AnyCommitted();
		ApplyPendingOps(actor, commit);
		DrawActorMaterials(actor);
		DrawAddComponent();
		if (!mOps.add.empty())
		{
			actor->AddComponent(mOps.add);
			mOps.label = EditorLocale::Format("Add {}", mOps.add);
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
		if (EditorGui::Checkbox("##active", &active)) { actor->SetActive(active); commit = true; }
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
		bool open = EditorGui::CollapsingHeader("      Transform", ImGuiTreeNodeFlags_DefaultOpen);
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
				if (dynamic_cast<MissingComponent*>(c))
				{
					// 型が見つからない。値は残してあるので、クラスが戻れば元に戻る
					EditorIcons::Inline(Icon::Warning);
					ImGui::SameLine();
					ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kWarning);
					ImGui::TextWrapped("%s", Tr("The component type was not found. Its values are kept and come back when the type is registered again (for example after reloading the game code)."));
					ImGui::PopStyleColor();
				}
				c->Reflect(inspector);
				commit |= ComponentEditors::Draw(*c);
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
		if (dynamic_cast<MissingComponent*>(c)) { label += std::string(" (") + Tr("Missing") + ")"; }
		bool open = EditorGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowOverlap);
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

		// 有効 / 無効のチェックは見出しの中に収まるよう少し小さくし、右端で上下の真ん中に置く
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec2 padding(style.FramePadding.x, style.FramePadding.y * 0.4f);
		float boxSize = ImGui::GetFontSize() + padding.y * 2.0f;
		ImGui::SameLine();
		ImGui::SetCursorScreenPos(ImVec2(headerMax.x - boxSize - style.FramePadding.x, headerMin.y + (headerMax.y - headerMin.y - boxSize) * 0.5f));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, padding);
		bool enabled = c->IsEnabled();
		if (EditorGui::Checkbox("##enabled", &enabled)) { c->SetEnabled(enabled); commit = true; }
		ImGui::PopStyleVar();
		return open;
	}

	void PropertiesPanel::DrawComponentMenu(Component* c, int index, int count)
	{
		if (!ImGui::BeginPopupContextItem("comp")) { return; }
		if (ImGui::MenuItem(Tr("Remove Component"))) { mOps.remove = c; }
		if (ImGui::MenuItem(Tr("Reset"))) { mOps.reset = c; }
		ImGui::Separator();
		if (ImGui::MenuItem(Tr("Copy Component"))) { mComponentClipboard = SaveComponent(c); }
		bool sameType = mComponentClipboard.is_object() && mComponentClipboard.value("Type", "") == c->GetTypeName();
		if (ImGui::MenuItem(Tr("Paste Component Values"), nullptr, false, sameType)) { mOps.paste = c; }
		if (ImGui::MenuItem(Tr("Paste Component As New"), nullptr, false, mComponentClipboard.is_object())) { mOps.paste = c; mOps.pasteAsNew = true; }
		ImGui::Separator();
		if (ImGui::MenuItem(Tr("Move Up"), nullptr, false, index > 0)) { mOps.move = c; mOps.moveIndex = index - 1; }
		if (ImGui::MenuItem(Tr("Move Down"), nullptr, false, index < count - 1)) { mOps.move = c; mOps.moveIndex = index + 1; }
		if (ImGui::MenuItem(Tr("Move to Top"), nullptr, false, index > 0)) { mOps.move = c; mOps.moveIndex = 0; }
		if (ImGui::MenuItem(Tr("Move to Bottom"), nullptr, false, index < count - 1)) { mOps.move = c; mOps.moveIndex = count - 1; }
		ImGui::EndPopup();
	}

	void PropertiesPanel::ApplyPendingOps(Actor* actor, bool& commit)
	{
		if (mOps.remove)
		{
			mOps.label = EditorLocale::Format("Remove {}", mOps.remove->GetTypeName());
			actor->RemoveComponent(mOps.remove);
			mEditor.GetScene().FlushPendingDestroy();
			commit = true;
		}
		if (mOps.move && actor->SetComponentIndex(mOps.move, mOps.moveIndex))
		{
			mOps.label = EditorLocale::Format("Move {}", mOps.move->GetTypeName());
			commit = true;
		}
		// 新品のインスタンスの値で上書きする
		if (mOps.reset)
		{
			if (auto fresh = ComponentRegistry::Create(mOps.reset->GetTypeName()))
			{
				LoadComponent(mOps.reset, SaveComponent(fresh.get()));
				mOps.label = EditorLocale::Format("Reset {}", mOps.reset->GetTypeName());
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
				mOps.label = EditorLocale::Format("Paste {}", type);
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
		if (EditorGui::AccentButton(Tr("Add Component"), ImVec2(w * 0.7f, 0))) { ImGui::OpenPopup("AddComponent"); mAddFilter.clear(); }
		if (!ImGui::BeginPopup("AddComponent")) { return; }
		if (ImGui::IsWindowAppearing()) { ImGui::SetKeyboardFocusHere(); }
		EditorGui::SearchField("##filter", mAddFilter, Tr("Search"), 240.0f);
		std::string category;
		for (const auto& entry : ComponentRegistry::GetEntries())
		{
			if (!mAddFilter.empty() && !ContainsNoCase(entry.name, mAddFilter)) { continue; }
			if (entry.category != category)
			{
				category = entry.category;
				// 分類は訳す。コンポーネントの名前はコードの型の名前なので英語のまま（Unity と同じ）
				EditorGui::SectionLabel(Tr(category));
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
			else { ImGui::TextDisabled("%s", Tr(tex->IsFailed() ? "Failed to load" : "Loading...")); }
		}
		else if (icon == Icon::Model)
		{
			auto model = AssetManager::Get().LoadModel(path);
			if (model->IsReady())
			{
				ImGui::Text("%s: %zu", Tr("Meshes"), model->GetSubMeshes().size());
				ImGui::Text("%s: %zu", Tr("Materials"), model->GetMaterials().size());
				ImGui::Text("%s: %zu", Tr("Animations"), model->GetClips().size());
				ImGui::Text("%s: %s", Tr("Skinned"), Tr(model->HasSkinning() ? "yes" : "no"));
				for (const auto& clip : model->GetClips()) { ImGui::BulletText("%s (%.2fs)", clip->name.c_str(), clip->duration); }
			}
			else { ImGui::TextDisabled("%s", Tr(model->IsFailed() ? "Failed to load" : "Loading...")); }
		}
		else if (icon == Icon::Prefab || icon == Icon::Scene)
		{
			EditorGui::EmptyHint(Tr(icon == Icon::Prefab ? "Drag into the Hierarchy or the Scene view to place it." : "Double-click in the Project panel to open it."));
		}
		else if (icon == Icon::Shader && GetExtension(path) == ".hlsl")
		{
			// 読み込みはキャッシュされるので、毎フレームファイルを読み直さない
			auto shader = AssetManager::Get().LoadNow<ShaderAsset>(path);
			if (shader && shader->IsReady() && ShaderAsset::IsSurfaceShaderSource(shader->GetSource())) { DrawShaderInfo(path); }
			else { EditorGui::EmptyHint(Tr("Engine shader. Surface shaders (that include Surface.hlsli) can be set in a material.")); }
		}
		else if (icon == Icon::Material)
		{
			auto material = AssetManager::Get().LoadNow<MaterialAsset>(path);
			if (!material || !material->IsReady())
			{
				ImGui::TextDisabled("%s", Tr("Failed to load"));
				return;
			}
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextDisabled("%s", Tr("Drag onto a model in the Scene view or the Hierarchy, or onto a material slot of a MeshRenderer."));
			ImGui::PopTextWrapPos();
			ImGui::Spacing();
			DrawMaterialEditor(*material);
		}
	}

	// サーフェスシェーダーの Properties の一覧と書き間違い。開く・読み直すボタン
	void PropertiesPanel::DrawShaderInfo(const std::string& path)
	{
		auto shader = AssetManager::Get().LoadNow<ShaderAsset>(path);
		if (!shader || !shader->IsReady())
		{
			ImGui::TextDisabled("%s", Tr("Failed to load"));
			return;
		}
		if (ImGui::Button(Tr("Open"))) { FileDialog::OpenWithDefaultApp(path); }
		ImGui::SameLine();
		if (ImGui::Button(Tr("Reload Shaders (Ctrl+R)"))) { Renderer::Get().ReloadShaders(); }
		ImGui::PushTextWrapPos(0.0f);
		ImGui::TextDisabled("%s", Tr("Surface shader. Set it in a material's Shader field; the properties below appear in the material."));
		ImGui::PopTextWrapPos();
		ImGui::Spacing();

		for (const std::string& error : shader->GetErrors())
		{
			ImGui::PushTextWrapPos(0.0f);
			ImGui::TextColored(EditorStyle::kError, "%s", error.c_str());
			ImGui::PopTextWrapPos();
		}
		const auto& props = shader->GetProperties();
		if (props.empty())
		{
			ImGui::TextDisabled("%s", Tr("No properties. Add a /* Properties { ... } */ block to expose values."));
			return;
		}
		static const char* const kTypeNames[] = { "Color", "Float", "Range", "Int", "Vector", "2D" };
		if (ImGui::BeginTable("props", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingStretchProp))
		{
			ImGui::TableSetupColumn(Tr("Name"));
			ImGui::TableSetupColumn(Tr("Type"));
			ImGui::TableSetupColumn(Tr("Default"));
			ImGui::TableHeadersRow();
			for (const ShaderProperty& p : props)
			{
				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::TextUnformatted(p.displayName.c_str());
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", p.name.c_str()); }
				ImGui::TableNextColumn();
				std::string type = kTypeNames[int(p.type)];
				if (p.type == ShaderProperty::Type::Range) { type = std::format("Range({:g}, {:g})", p.min, p.max); }
				if (p.hdr) { type = "[HDR] " + type; }
				if (p.toggle) { type = "[Toggle] " + type; }
				ImGui::TextDisabled("%s", type.c_str());
				ImGui::TableNextColumn();
				const Vector4& d = p.defaultValue;
				switch (p.type)
				{
				case ShaderProperty::Type::Color:
				case ShaderProperty::Type::Vector: ImGui::Text("(%g, %g, %g, %g)", d.x, d.y, d.z, d.w); break;
				case ShaderProperty::Type::Texture: ImGui::Text("\"%s\"", p.defaultTexture.c_str()); break;
				default: ImGui::Text("%g", d.x); break;
				}
			}
			ImGui::EndTable();
		}
	}

	// 同じアセットを 2 つの枠に入れていても 1 回だけ出す
	void PropertiesPanel::DrawActorMaterials(Actor* actor)
	{
		std::vector<MaterialAsset*> assets;
		for (MeshRenderer* mr : actor->GetComponents<MeshRenderer>())
		{
			for (size_t i = 0; i < mr->GetMaterialCount(); ++i)
			{
				MaterialAsset* asset = mr->GetSharedMaterial(i);
				if (asset && asset->IsReady() && std::find(assets.begin(), assets.end(), asset) == assets.end()) { assets.push_back(asset); }
			}
		}
		for (MaterialAsset* asset : assets)
		{
			ImGui::PushID(asset->GetPath().c_str());
			std::string label = std::format("      {} ({})###{}", GetStem(asset->GetPath()), Tr("Material"), asset->GetPath());
			bool open = EditorGui::CollapsingHeader(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen);
			ImVec2 min = ImGui::GetItemRectMin(), max = ImGui::GetItemRectMax();
			float iconSize = EditorStyle::IconSize();
			EditorIcons::Draw(ImGui::GetWindowDrawList(), ImVec2(min.x + ImGui::GetTreeNodeToLabelSpacing(), min.y + (max.y - min.y - iconSize) * 0.5f), iconSize, Icon::Material);
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s\n%s", asset->GetPath().c_str(), Tr("Shared asset: changes apply to every renderer that uses it.")); }
			if (open)
			{
				ImGui::Indent(4.0f);
				DrawMaterialEditor(*asset);
				ImGui::Unindent(4.0f);
			}
			ImGui::PopID();
		}
	}

	// ドラッグ中は値だけを変え（見た目はすぐ変わる）、離したときにファイルへ保存して Undo に積む。
	//  編集していない間は、Undo などで外から変わった値に「前」の記録を合わせておく
	void PropertiesPanel::DrawMaterialEditor(MaterialAsset& asset)
	{
		const std::string& path = asset.GetPath();
		auto [it, inserted] = mMaterialSnapshots.try_emplace(path);
		if (inserted) { it->second = asset.ToJson(); }

		ImGuiInspector inspector(&mEditor);
		ImGui::PushID("material");
		asset.material.Reflect(inspector);
		ImGui::PopID();
		if (inspector.AnyChanged()) { asset.MarkChanged(); }
		if (inspector.AnyCommitted())
		{
			Json after = asset.ToJson();
			if (after != it->second)
			{
				asset.Save();
				auto command = std::make_unique<MaterialAssetCommand>(EditorLocale::Format("Edit Material {}", GetStem(path)), path, it->second, after);
				command->SetSelection(mEditor.GetSelectedId());
				mEditor.PushCommand(std::move(command));
			}
			it->second = std::move(after);
		}
		else if (!inspector.AnyChanged() && !ImGui::IsAnyItemActive())
		{
			it->second = asset.ToJson();
		}
	}
}
