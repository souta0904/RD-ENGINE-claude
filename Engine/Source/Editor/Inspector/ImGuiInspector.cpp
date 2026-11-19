#include "RdPch.h"
#include "Editor/Inspector/ImGuiInspector.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Editor.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Style/EditorStyle.h"
#include "Render/Material/MaterialAsset.h"
#include "Render/Material/ShaderAsset.h"
#include <filesystem>
#include "Scene/Reference/ActorRef.h"
#include "Scene/Reference/ComponentRef.h"
#include "Scene/Scene.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_stdlib.h>
#include <unordered_map>

namespace rd
{
	namespace
	{
		ImGuiSliderFlags SliderFlags(const PropertyMeta& meta)
		{
			return meta.min < meta.max ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
		}

		// クォータニオンをオイラー角で編集すると値が飛びやすいので、最後に表示した角度を覚えておく
		struct EulerCache
		{
			Quaternion q;
			Vector3 degrees;
		};
		std::unordered_map<const void*, EulerCache> gEulerCache;

		// 項目名の訳。"Material 0" のように後ろに番号が付く名前は、番号の前だけを訳す（"マテリアル 0"）
		std::string PropertyText(std::string_view name)
		{
			if (EditorLocale::Has(name)) { return Tr(name); }
			size_t space = name.find_last_of(' ');
			if (space != std::string_view::npos && space + 1 < name.size() &&
				std::all_of(name.begin() + space + 1, name.end(), [](char c) { return c >= '0' && c <= '9'; }))
			{
				std::string_view head = name.substr(0, space);
				if (EditorLocale::Has(head)) { return std::string(Tr(head)) + std::string(name.substr(space)); }
			}
			return std::string(Tr(name)); // 表になければ英語のまま（訳し忘れの一覧に残る）
		}
	}

	float ImGuiInspector::LabelWidth()
	{
		return std::max(ImGui::GetFontSize() * 7.5f, ImGui::GetContentRegionAvail().x * 0.38f);
	}

	void ImGuiInspector::Label(const char* name, const PropertyMeta& meta)
	{
		ImGui::AlignTextToFramePadding();
		const char* tooltip = meta.tooltip ? Tr(meta.tooltip) : nullptr;
		// 表示名があればそれを出し、保存に使う名前は乗せたときに出す（シェーダーの _Color など）
		if (meta.label && *meta.label)
		{
			ImGui::TextUnformatted(meta.label);
			if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s%s%s", name, tooltip ? "\n" : "", tooltip ? tooltip : ""); }
		}
		else
		{
			// 訳した名前を出し、コードやファイルで使う英語の名前は乗せたときに出す（Unity の日本語表示と同じ）
			std::string text = PropertyText(name);
			ImGui::TextUnformatted(text.c_str());
			bool translated = text != name;
			if (ImGui::IsItemHovered() && (translated || tooltip))
			{
				ImGui::SetTooltip("%s%s%s", translated ? name : "", translated && tooltip ? "\n" : "", tooltip ? tooltip : "");
			}
		}
		ImGui::SameLine(LabelWidth());
		ImGui::SetNextItemWidth(-1.0f);
	}

	bool ImGuiInspector::Track(bool changed, bool discrete)
	{
		if (changed) { mChanged = true; }
		// ドラッグや入力は離したとき、チェックボックスなどはすぐに確定する
		if (ImGui::IsItemDeactivatedAfterEdit() || (changed && discrete)) { mCommitted = true; }
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, bool& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		ImGui::BeginDisabled(meta.flags & PropertyMeta::ReadOnly);
		bool changed = Track(ImGui::Checkbox("##v", &value), true);
		ImGui::EndDisabled();
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, int32_t& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		ImGui::BeginDisabled(meta.flags & PropertyMeta::ReadOnly);
		bool changed = Track(ImGui::DragInt("##v", &value, std::max(meta.speed, 0.1f), int(meta.min), int(meta.max)));
		ImGui::EndDisabled();
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, uint32_t& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		ImGui::BeginDisabled(meta.flags & PropertyMeta::ReadOnly);
		uint32_t mn = uint32_t(std::max(0.0f, meta.min)), mx = uint32_t(std::max(0.0f, meta.max));
		bool changed = Track(ImGui::DragScalar("##v", ImGuiDataType_U32, &value, std::max(meta.speed, 0.1f),
			meta.min < meta.max ? &mn : nullptr, meta.min < meta.max ? &mx : nullptr));
		ImGui::EndDisabled();
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, float& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		ImGui::BeginDisabled(meta.flags & PropertyMeta::ReadOnly);
		bool changed;
		if (meta.flags & PropertyMeta::Angle)
		{
			float deg = math::ToDegrees(value);
			changed = ImGui::DragFloat("##v", &deg, meta.speed, 0.0f, 0.0f, "%.1f deg");
			if (changed) { value = math::ToRadians(deg); }
		}
		else
		{
			changed = ImGui::DragFloat("##v", &value, meta.speed, meta.min, meta.max, "%.3f", SliderFlags(meta));
		}
		Track(changed);
		ImGui::EndDisabled();
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, Vector2& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		bool changed = Track(ImGui::DragFloat2("##v", value.Data(), meta.speed, meta.min, meta.max, "%.3f", SliderFlags(meta)));
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, Vector3& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		bool changed = Track(ImGui::DragFloat3("##v", value.Data(), meta.speed, meta.min, meta.max, "%.3f", SliderFlags(meta)));
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, Vector4& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		bool changed = Track(ImGui::DragFloat4("##v", value.Data(), meta.speed, meta.min, meta.max, "%.3f", SliderFlags(meta)));
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, Quaternion& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		auto& cache = gEulerCache[&value];
		if (!(cache.q == value))
		{
			Vector3 e = value.ToEuler();
			cache.degrees = Vector3(math::ToDegrees(e.x), math::ToDegrees(e.y), math::ToDegrees(e.z));
			cache.q = value;
		}
		bool changed = ImGui::DragFloat3("##v", cache.degrees.Data(), meta.speed, 0.0f, 0.0f, "%.1f");
		if (changed)
		{
			value = Quaternion::FromEuler(Vector3(math::ToRadians(cache.degrees.x), math::ToRadians(cache.degrees.y), math::ToRadians(cache.degrees.z)));
			cache.q = value;
		}
		Track(changed);
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, Color& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		ImGuiColorEditFlags flags = ImGuiColorEditFlags_Float | ImGuiColorEditFlags_AlphaPreviewHalf;
		if (meta.flags & PropertyMeta::HDR) { flags |= ImGuiColorEditFlags_HDR; }
		bool changed = Track(ImGui::ColorEdit4("##v", value.Data(), flags));
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::Property(const char* name, std::string& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		if (meta.flags & (PropertyMeta::AssetMaterial | PropertyMeta::AssetShader))
		{
			bool changed = AssetObjectField(value, meta);
			ImGui::PopID();
			return changed;
		}
		ImGui::BeginDisabled(meta.flags & PropertyMeta::ReadOnly);
		bool changed = (meta.flags & PropertyMeta::Multiline)
			? ImGui::InputTextMultiline("##v", &value)
			: ImGui::InputText("##v", &value);
		Track(changed);
		ImGui::EndDisabled();

		// アセットのパスはプロジェクトパネルからドラッグして入れられる
		constexpr uint32_t kAssetFlags = PropertyMeta::AssetTexture | PropertyMeta::AssetModel | PropertyMeta::AssetAudio |
			PropertyMeta::AssetPrefab | PropertyMeta::AssetScene;
		if ((meta.flags & kAssetFlags) && ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ASSET_PATH"))
			{
				value = static_cast<const char*>(payload->Data);
				changed = true;
				mChanged = mCommitted = true;
			}
			ImGui::EndDragDropTarget();
		}
		ImGui::PopID();
		return changed;
	}

	// アセットの欄: [ Wood (Material) ][...] / [ Stripes (Shader) ][...]
	//  プロジェクトパネルから合う種類のファイルをドラッグして入れる（ほかの種類は受け付けない）、
	//  [...] で Assets/ の中の候補の一覧から選ぶ、ダブルクリックでプロジェクトパネルのそのファイルを選ぶ
	//  シェーダーの候補は、Surface.hlsli を使っている .hlsl だけ。空なら標準のシェーダー（Standard）
	bool ImGuiInspector::AssetObjectField(std::string& value, const PropertyMeta& meta)
	{
		const bool shaderField = (meta.flags & PropertyMeta::AssetShader) != 0;
		const char* typeName = Tr(shaderField ? "Shader" : "Material");
		auto accepts = [&](const std::string& path)
			{
				return shaderField ? ShaderAsset::IsSurfaceShaderPath(path) : MaterialAsset::IsMaterialPath(path);
			};
		bool changed = false;
		bool readOnly = (meta.flags & PropertyMeta::ReadOnly) != 0;
		std::error_code ec;
		bool missing = !value.empty() && !std::filesystem::exists(ToWide(value), ec);
		std::string text = value.empty() ? std::string(Tr(shaderField ? "Standard" : "None (Material)"))
			: std::format("{} ({}){}", GetStem(value), typeName, missing ? Tr(" - Missing") : "");

		const float buttonWidth = ImGui::GetFrameHeight() * 1.4f;
		const float fieldWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if (missing) { ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kError); }
		else if (value.empty()) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }
		else { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text)); }
		ImGui::Button(text.c_str(), ImVec2(fieldWidth, 0.0f));
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar();
		if (ImGui::IsItemHovered() && !value.empty()) { ImGui::SetTooltip("%s", value.c_str()); }
		if (!value.empty() && !missing && mEditor && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			mEditor->GetProject().SelectFile(value);
		}

		if (!readOnly && ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* peek = ImGui::GetDragDropPayload();
			if (peek && peek->IsDataType("ASSET_PATH") && accepts(static_cast<const char*>(peek->Data)))
			{
				if (const ImGuiPayload* p = ImGui::AcceptDragDropPayload("ASSET_PATH"))
				{
					value = static_cast<const char*>(p->Data);
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::BeginDisabled(readOnly);
		if (ImGui::Button("...", ImVec2(buttonWidth, 0.0f))) { ImGui::OpenPopup("pickAsset"); }
		ImGui::EndDisabled();
		if (ImGui::BeginPopup("pickAsset"))
		{
			static std::string sFilter;
			static std::vector<std::string> sFound;
			if (ImGui::IsWindowAppearing())
			{
				sFilter.clear();
				ImGui::SetKeyboardFocusHere();
				// 開いたときに 1 回だけ Assets/ の中を探す
				sFound.clear();
				for (auto it = std::filesystem::recursive_directory_iterator(L"Assets", ec); !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec))
				{
					if (!it->is_regular_file(ec)) { continue; }
					std::string path = NormalizePath(ToUtf8(it->path().generic_wstring()));
					// エンジンの組み込みのシェーダーは候補に出さない（Standard を選べば同じ）
					if (shaderField && path.starts_with("Assets/Engine/")) { continue; }
					if (accepts(path)) { sFound.push_back(path); }
				}
				std::sort(sFound.begin(), sFound.end());
			}
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 16.0f);
			ImGui::InputTextWithHint("##filter", Tr("Search"), &sFilter);
			ImGui::Separator();
			ImGui::BeginChild("list", ImVec2(320.0f, 280.0f));
			if (ImGui::Selectable(Tr(shaderField ? "Standard" : "None"), value.empty()))
			{
				value.clear();
				changed = true;
				ImGui::CloseCurrentPopup();
			}
			for (const std::string& path : sFound)
			{
				if (!sFilter.empty() && !ContainsNoCase(path, sFilter)) { continue; }
				std::string label = std::format("{}##{}", GetStem(path), path);
				if (ImGui::Selectable(label.c_str(), path == value))
				{
					value = path;
					changed = true;
					ImGui::CloseCurrentPopup();
				}
				if (ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", path.c_str()); }
			}
			if (sFound.empty())
			{
				ImGui::TextDisabled("%s", Tr(shaderField ? "No surface shaders in Assets/. Create one in the Project panel."
					: "No materials in Assets/. Create one in the Project panel."));
			}
			ImGui::EndChild();
			ImGui::EndPopup();
		}

		if (changed) { mChanged = mCommitted = true; }
		return changed;
	}

	// 参照の欄: [ Player (Actor) ][...]
	//  ヒエラルキーからアクターをドラッグして入れる（型が合わないものは受け付けない）、[...] で候補の一覧から選ぶ、
	//  ダブルクリックで参照先を選ぶ
	bool ImGuiInspector::Property(const char* name, ObjectRefBase& base, const PropertyMeta& meta)
	{
		auto& value = static_cast<ObjectRef&>(base);
		ImGui::PushID(name);
		Label(name, meta);
		bool changed = false;
		Scene* scenePtr = value.GetScene() ? value.GetScene() : (mEditor ? &mEditor->GetScene() : nullptr);
		if (!scenePtr)
		{
			// どのシーンの参照か分からない（エディタなしで使われた）ので表示だけにする
			ImGui::TextDisabled("%s", Tr("(no scene)"));
			ImGui::PopID();
			return false;
		}
		Scene& scene = *scenePtr;
		if (!value.GetScene()) { value.SetScene(&scene); }
		Actor* target = value.GetActor();
		bool readOnly = (meta.flags & PropertyMeta::ReadOnly) != 0;

		std::string text;
		if (target) { text = std::format("{} ({})", target->GetName(), value.GetTypeName()); }
		else if (value.GetId() != 0) { text = EditorLocale::Format("Missing ({})", value.GetTypeName()); }
		else { text = EditorLocale::Format("None ({})", value.GetTypeName()); }

		const float buttonWidth = ImGui::GetFrameHeight() * 1.4f;
		const float fieldWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if (!target && value.GetId() != 0) { ImGui::PushStyleColor(ImGuiCol_Text, EditorStyle::kError); }
		else if (!target) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }
		else { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text)); }
		ImGui::Button(text.c_str(), ImVec2(fieldWidth, 0.0f));
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar();

		if (target && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (mEditor) { mEditor->Select(target); }
		}

		if (!readOnly && ImGui::BeginDragDropTarget())
		{
			const ImGuiPayload* peek = ImGui::GetDragDropPayload();
			if (peek && peek->IsDataType("ACTOR_ID"))
			{
				Actor* dropped = scene.FindActor(*static_cast<const ActorId*>(peek->Data));
				if (value.Accepts(dropped) && ImGui::AcceptDragDropPayload("ACTOR_ID"))
				{
					value.SetActor(dropped);
					changed = true;
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::BeginDisabled(readOnly);
		if (ImGui::Button("...", ImVec2(buttonWidth, 0.0f))) { ImGui::OpenPopup("pick"); }
		ImGui::EndDisabled();
		if (ImGui::BeginPopup("pick"))
		{
			static std::string sFilter;
			if (ImGui::IsWindowAppearing()) { sFilter.clear(); ImGui::SetKeyboardFocusHere(); }
			ImGui::SetNextItemWidth(ImGui::GetFontSize() * 14.0f);
			ImGui::InputTextWithHint("##filter", Tr("Search"), &sFilter);
			ImGui::Separator();
			ImGui::BeginChild("list", ImVec2(260.0f, 280.0f));
			if (ImGui::Selectable(Tr("None"), value.GetId() == 0))
			{
				value.Clear();
				changed = true;
				ImGui::CloseCurrentPopup();
			}
			std::string lowerFilter = ToLower(sFilter);
			scene.ForEachActor([&](Actor* a)
				{
					if (a->IsPendingDestroy() || !value.Accepts(a)) { return; }
					if (!lowerFilter.empty() && ToLower(a->GetName()).find(lowerFilter) == std::string::npos) { return; }
					ImGui::PushID(reinterpret_cast<void*>(a->GetId()));
					if (ImGui::Selectable(a->GetName().c_str(), a == target))
					{
						value.SetActor(a);
						changed = true;
						ImGui::CloseCurrentPopup();
					}
					ImGui::PopID();
				});
			ImGui::EndChild();
			ImGui::EndPopup();
		}

		if (changed) { mChanged = mCommitted = true; }
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::EnumProperty(const char* name, int64_t& value, std::span<const std::string_view> names,
		std::span<const int64_t> values, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		std::string current = "?";
		for (size_t i = 0; i < values.size(); ++i) { if (values[i] == value) { current = PropertyText(names[i]); } }
		bool changed = false;
		if (ImGui::BeginCombo("##v", current.c_str()))
		{
			for (size_t i = 0; i < values.size(); ++i)
			{
				std::string label = std::format("{}##{}", PropertyText(names[i]), names[i]);
				if (ImGui::Selectable(label.c_str(), values[i] == value))
				{
					value = values[i];
					changed = true;
				}
			}
			ImGui::EndCombo();
		}
		Track(changed, true);
		ImGui::PopID();
		return changed;
	}

	bool ImGuiInspector::BeginGroup(const char* name)
	{
		// ID は英語の名前のまま（言語を変えても開いている状態が保たれる）
		return ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanAvailWidth, "%s", PropertyText(name).c_str());
	}

	void ImGuiInspector::SetNextGroupOpen()
	{
		ImGui::SetNextItemOpen(true, ImGuiCond_Once);
	}

	void ImGuiInspector::EndGroup()
	{
		ImGui::TreePop();
	}
}
