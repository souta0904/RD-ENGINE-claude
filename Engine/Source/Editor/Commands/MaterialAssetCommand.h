#pragma once
#include "Core/Serialization/Json.h"
#include "Editor/Commands/EditorCommand.h"

namespace rd
{
	// マテリアルのアセット（.rdmat）の値の変更
	//  前後の値（MaterialAsset::ToJson の形）を持ち、戻すときはアセットへ入れてファイルも書き直す。
	//  シーンは書き換えないので、シーンの「保存していない」印には数えない
	class MaterialAssetCommand final : public EditorCommand
	{
	public:
		MaterialAssetCommand(std::string label, std::string path, Json before, Json after);
		void Undo(Scene& scene) override;
		void Redo(Scene& scene) override;
		bool ChangesScene() const override { return false; }

	private:
		void Apply(const Json& values);

		std::string mPath;
		Json mBefore;
		Json mAfter;
	};
}
