#include "RdPch.h"
#include "Editor/Panels/BehaviorTreeGui.h"
#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Core/EditorLocale.h"
#include "Editor/Style/EditorStyle.h"

namespace rd::btgui
{
	using bt::NodeKind;

	ImVec4 KindColor(const bt::NodeTypeInfo* info, const std::string& type)
	{
		if (!info) { return EditorStyle::kError; }
		switch (info->kind)
		{
		case NodeKind::Root: return ImVec4(0.62f, 0.64f, 0.70f, 1.0f);
		case NodeKind::Composite: return EditorStyle::kAccent;
		case NodeKind::Decorator: return ImVec4(0.72f, 0.56f, 0.90f, 1.0f);
		case NodeKind::Branch:
		case NodeKind::Switch: return ImVec4(0.95f, 0.66f, 0.36f, 1.0f);
		case NodeKind::Leaf: return type == bt::builtin::kCheck ? ImVec4(0.93f, 0.82f, 0.38f, 1.0f) : ImVec4(0.50f, 0.74f, 1.00f, 1.0f);
		}
		return EditorStyle::kAccent;
	}

	const char* KindLabel(const bt::NodeTypeInfo* info)
	{
		if (!info) { return "Unknown"; }
		switch (info->kind)
		{
		case NodeKind::Root: return "Root";
		case NodeKind::Composite: return "Composite";
		case NodeKind::Decorator: return "Decorator";
		case NodeKind::Branch:
		case NodeKind::Switch: return "Flow";
		case NodeKind::Leaf: return info->category.c_str();
		}
		return "";
	}

	std::string SlotLabel(const bt::NodeData& node, const bt::NodeTypeInfo* info, int slot)
	{
		if (!info) { return {}; }
		if (info->kind == NodeKind::Branch) { return Tr(slot == 0 ? "True" : "False"); }
		if (info->kind == NodeKind::Switch)
		{
			if (const bt::SelectorInfo* sel = bt::Registry::FindSelector(bt::GetString(node.params, "Value")))
			{
				if (slot < int(sel->cases.size())) { return Tr(sel->cases[slot]); }
			}
			return std::to_string(slot);
		}
		return {};
	}

	std::string ValueText(const Json& v)
	{
		if (v.is_string()) { return Tr(v.get<std::string>()); }
		if (v.is_boolean()) { return Tr(v.get<bool>() ? "On" : "Off"); }
		if (v.is_number_float()) { return std::format("{:g}", v.get<float>()); }
		if (v.is_number()) { return std::to_string(v.get<int64_t>()); }
		return {};
	}

	std::string Summary(const bt::NodeData& node, const bt::NodeTypeInfo* info)
	{
		if (!info) { return {}; }
		std::string out;
		for (const bt::ParamDef& p : info->params)
		{
			auto it = node.params.find(p.name);
			if (it == node.params.end()) { continue; }
			std::string part;
			switch (p.type)
			{
			case bt::ParamType::Condition:
			{
				std::string name = it->is_string() ? it->get<std::string>() : std::string();
				part = name.empty() ? std::string(Tr("(no condition)")) : std::string(Tr(name));
				if (bt::GetBool(node.params, "Invert")) { part += std::string(" ") + Tr("(inverted)"); }
				break;
			}
			case bt::ParamType::Selector:
			{
				std::string name = it->is_string() ? it->get<std::string>() : std::string();
				part = name.empty() ? std::string(Tr("(no value)")) : std::string(Tr(name));
				break;
			}
			case bt::ParamType::Bool:
				if (p.name == "Invert") { continue; }
				if (info->kind == NodeKind::Root) { part = it->get<bool>() ? Tr("Repeat") : Tr("Once"); break; }
				part = std::format("{}: {}", Tr(p.name), ValueText(*it));
				break;
			default:
				if (node.type == bt::builtin::kWait) { part = std::format("{} s", ValueText(*it)); break; }
				if (node.type == bt::builtin::kLog) { part = "\"" + ValueText(*it) + "\""; break; }
				part = std::format("{}: {}", Tr(p.name), ValueText(*it));
				break;
			}
			if (!out.empty()) { out += "\n"; }
			out += part;
		}
		return out;
	}

	void AutoLayout(bt::TreeAsset& asset)
	{
		const float fs = ImGui::GetFontSize();
		asset.AutoLayout(fs * 12.0f, fs * 7.5f);
	}

	bool Contains(const std::string& text, const std::string& filter)
	{
		return filter.empty() || ToLower(text).find(ToLower(filter)) != std::string::npos;
	}
}
