#include "RdPch.h"
#include "Render/Material/ShaderAsset.h"
#include "Core/Util/FileUtil.h"
#include "Core/Util/StringUtil.h"
#include <regex>

namespace rd
{
	namespace
	{
		std::atomic<uint32_t> gNextShaderId = 1;

		std::string Trim(const std::string& s)
		{
			size_t b = s.find_first_not_of(" \t\r\n");
			if (b == std::string::npos) { return {}; }
			size_t e = s.find_last_not_of(" \t\r\n");
			return s.substr(b, e - b + 1);
		}

		bool ParseFloat(const std::string& text, float& out)
		{
			try
			{
				size_t used = 0;
				std::string t = Trim(text);
				out = std::stof(t, &used);
				return used == t.size();
			}
			catch (const std::exception&)
			{
				return false;
			}
		}

		// "(1, 0.5, 0.2, 1)" → 4 つの数（足りない分は 0。色の a は 1）
		bool ParseTuple(const std::string& text, Vector4& out, bool color)
		{
			std::string t = Trim(text);
			if (t.size() < 2 || t.front() != '(' || t.back() != ')') { return false; }
			auto parts = Split(t.substr(1, t.size() - 2), ',');
			if (parts.empty() || parts.size() > 4) { return false; }
			float v[4] = { 0.0f, 0.0f, 0.0f, color ? 1.0f : 0.0f };
			for (size_t i = 0; i < parts.size(); ++i)
			{
				if (!ParseFloat(parts[i], v[i])) { return false; }
			}
			out = Vector4(v[0], v[1], v[2], v[3]);
			return true;
		}

		bool IsIdentifier(const std::string& s)
		{
			static const std::regex kIdent(R"([A-Za-z_][A-Za-z0-9_]*)");
			return std::regex_match(s, kIdent);
		}
	}

	// 1 行の形: [属性]... 名前 ("表示名", 種類) = 既定値
	//  種類: Color / Float / Range(最小, 最大) / Int / Vector / 2D
	void ShaderAsset::ParseProperties(const std::string& source, std::vector<ShaderProperty>& outProperties, std::vector<std::string>& outErrors)
	{
		outProperties.clear();
		outErrors.clear();
		static const std::regex kBlock(R"(/\*\s*Properties\s*\{([\s\S]*?)\}\s*\*/)");
		std::smatch block;
		if (!std::regex_search(source, block, kBlock)) { return; }
		// ブロックの中身が始まる行（エラーの行番号を元のファイルの行に合わせる）
		size_t bodyStart = size_t(block.position(1));
		int firstLine = 1 + int(std::count(source.begin(), source.begin() + bodyStart, '\n'));

		// 中に )" を含むので、生の文字列の区切りを re にしている
		static const std::regex kLine(
			R"re(^((?:\s*\[\w+\])*)\s*(\w+)\s*\(\s*"([^"]*)"\s*,\s*(\w+)\s*(?:\(\s*([^,()]+)\s*,\s*([^,()]+)\s*\))?\s*\)\s*=\s*(.+?)\s*$)re");
		static const std::regex kAttr(R"re(\[(\w+)\])re");
		static const std::regex kTexDefault(R"re(^"(\w*)"\s*(\{\s*\})?$)re");

		auto lines = Split(block.str(1), '\n');
		int textureCount = 0;
		for (size_t li = 0; li < lines.size(); ++li)
		{
			int lineNo = firstLine + int(li);
			std::string line = Trim(lines[li]);
			if (line.empty() || line.starts_with("//")) { continue; }
			auto error = [&](const std::string& what) { outErrors.push_back(std::format("line {}: {} ({})", lineNo, what, line)); };

			std::smatch m;
			if (!std::regex_match(line, m, kLine))
			{
				error("expected: _Name (\"Display Name\", Type) = default");
				continue;
			}
			ShaderProperty p;
			p.name = m.str(2);
			p.displayName = m.str(3).empty() ? p.name : m.str(3);
			std::string attrs = m.str(1);
			for (std::sregex_iterator it(attrs.begin(), attrs.end(), kAttr), end; it != end; ++it)
			{
				std::string a = ToLower((*it).str(1));
				if (a == "hdr") { p.hdr = true; }
				else if (a == "toggle") { p.toggle = true; }
			}
			if (!IsIdentifier(p.name))
			{
				error("invalid name");
				continue;
			}
			if (std::any_of(outProperties.begin(), outProperties.end(), [&](const ShaderProperty& q) { return q.name == p.name; }))
			{
				error("duplicate name");
				continue;
			}

			std::string type = ToLower(m.str(4));
			std::string def = m.str(7);
			bool ok = true;
			if (type == "color")
			{
				p.type = ShaderProperty::Type::Color;
				ok = ParseTuple(def, p.defaultValue, true);
			}
			else if (type == "vector")
			{
				p.type = ShaderProperty::Type::Vector;
				ok = ParseTuple(def, p.defaultValue, false);
			}
			else if (type == "float")
			{
				p.type = ShaderProperty::Type::Float;
				ok = ParseFloat(def, p.defaultValue.x);
			}
			else if (type == "int" || type == "integer")
			{
				p.type = ShaderProperty::Type::Int;
				ok = ParseFloat(def, p.defaultValue.x);
				p.defaultValue.x = std::round(p.defaultValue.x);
			}
			else if (type == "range")
			{
				p.type = ShaderProperty::Type::Range;
				if (!m[5].matched || !ParseFloat(m.str(5), p.min) || !ParseFloat(m.str(6), p.max) || p.min > p.max)
				{
					error("Range needs (min, max)");
					continue;
				}
				ok = ParseFloat(def, p.defaultValue.x);
				p.defaultValue.x = std::clamp(p.defaultValue.x, p.min, p.max);
			}
			else if (type == "2d" || type == "texture")
			{
				p.type = ShaderProperty::Type::Texture;
				std::smatch t;
				std::string d = Trim(def);
				if (std::regex_match(d, t, kTexDefault))
				{
					p.defaultTexture = t.str(1).empty() ? "white" : ToLower(t.str(1));
					if (p.defaultTexture != "white" && p.defaultTexture != "black" && p.defaultTexture != "gray" && p.defaultTexture != "bump")
					{
						error("texture default must be \"white\", \"black\", \"gray\" or \"bump\"");
						continue;
					}
				}
				else { ok = false; }
				if (ok && textureCount >= kMaxTextures)
				{
					error(std::format("too many textures (max {})", kMaxTextures));
					continue;
				}
				if (ok) { p.textureSlot = textureCount++; }
			}
			else
			{
				error("unknown type (Color, Float, Range(min, max), Int, Vector, 2D)");
				continue;
			}
			if (!ok)
			{
				error("invalid default value");
				continue;
			}
			outProperties.push_back(std::move(p));
		}
	}

	// 数と色は 1 つずつ 16 バイトに置く（float の後ろに float3 の詰め物を足す）。C++ 側も同じ並びで詰める
	std::string ShaderAsset::BuildPrelude(const std::vector<ShaderProperty>& properties, const std::string& path)
	{
		std::string out = "// ---- Properties (added by the engine) ----\n";
		bool anyConstant = std::any_of(properties.begin(), properties.end(), [](const ShaderProperty& p) { return p.type != ShaderProperty::Type::Texture; });
		if (anyConstant)
		{
			out += "cbuffer RDShaderProperties : register(b4)\n{\n";
			int pad = 0;
			for (const ShaderProperty& p : properties)
			{
				switch (p.type)
				{
				case ShaderProperty::Type::Color:
				case ShaderProperty::Type::Vector:
					out += std::format("    float4 {};\n", p.name);
					break;
				case ShaderProperty::Type::Int:
					out += std::format("    int {}; int3 _rdPad{};\n", p.name, pad++);
					break;
				case ShaderProperty::Type::Texture:
					break;
				default:
					out += std::format("    float {}; float3 _rdPad{};\n", p.name, pad++);
					break;
				}
			}
			out += "};\n";
		}
		for (const ShaderProperty& p : properties)
		{
			if (p.type == ShaderProperty::Type::Texture) { out += std::format("Texture2D<float4> {} : register(t{});\n", p.name, 5 + p.textureSlot); }
		}
		out += std::format("#line 1 \"{}\"\n", path);
		return out;
	}

	uint32_t ShaderAsset::GetConstantCount() const
	{
		return uint32_t(std::count_if(mProperties.begin(), mProperties.end(), [](const ShaderProperty& p) { return p.type != ShaderProperty::Type::Texture; }));
	}

	const ShaderProperty* ShaderAsset::FindProperty(const std::string& name) const
	{
		for (const ShaderProperty& p : mProperties)
		{
			if (p.name == name) { return &p; }
		}
		return nullptr;
	}

	void ShaderAsset::Load()
	{
		if (mId == 0) { mId = gNextShaderId++; }
		if (!ReadTextFile(mPath, mSource))
		{
			Log::Error(LogCat::Asset, "Failed to load shader: {}", mPath);
			SetState(AssetState::Failed);
			return;
		}
		ParseProperties(mSource, mProperties, mErrors);
		for (const auto& e : mErrors) { Log::Error(LogCat::Asset, "Shader properties ({}): {}", mPath, e); }
		mPrelude = BuildPrelude(mProperties, mPath);
		SetState(AssetState::Ready);
	}

	void ShaderAsset::Reload()
	{
		Load();
	}

	bool ShaderAsset::IsSurfaceShaderSource(const std::string& source)
	{
		// 標準の Mesh.hlsl も Surface.hlsli を使うが、Surface 関数を持たない（NO_SURFACE_FUNCTION）ので外す
		return source.find("Surface.hlsli") != std::string::npos && source.find("NO_SURFACE_FUNCTION") == std::string::npos;
	}

	bool ShaderAsset::IsSurfaceShaderPath(const std::string& path)
	{
		if (ToLower(GetExtension(path)) != ".hlsl") { return false; }
		std::string text;
		return ReadTextFile(path, text) && IsSurfaceShaderSource(text);
	}

	bool ShaderAsset::CreateFile(const std::string& path)
	{
		std::error_code ec;
		if (std::filesystem::exists(ToWide(path), ec)) { return false; }
		static const char* kTemplate = R"(// サーフェスシェーダー
//  色や質感だけを Surface 関数で決める。ライト・影・霧はエンジンが付ける。
//  下の Properties に書いた値は、マテリアルの画面で設定できる（変更したら Ctrl+R で読み直す）。
//  o にははじめからマテリアルの標準の値（Base Color やテクスチャ）が入っている。
/* Properties {
    _Color ("Color", Color) = (1, 1, 1, 1)
    _Strength ("Strength", Range(0, 1)) = 0.5
    _MainTex ("Texture", 2D) = "white" {}
} */
#include "Surface.hlsli"

void Surface(SurfaceInput i, inout SurfaceOutput o)
{
    float3 tex = _MainTex.Sample(gSamplerWrap, i.uv).rgb;
    o.albedo = lerp(o.albedo, o.albedo * tex * _Color.rgb, _Strength);
}
)";
		return WriteTextFile(path, kTemplate);
	}
}
