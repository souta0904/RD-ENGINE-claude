#pragma once
#include "Asset/Asset.h"
#include "Math/Vector4.h"
#include <string>
#include <vector>

namespace rd
{
	// シェーダーが外に出す値 1 つ（Unity の Properties の 1 行）
	struct RD_API ShaderProperty
	{
		enum class Type { Color, Float, Range, Int, Vector, Texture };

		std::string name;        // シェーダーの中の変数の名前（_Color など）。マテリアルのファイルにもこの名前で保存する
		std::string displayName; // 画面に出す名前
		Type type = Type::Float;
		Vector4 defaultValue;    // 数と色の既定値（Float / Range / Int は x だけ使う）
		float min = 0.0f;        // Range の範囲
		float max = 1.0f;
		std::string defaultTexture = "white"; // white / black / gray / bump（平らな法線）
		bool hdr = false;        // [HDR]: 1 を超える明るさの色を選べる
		bool toggle = false;     // [Toggle]: Float をチェックボックスで 0 / 1 にする
		int textureSlot = -1;    // テクスチャなら t5 から数えた番号
	};

	// 自作のシェーダー（.hlsl）。Surface.hlsli を include して Surface 関数を書いたもの
	//
	//  ファイルの先頭の注釈に Unity と同じ書き方で値を並べると、マテリアルの画面で設定できるようになる:
	//
	//    /* Properties {
	//        _Color ("Color", Color) = (1, 0.5, 0.2, 1)
	//        [HDR] _Glow ("Glow", Color) = (0, 0, 0, 1)
	//        _Speed ("Speed", Range(0, 10)) = 2
	//        _Count ("Count", Int) = 8
	//        [Toggle] _Wave ("Wave", Float) = 1
	//        _Offset ("Offset", Vector) = (0, 1, 0, 0)
	//        _MainTex ("Texture", 2D) = "white" {}
	//    } */
	//
	//  エンジンはこれを読んで、シェーダーの前に変数の宣言を足してからコンパイルする
	//  （数と色は b4 の定数バッファ、テクスチャは t5 から順に。テクスチャは 4 枚まで）。
	//  そのためシェーダーの中では _Color や _MainTex をそのまま使える。
	//  中身は小さいので、マテリアルと同じく読み込みはその場で済ませる
	class RD_API ShaderAsset final : public Asset
	{
	public:
		static constexpr int kMaxTextures = 4;

		const char* GetTypeName() const override { return "Shader"; }

		const std::vector<ShaderProperty>& GetProperties() const { return mProperties; }
		const ShaderProperty* FindProperty(const std::string& name) const;
		// Properties の書き間違い（行番号つき）。空なら問題なし
		const std::vector<std::string>& GetErrors() const { return mErrors; }
		// シェーダーの前に足す宣言（最後に #line を置くので、エラーの行番号は元のファイルのまま）
		const std::string& GetPrelude() const { return mPrelude; }
		const std::string& GetSource() const { return mSource; }
		// パイプラインのキャッシュのキーに使う番号（アセットごとに違う）
		uint32_t GetId() const { return mId; }
		// 数と色を並べた定数バッファの大きさ（1 つ 16 バイト）
		uint32_t GetConstantCount() const;

		// ファイルを読み直す（Reload Shaders のとき）
		void Reload();

		// Properties の注釈を読む（テストからも使う）
		static void ParseProperties(const std::string& source, std::vector<ShaderProperty>& outProperties, std::vector<std::string>& outErrors);
		static std::string BuildPrelude(const std::vector<ShaderProperty>& properties, const std::string& path);
		// .hlsl で、Surface.hlsli を使っているもの（マテリアルに入れられるシェーダー）か
		static bool IsSurfaceShaderPath(const std::string& path);
		static bool IsSurfaceShaderSource(const std::string& source);
		// 新しいサーフェスシェーダーの雛形を書く（もうあれば false）
		static bool CreateFile(const std::string& path);

	protected:
		void Load() override;

	private:
		std::vector<ShaderProperty> mProperties;
		std::vector<std::string> mErrors;
		std::string mPrelude;
		std::string mSource;
		uint32_t mId = 0;
	};
}
