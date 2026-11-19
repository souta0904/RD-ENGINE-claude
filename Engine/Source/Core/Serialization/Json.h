#pragma once
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector4.h"
#include <nlohmann/json.hpp>
#include <string>

namespace rd
{
	// JSON の値（nlohmann/json ライブラリの型）
	//  ordered_json はキーを入れた順に保存する（普通の json はキーの名前順に並べ替える）。
	//  Reflect に並べた順のままファイルに出るので、手で読んだり差分を見たりしやすい
	using Json = nlohmann::ordered_json;

	// 数学型は JSON の数値の配列として保存する（Vector3 なら [1.0, 2.0, 3.0]）
	//  to_json / from_json という名前の関数を同じ名前空間に置くと、nlohmann/json が見つけて使ってくれる
	//  （json["pos"] = v; や j.get<Vector3>() と書けるようになる）
	//  Quaternion は [w, x, y, z] の順
	void to_json(Json& j, const Vector2& v);
	void to_json(Json& j, const Vector3& v);
	void to_json(Json& j, const Vector4& v);
	void to_json(Json& j, const Quaternion& v);
	void to_json(Json& j, const Color& v);
	void from_json(const Json& j, Vector2& v);
	void from_json(const Json& j, Vector3& v);
	void from_json(const Json& j, Vector4& v);
	void from_json(const Json& j, Quaternion& v);
	void from_json(const Json& j, Color& v);

	// ObjectRef は { "$ref": id } として保存する
	//  ただの数と区別できるよう、キーが 1 つだけのオブジェクトにする（$ は普通のプロパティ名に使わない字）
	inline constexpr const char* kRefKey = "$ref";
	bool IsRefJson(const Json& j);

	// ファイルを読んで JSON にする。読めない・書き方が間違っている場合は false（間違いはログに出す）
	bool LoadJsonFile(const std::string& path, Json& out);
	// 字下げ 4 つで読みやすく書く
	bool SaveJsonFile(const std::string& path, const Json& json);
}
