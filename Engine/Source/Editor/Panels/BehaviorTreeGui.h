#pragma once
#include "AI/BehaviorTree/BehaviorTreeAsset.h"
#include "AI/BehaviorTree/BehaviorTreeRegistry.h"
#include <ImGui/imgui_node_editor.h>
#include <string>

namespace rd::btgui
{
	// 行動ツリーのパネル（BehaviorTreePanel*.cpp）で共通に使う小さな関数
	namespace ed = ax::NodeEditor;

	// 番号の作り方: ノード = id*64、入口 = id*64+1、出口 = id*64+2+出口の番号、つなぎ線 = 子の id*64+63
	//  （子の親は 1 つだけなので、つなぎ線は子の番号で決まる）
	constexpr uint64_t kStride = 64;
	constexpr int kMaxSlots = 60;
	inline ed::NodeId NodeKey(uint32_t id) { return ed::NodeId(uint64_t(id) * kStride); }
	inline ed::PinId InputPin(uint32_t id) { return ed::PinId(uint64_t(id) * kStride + 1); }
	inline ed::PinId OutputPin(uint32_t id, int slot) { return ed::PinId(uint64_t(id) * kStride + 2 + uint64_t(slot)); }
	inline ed::LinkId LinkKey(uint32_t child) { return ed::LinkId(uint64_t(child) * kStride + 63); }
	inline uint32_t IdOf(uint64_t key) { return uint32_t(key / kStride); }
	inline int PinSlot(uint64_t pin) { return int(pin % kStride) - 2; } // -1 なら入口

	inline ImVec2 Add(const ImVec2& a, const ImVec2& b) { return ImVec2(a.x + b.x, a.y + b.y); }
	inline ImU32 ToU32(const ImVec4& c) { return ImGui::ColorConvertFloat4ToU32(c); }
	inline ImVec4 WithAlpha(ImVec4 c, float a) { c.w = a; return c; }

	// ノードの色（種類ごと）
	ImVec4 KindColor(const bt::NodeTypeInfo* info, const std::string& type);
	// ノードの上に出す種類の名前
	const char* KindLabel(const bt::NodeTypeInfo* info);
	// 出口の名前（Branch は True / False、Switch は選び方の値の名前）
	std::string SlotLabel(const bt::NodeData& node, const bt::NodeTypeInfo* info, int slot);
	// 設定値 1 つを短い文字にする
	std::string ValueText(const Json& v);
	// ノードに出す設定値の要約（例: "IsTargetFound"・"1 s"・"Vision: Caution"）
	std::string Summary(const bt::NodeData& node, const bt::NodeTypeInfo* info);
	// ノードの幅は文字の大きさで決まる（DrawNode）ので、並べる間隔も文字の大きさに合わせる
	void AutoLayout(bt::TreeAsset& asset);
	// filter が空か、text に（大文字小文字を区別せずに）含まれる
	bool Contains(const std::string& text, const std::string& filter);
}
