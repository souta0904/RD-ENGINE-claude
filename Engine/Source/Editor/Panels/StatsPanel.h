#pragma once
#include "Editor/Panels/EditorPanel.h"
#include <vector>

namespace rd
{
	// 統計（FPS のグラフ、描画・システム・読み込んだアセットの数）
	class RD_API StatsPanel final : public EditorPanel
	{
	public:
		explicit StatsPanel(Editor& editor);
		void Update(float dt) override;

	protected:
		void OnGui() override;

	private:
		std::vector<float> mFrameTimes;
	};
}
