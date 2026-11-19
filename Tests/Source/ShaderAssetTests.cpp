#include "TestFramework.h"
#include "Core/Serialization/JsonReader.h"
#include "Core/Serialization/JsonWriter.h"
#include "Render/Material/Material.h"
#include "Render/Material/ShaderAsset.h"

using namespace rd;

// シェーダーの Properties の読み取りと、シェーダーの前に足す宣言を確かめる
//  （コンパイルと描画は GPU が要るので、エディタで確かめる）

namespace
{
	const char* kSource = R"(// sample
/* Properties {
    _Color ("Main Color", Color) = (1, 0.5, 0.25, 1)
    [HDR] _Glow ("Glow", Color) = (2, 2, 2)
    _Speed ("Speed", Range(0, 10)) = 20
    _Count ("Count", Int) = 3.6
    [Toggle] _On ("On", Float) = 1
    _Dir ("Direction", Vector) = (0, 1)
    // comment lines are skipped

    _MainTex ("Texture", 2D) = "white" {}
    _Bump ("Normal", 2D) = "bump" {}
} */
#include "Surface.hlsli"
void Surface(SurfaceInput i, inout SurfaceOutput o) {}
)";
}

RD_TEST(ShaderAsset, ParsesAllTypes)
{
	std::vector<ShaderProperty> props;
	std::vector<std::string> errors;
	ShaderAsset::ParseProperties(kSource, props, errors);
	RD_CHECK(errors.empty());
	RD_REQUIRE(props.size() == 8);

	RD_CHECK_EQ(props[0].name, std::string("_Color"));
	RD_CHECK_EQ(props[0].displayName, std::string("Main Color"));
	RD_CHECK(props[0].type == ShaderProperty::Type::Color);
	RD_CHECK_NEAR(props[0].defaultValue.z, 0.25f, 1.0e-5f);

	// 色の a を省くと 1
	RD_CHECK(props[1].hdr);
	RD_CHECK_NEAR(props[1].defaultValue.w, 1.0f, 1.0e-5f);

	// Range の既定値は範囲に収める
	RD_CHECK(props[2].type == ShaderProperty::Type::Range);
	RD_CHECK_NEAR(props[2].max, 10.0f, 1.0e-5f);
	RD_CHECK_NEAR(props[2].defaultValue.x, 10.0f, 1.0e-5f);

	RD_CHECK(props[3].type == ShaderProperty::Type::Int);
	RD_CHECK_NEAR(props[3].defaultValue.x, 4.0f, 1.0e-5f);

	RD_CHECK(props[4].toggle);
	// ベクトルの足りない成分は 0
	RD_CHECK(props[5].type == ShaderProperty::Type::Vector);
	RD_CHECK_NEAR(props[5].defaultValue.y, 1.0f, 1.0e-5f);
	RD_CHECK_NEAR(props[5].defaultValue.w, 0.0f, 1.0e-5f);

	RD_CHECK(props[6].type == ShaderProperty::Type::Texture);
	RD_CHECK_EQ(props[6].textureSlot, 0);
	RD_CHECK_EQ(props[7].defaultTexture, std::string("bump"));
	RD_CHECK_EQ(props[7].textureSlot, 1);
}

// 書き間違いは行番号つきで報告し、その行だけ飛ばす
RD_TEST(ShaderAsset, ReportsErrorsWithLineNumbers)
{
	const char* source = "\n/* Properties {\n    _A (\"A\", Color) = (1, 1, 1, 1)\n    _B (\"B\", Bogus) = 1\n    _A (\"Again\", Float) = 0\n    _C (\"C\", Range(1, 0)) = 0\n    broken line\n} */\n";
	std::vector<ShaderProperty> props;
	std::vector<std::string> errors;
	ShaderAsset::ParseProperties(source, props, errors);
	RD_CHECK_EQ(props.size(), size_t(1));
	RD_REQUIRE(errors.size() == 4);
	RD_CHECK(errors[0].starts_with("line 4:"));
	RD_CHECK(errors[1].find("duplicate") != std::string::npos);
	RD_CHECK(errors[2].find("Range") != std::string::npos);
	RD_CHECK(errors[3].starts_with("line 7:"));
}

// テクスチャは 4 枚まで
RD_TEST(ShaderAsset, LimitsTextureCount)
{
	std::string source = "/* Properties {\n";
	for (int i = 0; i < 5; ++i) { source += std::format("    _T{} (\"T\", 2D) = \"white\" {{}}\n", i); }
	source += "} */\n";
	std::vector<ShaderProperty> props;
	std::vector<std::string> errors;
	ShaderAsset::ParseProperties(source, props, errors);
	RD_CHECK_EQ(props.size(), size_t(ShaderAsset::kMaxTextures));
	RD_CHECK_EQ(errors.size(), size_t(1));
}

// 宣言は 1 つ 16 バイトに並び、テクスチャは t5 から。最後に #line で元の行番号へ戻す
RD_TEST(ShaderAsset, PreludeLayout)
{
	std::vector<ShaderProperty> props;
	std::vector<std::string> errors;
	ShaderAsset::ParseProperties(kSource, props, errors);
	std::string prelude = ShaderAsset::BuildPrelude(props, "Assets/X.hlsl");
	RD_CHECK(prelude.find("register(b4)") != std::string::npos);
	RD_CHECK(prelude.find("float4 _Color;") != std::string::npos);
	RD_CHECK(prelude.find("float _Speed; float3 _rdPad") != std::string::npos);
	RD_CHECK(prelude.find("int _Count; int3 _rdPad") != std::string::npos);
	RD_CHECK(prelude.find("Texture2D<float4> _MainTex : register(t5);") != std::string::npos);
	RD_CHECK(prelude.find("Texture2D<float4> _Bump : register(t6);") != std::string::npos);
	RD_CHECK(prelude.ends_with("#line 1 \"Assets/X.hlsl\"\n"));

	// 数と色がなければ定数バッファは作らない（空の cbuffer はコンパイルできない）
	std::vector<ShaderProperty> texOnly(1);
	texOnly[0].name = "_T";
	texOnly[0].type = ShaderProperty::Type::Texture;
	texOnly[0].textureSlot = 0;
	RD_CHECK(ShaderAsset::BuildPrelude(texOnly, "a").find("cbuffer") == std::string::npos);

	// Properties がなくても使える
	ShaderAsset::ParseProperties("#include \"Surface.hlsli\"\n", props, errors);
	RD_CHECK(props.empty());
	RD_CHECK(errors.empty());
}

RD_TEST(ShaderAsset, SurfaceShaderDetection)
{
	RD_CHECK(ShaderAsset::IsSurfaceShaderSource(kSource));
	// 標準の Mesh.hlsl は Surface 関数を持たないので、マテリアルに入れるシェーダーの候補にしない
	RD_CHECK(!ShaderAsset::IsSurfaceShaderSource("#define NO_SURFACE_FUNCTION\n#include \"Surface.hlsli\"\n"));
	RD_CHECK(!ShaderAsset::IsSurfaceShaderSource("float4 main() : SV_TARGET { return 1; }"));
}

// シェーダーを入れていないマテリアルは、前と同じ値で保存・読み込みできる（"Shader" は空で書く）
RD_TEST(ShaderAsset, MaterialWithoutShaderRoundTrip)
{
	Material m;
	m.baseColor = Color::kGreen;
	Json json = Json::object();
	JsonWriter writer(json);
	m.Reflect(writer);
	RD_CHECK(json.contains("Shader"));
	RD_CHECK(!json.contains("Shader Properties"));

	Material loaded;
	JsonReader reader(json);
	loaded.Reflect(reader);
	RD_CHECK(loaded.shaderPath.empty());
	RD_CHECK(loaded.GetCustomShader() == nullptr);
	RD_CHECK_NEAR(loaded.baseColor.g, 1.0f, 1.0e-5f);
	std::vector<Vector4> constants;
	loaded.BuildShaderConstants(constants);
	RD_CHECK(constants.empty());
}
