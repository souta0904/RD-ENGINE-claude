#include "RdPch.h"
#include "Editor/Panels/PreviewPanel.h"
#include "Asset/AssetManager.h"
#include "Asset/Model/Model.h"
#include "Asset/Texture/Texture.h"
#include "Components/Rendering/MeshRenderer.h"
#include "Core/Util/StringUtil.h"
#include "Editor/Editor.h"
#include "Editor/Panels/ProjectPanel.h"
#include "Editor/Style/EditorGui.h"
#include "Editor/Style/EditorStyle.h"
#include "Editor/Core/EditorLocale.h"

namespace rd
{
	PreviewPanel::PreviewPanel(Editor& editor) : EditorPanel(editor, "Preview") {}

	// アクターのモデルは組み込みの形（拡張子がない）こともあるので、拡張子によらずモデルとして扱う
	std::string PreviewPanel::GetTargetPath(Icon& icon) const
	{
		const std::string& asset = mEditor.GetProject().GetSelectedFile();
		if (!asset.empty())
		{
			icon = EditorIcons::ForFile(GetExtension(asset), false);
			return asset;
		}
		if (Actor* actor = mEditor.GetSelected())
		{
			if (auto mr = actor->GetComponent<MeshRenderer>(); mr && !mr->modelPath.empty())
			{
				icon = Icon::Model;
				return mr->modelPath;
			}
		}
		icon = Icon::File;
		return {};
	}

	void PreviewPanel::OnGui()
	{
		Icon icon = Icon::File;
		std::string path = GetTargetPath(icon);
		if (icon != Icon::Image && icon != Icon::Model && icon != Icon::Prefab && icon != Icon::Material)
		{
			EditorGui::EmptyState(Icon::Image, Tr("Select a texture, a model, a prefab or a material in the Project panel."));
			return;
		}

		EditorIcons::Inline(icon);
		ImGui::SameLine();
		ImGui::AlignTextToFramePadding();
		ImGui::TextUnformatted(GetFileName(path).c_str());

		if (icon == Icon::Image)
		{
			auto tex = AssetManager::Get().LoadTexture(path);
			if (!tex->IsReady())
			{
				ImGui::TextDisabled("%s", Tr(tex->IsFailed() ? "Failed to load" : "Loading..."));
				return;
			}
			ImGui::SameLine();
			ImGui::TextDisabled("%u x %u", tex->GetWidth(), tex->GetHeight());
			DrawTexturePreview(*tex, std::max(ImGui::GetContentRegionAvail().y, 64.0f));
			return;
		}
		if (icon == Icon::Prefab)
		{
			mPrefabPreview.Draw(path);
			return;
		}
		if (icon == Icon::Material)
		{
			mMaterialPreview.Draw(path);
			return;
		}

		auto model = AssetManager::Get().LoadModel(path);
		if (!model->IsReady())
		{
			ImGui::TextDisabled("%s", Tr(model->IsFailed() ? "Failed to load" : "Loading..."));
			return;
		}
		mModelPreview.Draw(path);
	}
}
