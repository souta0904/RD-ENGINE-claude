#pragma once
#include "Editor/Panels/ViewPanel.h"

namespace rd
{
	// ゲームビュー（Unity の Game）
	//  シーンのメインカメラで描く。縦横比を選べ、プレイ中にフォーカスしているときだけゲームに入力が届く
	class RD_API GameViewPanel final : public ViewPanel
	{
	public:
		explicit GameViewPanel(Editor& editor);

		void Update(float dt) override;
		// メインカメラがなければ決まった位置から見る（FrameComposer と同じ）
		RenderCamera GetViewCamera(float aspect) const override;

	protected:
		void OnGui() override;

	private:
		void DrawToolbar();

		int mAspect = 0;
	};
}
