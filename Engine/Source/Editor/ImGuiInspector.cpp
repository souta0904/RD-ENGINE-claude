#include "RdPch.h"
#include "Editor/ImGuiInspector.h"
#include "Core/StringUtil.h"
#include "Editor/Editor.h"
#include "Engine.h"
#include "Scene/ObjectRef.h"
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
	}

	float ImGuiInspector::LabelWidth()
	{
		return std::max(110.0f, ImGui::GetContentRegionAvail().x * 0.38f);
	}

	void ImGuiInspector::Label(const char* name, const PropertyMeta& meta)
	{
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(name);
		if (meta.tooltip && ImGui::IsItemHovered()) { ImGui::SetTooltip("%s", meta.tooltip); }
		ImGui::SameLine(LabelWidth());
		ImGui::SetNextItemWidth(-1.0f);
	}

	bool ImGuiInspector::Track(bool changed, bool discrete)
	{
		if (changed) { mChanged = true; }
		// ドラッグや入力は「離したとき」、チェックボックスなどは即座に確定
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
		ImGui::BeginDisabled(meta.flags & PropertyMeta::ReadOnly);
		bool changed = (meta.flags & PropertyMeta::Multiline)
			? ImGui::InputTextMultiline("##v", &value)
			: ImGui::InputText("##v", &value);
		Track(changed);
		ImGui::EndDisabled();

		// アセットパスはアセットパネルからドラッグ&ドロップできる
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

	// 参照フィールド: [ Player (Actor) ][...]
	//  - ヒエラルキーのアクターをドラッグ&ドロップ（型が合わないものは受け付けない）
	//  - [...] で候補の一覧から選ぶ / None に戻す
	//  - ダブルクリックで参照先を選択
	bool ImGuiInspector::Property(const char* name, ObjectRef& value, const PropertyMeta& meta)
	{
		ImGui::PushID(name);
		Label(name, meta);
		bool changed = false;
		Scene& scene = value.GetScene() ? *value.GetScene() : Engine::Get().GetScene();
		if (!value.GetScene()) { value.SetScene(&scene); }
		Actor* target = value.GetActor();
		bool readOnly = (meta.flags & PropertyMeta::ReadOnly) != 0;

		std::string text;
		if (target) { text = std::format("{} ({})", target->GetName(), value.GetTypeName()); }
		else if (value.GetId() != 0) { text = std::format("Missing ({})", value.GetTypeName()); }
		else { text = std::format("None ({})", value.GetTypeName()); }

		const float buttonWidth = ImGui::GetFrameHeight() * 1.4f;
		const float fieldWidth = std::max(1.0f, ImGui::GetContentRegionAvail().x - buttonWidth - ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_FrameBg));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgHovered));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImGui::GetStyleColorVec4(ImGuiCol_FrameBgActive));
		ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign, ImVec2(0.0f, 0.5f));
		if (!target && value.GetId() != 0) { ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f)); }
		else if (!target) { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled)); }
		else { ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_Text)); }
		ImGui::Button(text.c_str(), ImVec2(fieldWidth, 0.0f));
		ImGui::PopStyleColor(4);
		ImGui::PopStyleVar();

		if (target && ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			if (Editor* editor = Engine::Get().GetEditor()) { editor->Select(target); }
		}

		// ヒエラルキーからのドラッグ&ドロップ
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

		// 候補の一覧
		ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
		ImGui::BeginDisabled(readOnly);
		if (ImGui::Button("...", ImVec2(buttonWidth, 0.0f))) { ImGui::OpenPopup("pick"); }
		ImGui::EndDisabled();
		if (ImGui::BeginPopup("pick"))
		{
			static std::string sFilter;
			if (ImGui::IsWindowAppearing()) { sFilter.clear(); ImGui::SetKeyboardFocusHere(); }
			ImGui::SetNextItemWidth(260.0f);
			ImGui::InputTextWithHint("##filter", "Search", &sFilter);
			ImGui::Separator();
			ImGui::BeginChild("list", ImVec2(260.0f, 280.0f));
			if (ImGui::Selectable("None", value.GetId() == 0))
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
		for (size_t i = 0; i < values.size(); ++i) { if (values[i] == value) { current = std::string(names[i]); } }
		bool changed = false;
		if (ImGui::BeginCombo("##v", current.c_str()))
		{
			for (size_t i = 0; i < values.size(); ++i)
			{
				std::string label(names[i]);
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
		return ImGui::TreeNodeEx(name, ImGuiTreeNodeFlags_SpanAvailWidth);
	}

	void ImGuiInspector::EndGroup()
	{
		ImGui::TreePop();
	}
}
