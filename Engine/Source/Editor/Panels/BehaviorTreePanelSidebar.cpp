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
