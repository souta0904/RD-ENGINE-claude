#pragma once
#include "Editor/Panels/EditorPanel.h"

namespace rd
{
	struct ProfileFrame;

	// プロファイラ（フレームごとの棒グラフ、選んだフレームのスレッドごとのタイムライン、区間ごとの集計）
	//  棒をクリックするとそのフレームを見られるよう記録を止める。Ctrl + ホイールでタイムラインを横に拡大する
	class RD_API ProfilerPanel final : public EditorPanel
	{
	public:
		explicit ProfilerPanel(Editor& editor);
		const char* GetShortcut() const override { return "F9"; }

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(900.0f, 460.0f); }

	private:
		void DrawToolbar();
		void DrawFrameGraph();
		void DrawTimeline(const ProfileFrame& frame);
		void DrawStatsTable();

		int mSelected = -1;
		int mAverage = 60;
		float mZoom = 1.0f;
	};
}
