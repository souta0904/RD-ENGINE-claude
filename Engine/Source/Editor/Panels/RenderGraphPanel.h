#pragma once
#include "Editor/Panels/EditorPanel.h"
#include "Render/Graph/RenderGraph.h"
#include <string>

namespace ax::NodeEditor { struct EditorContext; }

namespace rd
{
	// メインの画面を最後に描いたときのレンダーグラフを見せるパネル（見るだけで、編集はしない）
	//  パスを左から右へ並べ、テクスチャを渡す所を線でつなぐ。飛ばしたパスは薄く出す。
	//  右の欄はテクスチャの一覧（大きさ・形式・使い回している実体の番号）
	class RD_API RenderGraphPanel final : public EditorPanel
	{
	public:
		explicit RenderGraphPanel(Editor& editor);
		~RenderGraphPanel() override;

	protected:
		void OnGui() override;
		bool HasPadding() const override { return false; }
		ImVec2 GetDefaultSize() const override { return ImVec2(ImGui::GetFontSize() * 70.0f, ImGui::GetFontSize() * 36.0f); }
		const char* GetDefaultDockWindow() const override { return "Scene"; }

	private:
		void DrawToolbar(const rg::GraphInfo& info);
		void DrawCanvas(const rg::GraphInfo& info, float width, float height);
		void DrawSidebar(const rg::GraphInfo& info, float width, float height);
		// パスの位置を決める（何に頼っているかの深さで列、同じ列は縦に並べる）
		void Layout(const rg::GraphInfo& info);

		ax::NodeEditor::EditorContext* mContext = nullptr;
		// パスの並びが変わったら並べ直す
		std::string mShape;
		bool mPushPositions = false;
		int mNavigateFrames = 0;
		std::vector<ImVec2> mPositions;
		int mSelectedPass = -1;
		int mHoveredResource = -1;
	};
}
