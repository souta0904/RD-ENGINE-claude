#pragma once
#include "Editor/Panels/EditorPanel.h"
#include "Math/Vector2.h"
#include "Render/Camera/RenderCamera.h"
#include "Render/Renderer/Renderer.h"

namespace rd
{
	// ゲームの画面を出すパネル（Scene / Game）の共通部分
	//  1 フレームにメインの描画（Renderer の結果）を使えるのは 1 つだけで、どちらが使うかは Editor が決める（primary）。
	//  もう片方も見えていれば、自分の描画先へ別に描いてもらう（Editor::RenderExtraViews）
	class ViewPanel : public EditorPanel
	{
	public:
		using EditorPanel::EditorPanel;

		void GetRenderSize(uint32_t& width, uint32_t& height) const;
		float GetAspect() const { return float(std::max(1u, mWidth)) / float(std::max(1u, mHeight)); }
		// このフレームに描かれた（タブの裏に隠れていない）
		bool IsVisible() const;
		// 前のフレームに描かれた（このフレームの primary を決めるときに使う）
		bool WasVisible() const;
		bool IsHovered() const { return mHovered; }
		bool IsFocused() const { return mFocused; }
		bool IsPrimary() const { return mPrimary; }
		void SetPrimary(bool primary) { mPrimary = primary; }

		virtual RenderCamera GetViewCamera(float aspect) const = 0;
		RenderTargets& GetOffscreenTargets() { return mOffscreen; }

	protected:
		ImGuiWindowFlags GetWindowFlags() const override { return ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse; }
		bool HasPadding() const override { return false; }

		// ツールバーを始める（パネルに余白がないので、余白を足してから）
		void BeginViewToolbar();
		// 残りの広さに画面を描き、マウスの状態を覚える（ratio > 0 ならその縦横比で中央に置き、余りは黒い帯）
		void DrawView(float ratio);
		Vector2 ToViewUv(ImVec2 mouse) const;

		uint32_t mWidth = 1280;
		uint32_t mHeight = 720;
		ImVec2 mImagePos = ImVec2(0.0f, 0.0f);
		ImVec2 mImageSize = ImVec2(1.0f, 1.0f);
		bool mHovered = false;
		bool mFocused = false;

	private:
		bool mPrimary = false;
		int mVisibleFrame = -10;
		RenderTargets mOffscreen;
	};
}
