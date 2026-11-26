#include "RdPch.h"
#include "Editor/Preview/TexturePreview.h"
#include "Asset/Texture/Texture.h"
#include <ImGui/imgui.h>

namespace rd
{
	void DrawTexturePreview(const Texture& texture, float maxHeight)
	{
		float texW = float(std::max(1u, texture.GetWidth()));
		float texH = float(std::max(1u, texture.GetHeight()));
		float availW = std::max(1.0f, ImGui::GetContentRegionAvail().x);
		float scale = std::min(availW / texW, maxHeight / texH);
		ImVec2 size(texW * scale, texH * scale);
		ImVec2 pos = ImGui::GetCursorScreenPos();
		pos.x += std::floor((availW - size.x) * 0.5f);
		ImGui::SetCursorScreenPos(pos);
		ImGui::InvisibleButton("##texturePreview", size);
		bool hovered = ImGui::IsItemHovered();
		ImVec2 end(pos.x + size.x, pos.y + size.y);

		// 透明なところが分かるように市松模様を敷く
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const float cell = 8.0f;
		dl->PushClipRect(pos, end, true);
		dl->AddRectFilled(pos, end, IM_COL32(70, 70, 70, 255));
		for (float y = pos.y; y < end.y; y += cell)
		{
			int row = int((y - pos.y) / cell);
			for (float x = pos.x + ((row & 1) ? cell : 0.0f); x < end.x; x += cell * 2.0f)
			{
				dl->AddRectFilled(ImVec2(x, y), ImVec2(x + cell, y + cell), IM_COL32(100, 100, 100, 255));
			}
		}
		ImTextureID id = ImTextureID(texture.GetSrv().ptr);
		dl->AddImage(id, pos, end);
		dl->PopClipRect();
		dl->AddRect(pos, end, ImGui::GetColorU32(ImGuiCol_Border));

		// マウスの下を 4 倍に拡大して、ピクセルの位置と一緒に出す
		if (hovered)
		{
			ImVec2 m = ImGui::GetMousePos();
			float u = std::clamp((m.x - pos.x) / size.x, 0.0f, 1.0f);
			float v = std::clamp((m.y - pos.y) / size.y, 0.0f, 1.0f);
			const float zoom = 4.0f, loupe = 160.0f;
			float hu = loupe / (zoom * scale * 2.0f) / texW;
			float hv = loupe / (zoom * scale * 2.0f) / texH;
			ImGui::BeginTooltip();
			ImGui::Text("%d, %d", int(u * texW), int(v * texH));
			ImVec2 p = ImGui::GetCursorScreenPos();
			ImGui::Dummy(ImVec2(loupe, loupe));
			ImDrawList* tl = ImGui::GetWindowDrawList();
			tl->AddRectFilled(p, ImVec2(p.x + loupe, p.y + loupe), IM_COL32(70, 70, 70, 255));
			tl->AddImage(id, p, ImVec2(p.x + loupe, p.y + loupe), ImVec2(u - hu, v - hv), ImVec2(u + hu, v + hv));
			ImGui::EndTooltip();
		}
	}
}
