#pragma once
#include "Editor/Preview/PreviewView.h"
#include <memory>
#include <string>

namespace rd
{
	class Scene;

	// プレハブを描く（プレビュー専用の小さなシーンに置いて、そのシーンの描画物をそのまま描く）
	//  エンジンの形式（JSON）のプレハブだけ読める。Unity のプレハブ（YAML）は読めない
	class RD_API PrefabView final : public PreviewView
	{
	public:
		PrefabView();
		~PrefabView() override;

		void SetPrefab(const std::string& path);
		// 同じプレハブを置き直す（ゲームのコードを読み直したとき）
		void Reload();
		const std::string& GetPath() const { return mPath; }
		// 読めなかったときの理由（読めたら空）
		const std::string& GetError() const { return mError; }
		size_t GetActorCount() const;
		bool IsReady() const override;
		bool IsFullyLoaded() override;
		bool Render(uint32_t width, uint32_t height) override;

	private:
		AABB ComputeBounds() const;

		std::unique_ptr<Scene> mScene;
		std::string mPath;
		std::string mError;
	};
}
