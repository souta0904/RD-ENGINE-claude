#pragma once
#include "Asset/Asset.h"
#include "Core/Serialization/Json.h"
#include "Render/Material/Material.h"

namespace rd
{
	// ファイルに保存したマテリアル（Unity の .mat にあたる。拡張子は .rdmat、中身は JSON）
	//  いくつもの MeshRenderer が同じものを指せて、値を変えると使っている全部の見た目が変わる。
	//  中身は小さいので、読み込みは裏に回さずその場で済ませる（AssetManager::LoadMaterial）。
	//  そのため受け取った時点で値がそろっていて、描画とゲームのコードがすぐ使える。
	//
	//  Unity の .mat は YAML で書かれていて読めないので、拡張子を分けている
	class MaterialAsset final : public Asset
	{
	public:
		static constexpr const char* kExtension = ".rdmat";

		Material material;

		const char* GetTypeName() const override { return "Material"; }

		// 値を JSON にする / JSON から戻す（ファイルの中身、Undo の記録に使う）
		Json ToJson() const;
		void FromJson(const Json& json);
		// 今の値でファイルを書き直す
		bool Save() const;
		// 値を変えるたびに増える（サムネイルを描き直す目安）
		uint32_t GetVersion() const { return mVersion; }
		void MarkChanged() { ++mVersion; }

		// 新しいマテリアルのファイルを作る（もうあれば false）
		static bool CreateFile(const std::string& path, const Material& initial = Material());
		static bool IsMaterialPath(const std::string& path);

	protected:
		void Load() override;

	private:
		uint32_t mVersion = 0;
	};
}
