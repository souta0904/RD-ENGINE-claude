#pragma once
#include "Editor/Preview/MaterialPreviewWidget.h"
#include "Editor/Preview/ModelPreviewWidget.h"
#include "Editor/Preview/PrefabPreviewWidget.h"
#include "Editor/Panels/EditorPanel.h"
#include "Editor/Style/EditorIcons.h"
#include <string>

namespace rd
{
	// プレビュー（プロジェクトパネルで選んだテクスチャ・モデル・プレハブの絵を出す）
	//  アクターを選んでいるときは、そのアクターの MeshRenderer のモデルを出す
	class RD_API PreviewPanel final : public EditorPanel
	{
	public:
		explicit PreviewPanel(Editor& editor);
		void OnGameCodeReloaded() override { mPrefabPreview.Reload(); }

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(360.0f, 360.0f); }

	private:
		std::string GetTargetPath(Icon& icon) const;

		ModelPreviewWidget mModelPreview;
		PrefabPreviewWidget mPrefabPreview;
		MaterialPreviewWidget mMaterialPreview;
	};
}
