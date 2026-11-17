#pragma once
#include "Editor/Core/AssetPreview.h"
#include "Editor/Panels/EditorPanel.h"
#include "Editor/Style/EditorIcons.h"
#include <string>

namespace rd
{
	// プレビュー（プロジェクトパネルで選んだテクスチャ・モデルの絵を出す）
	//  アクターを選んでいるときは、そのアクターの MeshRenderer のモデルを出す
	class PreviewPanel final : public EditorPanel
	{
	public:
		explicit PreviewPanel(Editor& editor);

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(360.0f, 360.0f); }

	private:
		std::string GetTargetPath(Icon& icon) const;

		ModelPreviewWidget mModelPreview;
	};
}
