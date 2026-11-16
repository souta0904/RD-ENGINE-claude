#pragma once
#include "Math/Color.h"
#include "Math/Quaternion.h"
#include "Math/Vector4.h"
#include <nlohmann/json.hpp>
#include <string>

namespace rd
{
	using Json = nlohmann::ordered_json;

	// 数学型は JSON の数値の配列として保存する
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
	inline constexpr const char* kRefKey = "$ref";
	bool IsRefJson(const Json& j);

	bool LoadJsonFile(const std::string& path, Json& out);
	bool SaveJsonFile(const std::string& path, const Json& json);
}
