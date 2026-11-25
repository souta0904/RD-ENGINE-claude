#include "RdPch.h"
#include "Editor/Panels/GameViewPanel.h"
#include "Asset/AssetManager.h"
#include "Components/Rendering/CameraComponent.h"
#include "Editor/Editor.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Engine.h"
#include "Platform/Input/Input.h"
#include "Scene/Scene.h"
#include "Editor/Core/EditorLocale.h"

namespace rd
{
	namespace
	{
		struct AspectOption
		{
			const char* label;
			float ratio;
		};
		constexpr AspectOption kAspects[] = { { "Free Aspect", 0.0f }, { "16:9", 16.0f / 9.0f }, { "16:10", 16.0f / 10.0f }, { "4:3", 4.0f / 3.0f }, { "1:1", 1.0f } };
	}

	GameViewPanel::GameViewPanel(Editor& editor) : ViewPanel(editor, "Game") {}

	// プレイ中にゲームビューへフォーカスしているときだけ、ゲームにキーとマウスを渡す
	void GameViewPanel::Update(float)
	{
		bool gameHasFocus = mEditor.IsPlaying() && mFocused && IsVisible();
		Input::SetBlocked(!gameHasFocus || ImGui::GetIO().WantTextInput, !gameHasFocus);
	}

	RenderCamera GameViewPanel::GetViewCamera(float aspect) const
	{
		if (CameraComponent* main = CameraComponent::FindMain(mEditor.GetScene())) { return main->GetRenderCamera(aspect); }
		return RenderCamera::Make({ 0.0f, 3.0f, -10.0f }, Quaternion::FromEuler({ math::ToRadians(10.0f), 0.0f, 0.0f }),
			math::ToRadians(60.0f), aspect, 0.1f, 1000.0f);
	}

	void GameViewPanel::OnGui()
	{
		DrawView(kAspects[mAspect].ratio);

		ImDrawList* dl = ImGui::GetWindowDrawList();
		ImVec2 pos = mImagePos, size = mImageSize;
		if (!CameraComponent::FindMain(mEditor.GetScene()))
		{
			dl->AddText(ImVec2(pos.x + 10.0f, std::max(pos.y, GetOverlayBottom()) + 8.0f), ImGui::ColorConvertFloat4ToU32(EditorStyle::kTextDim), Tr("No camera in the scene"));
		}
		if (mEditor.IsPlaying())
		{
			ImVec4 border = mEditor.GetEngine().IsPaused() ? EditorStyle::kWarning : EditorStyle::kSuccess;
			dl->AddRect(pos, ImVec2(pos.x + size.x, pos.y + size.y), ImGui::ColorConvertFloat4ToU32(border), 0.0f, 0, 2.0f);
		}

		// ツールバーは画面の上に浮かせる
		BeginOverlayToolbar();
		DrawToolbar();
		EndOverlayToolbar();
	}

	void GameViewPanel::DrawToolbar()
	{
		ImGui::SetNextItemWidth(ImGui::GetFontSize() * 9.0f);
		if (ImGui::BeginCombo("##aspect", Tr(kAspects[mAspect].label)))
		{
			for (int i = 0; i < int(std::size(kAspects)); ++i)
			{
				if (ImGui::Selectable(Tr(kAspects[i].label), i == mAspect)) { mAspect = i; }
			}
			ImGui::EndCombo();
		}
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextDisabled("%u x %u", mWidth, mHeight);
		if (mEditor.IsPlaying())
		{
			ImGui::SameLine();
			ImGui::TextDisabled("%s", Tr(mFocused ? "  Input: game" : "  Click to give input to the game"));
		}
	}
}
