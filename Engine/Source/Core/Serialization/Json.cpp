#include "RdPch.h"
#include "Core/Serialization/Json.h"
#include "Core/Util/FileUtil.h"

namespace rd
{
	void to_json(Json& j, const Vector2& v) { j = Json::array({ v.x, v.y }); }
	void to_json(Json& j, const Vector3& v) { j = Json::array({ v.x, v.y, v.z }); }
	void to_json(Json& j, const Vector4& v) { j = Json::array({ v.x, v.y, v.z, v.w }); }
	void to_json(Json& j, const Quaternion& v) { j = Json::array({ v.w, v.x, v.y, v.z }); }
	void to_json(Json& j, const Color& v) { j = Json::array({ v.r, v.g, v.b, v.a }); }

	namespace
	{
		// 配列から count 個の float を読む。out は構造体の最初のメンバ（&v.x）を指し、
		// メンバが float で隙間なく並んでいることを使って out[i] で順に書き込む
		// 配列が短ければ、足りない分は元の値のまま（古いファイルで要素が少なくても読める）
		// 配列でなければ例外を投げ、呼んだ側（JsonReader::Read）が警告を出して読み飛ばす
		void ReadFloats(const Json& j, float* out, size_t count)
		{
			if (!j.is_array()) { throw std::runtime_error("expected array"); }
			for (size_t i = 0; i < count && i < j.size(); ++i) { out[i] = j[i].get<float>(); }
		}
	}

	// Quaternion は w が最初のメンバなので、&v.w から 4 つ読むと [w, x, y, z] の順になる
	void from_json(const Json& j, Vector2& v) { ReadFloats(j, &v.x, 2); }
	void from_json(const Json& j, Vector3& v) { ReadFloats(j, &v.x, 3); }
	void from_json(const Json& j, Vector4& v) { ReadFloats(j, &v.x, 4); }
	void from_json(const Json& j, Quaternion& v) { ReadFloats(j, &v.w, 4); }
	void from_json(const Json& j, Color& v) { ReadFloats(j, &v.r, 4); }

	bool IsRefJson(const Json& j)
	{
		return j.is_object() && j.size() == 1 && j.contains(kRefKey);
	}

	// Json::parse は書き方が間違っていると例外を投げる（「3 行目の 5 文字目に , がない」のような内容）
	// 手で書き換えたファイルが壊れていてもエンジンが落ちないよう、ここで捕まえてログに出す
	bool LoadJsonFile(const std::string& path, Json& out)
	{
		std::string text;
		if (!ReadTextFile(path, text)) { return false; }
		try
		{
			out = Json::parse(text);
			return true;
		}
		catch (const std::exception& e)
		{
			Logger::Error(LogCat::Core, "Failed to parse JSON '{}': {}", path, e.what());
			return false;
		}
	}

	// dump(4) … 字下げを空白 4 つにして文字列にする（引数なしだと 1 行に詰める）
	bool SaveJsonFile(const std::string& path, const Json& json)
	{
		return WriteTextFile(path, json.dump(4));
	}
}
