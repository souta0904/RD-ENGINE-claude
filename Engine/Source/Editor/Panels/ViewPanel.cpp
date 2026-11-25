#include "RdPch.h"
#include "Editor/Panels/ViewPanel.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include <ImGui/imgui_internal.h>

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

	void ViewPanel::BeginOverlayToolbar()
	{
		// 台の内側の余白ぶん内へ寄せて置く。台は後ろの層に、部品を並べ終えてから大きさが分かったところで描く
		float margin = IM_ROUND(ImGui::GetFontSize() * 0.6f);
		ImVec2 pad = ImGui::GetStyle().WindowPadding;
		ImGui::SetCursorScreenPos(ImVec2(mViewOrigin.x + margin + pad.x, mViewOrigin.y + margin + pad.y * 0.8f));
		ImDrawList* dl = ImGui::GetWindowDrawList();
		mOverlaySplitter.Split(dl, 2);
		mOverlaySplitter.SetCurrentChannel(dl, 1);
		ImGui::PushID("overlayToolbar");
		ImGui::BeginGroup();
	}

	void ViewPanel::EndOverlayToolbar()
	{
		ImGui::EndGroup();
		ImGui::PopID();
		ImVec2 pad = ImGui::GetStyle().WindowPadding;
		ImVec2 min(ImGui::GetItemRectMin().x - pad.x, ImGui::GetItemRectMin().y - pad.y * 0.8f);
		ImVec2 max(ImGui::GetItemRectMax().x + pad.x, ImGui::GetItemRectMax().y + pad.y * 0.8f);
		ImDrawList* dl = ImGui::GetWindowDrawList();
		mOverlaySplitter.SetCurrentChannel(dl, 0);
		// 半透明の台 → 上の縁に差し色のごく細い線
		const ImVec4& bg = EditorStyle::kBaseBg;
		EditorStyle::DrawVerticalShade(dl, min, max, ImVec4(bg.x + 0.03f, bg.y + 0.032f, bg.z + 0.036f, 0.86f), ImVec4(bg.x, bg.y, bg.z, 0.86f));
		EditorStyle::DrawAccentGradient(dl, min, ImVec2(max.x, min.y + 1.0f), 0.45f);
		mOverlaySplitter.Merge(dl);
		mOverlayMin = min;
		mOverlayMax = max;
	}

	void ViewPanel::DrawView(float ratio)
	{
		mVisibleFrame = ImGui::GetFrameCount();
		ImVec2 avail = ImGui::GetContentRegionAvail();
		ImVec2 origin = ImGui::GetCursorScreenPos();
		mViewOrigin = origin;
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
		// 浮いたツールバーの上（前のフレームの位置）は、画面に乗っていない扱いにする
		ImVec2 m = ImGui::GetMousePos();
		if (m.x >= mOverlayMin.x && m.x < mOverlayMax.x && m.y >= mOverlayMin.y && m.y < mOverlayMax.y) { mHovered = false; }
		mFocused = ImGui::IsWindowFocused();
		// 右・中ボタンで操作し始めたらフォーカスを移す
		if (mHovered && (ImGui::IsMouseClicked(ImGuiMouseButton_Right) || ImGui::IsMouseClicked(ImGuiMouseButton_Middle)))
		{
			ImGui::SetWindowFocus();
		}
	}
}
