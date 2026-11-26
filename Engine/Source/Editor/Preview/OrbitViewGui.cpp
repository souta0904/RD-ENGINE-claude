#include "RdPch.h"
#include "Editor/Preview/OrbitViewGui.h"
#include "Editor/Preview/PreviewView.h"
#include <ImGui/imgui.h>
#include <ImGui/imgui_internal.h>

namespace rd
{
	void DrawOrbitView(PreviewView& view)
	{
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 size(std::max(1.0f, std::floor(avail.x)), std::max(64.0f, std::floor(avail.y)));
		ImVec2 pos = ImGui::GetCursorScreenPos();
		ImGui::InvisibleButton("##orbitPreview", size);
		bool hovered = ImGui::IsItemHovered();
		ImGuiIO& io = ImGui::GetIO();
		if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 0.0f))
		{
			view.yaw += io.MouseDelta.x * 0.4f;
			view.pitch = std::clamp(view.pitch + io.MouseDelta.y * 0.4f, -89.0f, 89.0f);
		}
		if (hovered && io.MouseWheel != 0.0f) { view.zoom = std::clamp(view.zoom * std::pow(0.88f, io.MouseWheel), 0.2f, 5.0f); }
		if (hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			view.yaw = PreviewView::kDefaultYaw;
			view.pitch = PreviewView::kDefaultPitch;
			view.zoom = 1.0f;
		}
		if (hovered)
		{
			// ホイールでパネルが流れないようにする
			ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);
		}

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 end(pos.x + size.x, pos.y + size.y);
		if (view.Render(uint32_t(size.x), uint32_t(size.y)))
		{
			dl->AddImage(ImTextureID(view.GetOutput().GetGpuSrv().ptr), pos, end);
		}
		dl->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border));
		if (hovered)
		{
			// 狭いときは短い説明にし、それでも収まらなければ切る
			const char* hint = "Drag: rotate   Wheel: zoom   Double-click: reset";
			if (ImGui::CalcTextSize(hint).x + 20.0f > size.x) { hint = "Drag / Wheel / Double-click"; }
			ImVec2 textSize = ImGui::CalcTextSize(hint);
			ImVec2 textPos(pos.x + 6.0f, end.y - textSize.y - 6.0f);
			dl->PushClipRect(pos, end, true);
			dl->AddRectFilled(ImVec2(textPos.x - 4.0f, textPos.y - 2.0f), ImVec2(textPos.x + textSize.x + 4.0f, textPos.y + textSize.y + 2.0f), IM_COL32(0, 0, 0, 140), 3.0f);
			dl->AddText(textPos, IM_COL32(230, 230, 230, 255), hint);
			dl->PopClipRect();
		}
	}
}
