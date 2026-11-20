#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Panels/EditorPanel.h"
#include <string>
#include <unordered_map>

namespace rd
{
	class Actor;
	class Component;
	class ImGuiInspector;
	class MaterialAsset;

	// プロパティ（選んだアクターの名前・Transform・コンポーネントを編集する。Unity のインスペクタにあたる）
	//  コンポーネントの見出しをドラッグして並べ替え、右クリックで削除・リセット・値のコピーと貼り付け・並べ替え
	//  アクターを選んでいないときは、プロジェクトパネルで選んだアセットの情報を出す（絵はプレビューパネル）
	class RD_API PropertiesPanel final : public EditorPanel
	{
	public:
		explicit PropertiesPanel(Editor& editor);

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
		// アクターの MeshRenderer に入れたマテリアルのアセットを、コンポーネントの下に並べて編集できるようにする（Unity と同じ）
		void DrawActorMaterials(Actor* actor);
		// マテリアルのアセットの値を編集する。確定したらファイルへ保存し、Undo の記録を積む
		void DrawMaterialEditor(MaterialAsset& asset);
		void DrawShaderInfo(const std::string& path);

		std::string mAddFilter;
		// マテリアルの編集を始める前の値（アセットのパスごと。Undo の「前」に使う）
		std::unordered_map<std::string, Json> mMaterialSnapshots;
		Json mComponentClipboard;
		PendingOps mOps;
		bool mDropLine = false;
		float mDropLineY = 0.0f;
		float mDropLineX0 = 0.0f;
		float mDropLineX1 = 0.0f;
	};
}
