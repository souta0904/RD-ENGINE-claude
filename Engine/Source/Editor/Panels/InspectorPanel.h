#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Core/AssetPreview.h"
#include "Editor/Panels/EditorPanel.h"
#include <string>

namespace rd
{
	class Actor;
	class Component;
	class ImGuiInspector;

	// インスペクタ（選んだアクターの名前・Transform・コンポーネントを編集する）
	//  コンポーネントの見出しをドラッグして並べ替え、右クリックで削除・リセット・値のコピーと貼り付け・並べ替え
	//  アクターを選んでいないときは、プロジェクトパネルで選んだアセットの情報を出す
	class InspectorPanel final : public EditorPanel
	{
	public:
		explicit InspectorPanel(Editor& editor);

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(360.0f, 600.0f); }

	private:
		// 見出しの操作は描き終えてから行う（moveIndex は動かした後の位置）
		struct PendingOps
		{
			Component* remove = nullptr;
			Component* reset = nullptr;
			Component* move = nullptr;
			int moveIndex = -1;
			Component* paste = nullptr;
			bool pasteAsNew = false;
			std::string add;
			std::string label;
		};

		void DrawActorHeader(Actor* actor, bool& commit);
		void DrawComponents(Actor* actor, ImGuiInspector& inspector, bool& commit);
		bool DrawComponentHeader(Component* c, int index, int count, bool& commit);
		void DrawComponentMenu(Component* c, int index, int count);
		void DrawAddComponent();
		void ApplyPendingOps(Actor* actor, bool& commit);
		void DrawAssetInfo(const std::string& path);

		std::string mAddFilter;
		Json mComponentClipboard;
		PendingOps mOps;
		ModelPreviewWidget mModelPreview;
		bool mDropLine = false;
		float mDropLineY = 0.0f;
		float mDropLineX0 = 0.0f;
		float mDropLineX1 = 0.0f;
	};
}
