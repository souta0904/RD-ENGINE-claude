#include "RdPch.h"
#include "Editor/Commands/MaterialAssetCommand.h"
#include "Asset/AssetManager.h"
#include "Core/Util/StringUtil.h"
#include "Render/Material/MaterialAsset.h"

namespace rd
{
	MaterialAssetCommand::MaterialAssetCommand(std::string label, std::string path, Json before, Json after)
		: EditorCommand(std::move(label)), mPath(std::move(path)), mBefore(std::move(before)), mAfter(std::move(after))
	{
	}

	void MaterialAssetCommand::Undo(Scene& /*scene*/)
	{
		Apply(mBefore);
	}

	void MaterialAssetCommand::Redo(Scene& /*scene*/)
	{
		Apply(mAfter);
	}

	// ファイルが消されていたら何もしない（読めなかったアセットへ書くと、消したファイルを作り直してしまう）
	void MaterialAssetCommand::Apply(const Json& values)
	{
		std::error_code ec;
		auto asset = std::filesystem::exists(ToWide(mPath), ec) ? AssetManager::Get().LoadNow<MaterialAsset>(mPath) : nullptr;
		if (!asset || !asset->IsReady())
		{
			Logger::Warning(LogCat::Editor, "Material no longer exists: {}", mPath);
			return;
		}
		asset->FromJson(values);
		asset->Save();
	}
}
