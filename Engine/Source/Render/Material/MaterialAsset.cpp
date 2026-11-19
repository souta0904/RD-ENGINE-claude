#include "RdPch.h"
#include "Render/Material/MaterialAsset.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Core/Util/StringUtil.h"

namespace rd
{
	namespace
	{
		// ファイルの先頭に置く印（後で中身の形を変えたときに見分けるため）
		constexpr int kFormatVersion = 1;
	}

	Json MaterialAsset::ToJson() const
	{
		Json json = Json::object();
		json["Format"] = "RDMaterial";
		json["Version"] = kFormatVersion;
		Json values = Json::object();
		JsonWriter writer(values);
		// Reflect は書き換えもできる形なので、書き出すときは写しを使う
		Material copy = material;
		copy.Reflect(writer);
		json["Material"] = std::move(values);
		return json;
	}

	// 無い項目は既定値にする（前の値を残さない。Undo で古い記録を戻すときも同じ結果になるように）
	void MaterialAsset::FromJson(const Json& json)
	{
		Material loaded;
		auto it = json.find("Material");
		if (it != json.end() && it->is_object())
		{
			JsonReader reader(*it);
			loaded.Reflect(reader);
		}
		// 読み込み済みのテクスチャは、パスが同じなら使い回す（ResolveTextures が比べて決める）
		loaded.albedo = material.albedo;
		loaded.normal = material.normal;
		loaded.emissiveMap = material.emissiveMap;
		material = std::move(loaded);
		MarkChanged();
	}

	bool MaterialAsset::Save() const
	{
		if (!SaveJsonFile(mPath, ToJson()))
		{
			Log::Error(LogCat::Asset, "Failed to save material: {}", mPath);
			return false;
		}
		return true;
	}

	void MaterialAsset::Load()
	{
		Json json;
		if (!LoadJsonFile(mPath, json))
		{
			Log::Error(LogCat::Asset, "Failed to load material: {}", mPath);
			SetState(AssetState::Failed);
			return;
		}
		FromJson(json);
		SetState(AssetState::Ready);
	}

	bool MaterialAsset::CreateFile(const std::string& path, const Material& initial)
	{
		std::error_code ec;
		if (std::filesystem::exists(ToWide(path), ec)) { return false; }
		MaterialAsset asset;
		asset.mPath = path;
		asset.material = initial;
		return asset.Save();
	}

	bool MaterialAsset::IsMaterialPath(const std::string& path)
	{
		return ToLower(GetExtension(path)) == kExtension;
	}
}
