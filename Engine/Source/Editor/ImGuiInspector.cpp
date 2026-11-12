#include "RdPch.h"
#include "Editor/ImGuiInspector.h"
#include "Core/StringUtil.h"
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
