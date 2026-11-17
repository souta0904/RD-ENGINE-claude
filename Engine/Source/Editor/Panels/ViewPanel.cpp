#include "RdPch.h"
#include "Editor/Panels/ViewPanel.h"
#include "Editor/Style/EditorGui.h"

namespace rd
{
	void ViewPanel::GetRenderSize(uint32_t& width, uint32_t& height) const
	{
		width = std::max(1u, mWidth);
		height = std::max(1u, mHeight);
	}

	bool ViewPanel::IsVisible() const
	{
		return mVisibleFrame == ImGui::GetFrameCount();
	}

	bool ViewPanel::WasVisible() const
	{
		return mVisibleFrame >= ImGui::GetFrameCount() - 1;
	}

	Vector2 ViewPanel::ToViewUv(ImVec2 m) const
	{
		return Vector2((m.x - mImagePos.x) / mImageSize.x, (m.y - mImagePos.y) / mImageSize.y);
	}

	void ViewPanel::BeginViewToolbar()
	{
		ImVec2 padding = ImGui::GetStyle().WindowPadding;
		ImGui::SetCursorPos(ImVec2(padding.x, ImGui::GetCursorPosY() + padding.y));
		EditorGui::BeginToolbar("viewToolbar");
	}

	void ViewPanel::DrawView(float ratio)
	{
		mVisibleFrame = ImGui::GetFrameCount();
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 origin = ImGui::GetCursorScreenPos();
		ImVec2 size(std::max(1.0f, avail.x), std::max(1.0f, avail.y));
		if (ratio > 0.0f)
		{
			if (size.x / size.y > ratio) { size.x = size.y * ratio; }
			else { size.y = size.x / ratio; }
		}
		ImVec2 pos(std::floor(origin.x + (avail.x - size.x) * 0.5f), std::floor(origin.y + (avail.y - size.y) * 0.5f));
		ImGui::GetWindowDrawList()->AddRectFilled(origin, ImVec2(origin.x + avail.x, origin.y + avail.y), IM_COL32(0, 0, 0, 255));
		mWidth = uint32_t(size.x);
		mHeight = uint32_t(size.y);
		mImagePos = pos;
		mImageSize = size;

		ImGui::SetCursorScreenPos(pos);
		const GpuTexture& output = mPrimary ? Renderer::Get().GetOutput() : mOffscreen.output;
		if (output.GetResource()) { ImGui::Image(ImTextureID(output.GetGpuSrv().ptr), size); }
		else { ImGui::Dummy(size); }
		mHovered = ImGui::IsItemHovered();
		mFocused = ImGui::IsWindowFocused();
		// 右・中ボタンで操作し始めたらフォーカスを移す
		if (mHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
		{
			ImGui::SetWindowFocus();
		}
	}
}
