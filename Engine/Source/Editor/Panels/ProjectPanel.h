#pragma once
#include "Editor/Core/AssetPreview.h"
#include "Editor/Panels/EditorPanel.h"
#include "Scene/Actor/Actor.h"
#include <filesystem>
#include <string>
#include <vector>

namespace rd
{
	// プロジェクトパネル（Assets/ の中を見る。左にフォルダの木、右に今のフォルダの中身。画像とモデルは縮小した絵で出す）
	//  ダブルクリックでフォルダへ入る / シーンを開く。ファイルはドラッグしてビューやプロパティへ入れられる
	//  アクターをドロップするとプレハブを作る。右クリックでフォルダの作成・名前の変更・削除（ごみ箱へ）・エクスプローラーで表示
	//  エクスプローラーからファイルを落とすと取り込む（フォルダの上なら そのフォルダ、それ以外は今のフォルダへ）
	class ProjectPanel final : public EditorPanel
	{
	public:
		explicit ProjectPanel(Editor& editor);

		void Update(float dt) override;

		const std::string& GetCurrentFolder() const { return mFolder; }
		const std::string& GetSelectedFile() const { return mSelected; }
		void ClearSelection() { mSelected.clear(); }
		// そのファイルのあるフォルダを開いて選ぶ（Unity の Ping。アクターの選択はそのまま）
		void SelectFile(const std::string& path);
		float GetIconScale() const { return mIconScale; }
		// 前回の状態を戻す（消えたフォルダやファイル、Assets の外を指していたら、その項目は戻さない）
		void RestoreState(const std::string& folder, const std::string& selected, float iconScale);

	protected:
		void OnGui() override;
		ImVec2 GetDefaultSize() const override { return ImVec2(800.0f, 260.0f); }

	private:
		struct Entry
		{
			std::string name;
			std::string path;
			std::string ext;
			bool directory = false;
		};

		void DrawToolbar();
		void DrawFolderTree(const std::filesystem::path& dir, const std::string& path);
		void DrawGrid();
		void DrawEntry(const Entry& e, float cell);
		void DrawEntryMenu(const Entry& e);
		void DrawBackgroundMenu();
		void DrawDialogs();
		void CollectEntries();
		void OpenEntry(const Entry& e);
		void CreateFolder();
		void CreateMaterial();
		void CreateShader();
		void BeginRename(const Entry& e);
		// 直前に描いた項目の上にファイルが落とされていたら、そのフォルダを取り込み先にする
		void CheckFileDropTarget(const std::string& folder);

		std::vector<std::filesystem::path> mDroppedFiles;
		ImVec2 mDropPoint;
		std::string mDropFolder;
		std::string mFolder = "Assets";
		std::string mSelected;
		std::string mFilter;
		float mIconScale = 1.0f;
		std::vector<Entry> mEntries;
		AssetThumbnails mThumbnails;
		ActorId mPrefabActor = 0;
		std::string mPrefabFolder;
		std::string mRenamePath;
		std::string mRenameBuffer;
		std::string mRenameExt;
		bool mOpenRename = false;
		std::string mDeletePath;
		bool mOpenDelete = false;
	};
}
