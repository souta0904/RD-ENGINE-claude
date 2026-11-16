#pragma once
#include "Editor/Panels/EditorPanel.h"

namespace rd
{
	// 描画の設定（影・ポストエフェクト）と、シーンの環境（空・環境光・霧）、物理と時間の設定
	//  シーンの設定の変更は Undo の履歴に積む
	class LightingPanel final : public EditorPanel
	{
	public:
		explicit LightingPanel(Editor& editor);

	protected:
		void OnGui() override;
	};
}
