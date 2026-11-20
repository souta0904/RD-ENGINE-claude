#pragma once
#include "Asset/Asset.h"
#include "Math/Math.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace rd
{
	class Texture;

	// 画面の文字を描くためのフォント（TrueType / OpenType の .ttf .otf .ttc）
	//  文字は使われたときに 1 枚の大きな画像（アトラス）へ描き足し、変わったらフレームの初めに GPU へ送り直す。
	//  大きさは kBakeSize ピクセルで描いておき、表示するときに拡大・縮小する
	class RD_API UIFont
	{
	public:
		static constexpr float kBakeSize = 48.0f;

		struct Glyph
		{
			Vector4 uv;          // アトラスの中の範囲（左上・右下）
			Vector2 offset;      // ペンの位置（ベースライン）から見た左上（kBakeSize のとき、下向きが +）
			Vector2 size;        // 画像の大きさ（kBakeSize のとき）
			float advance = 0.0f;// 次の文字までの幅（kBakeSize のとき）
			bool empty = true;   // 空白など、描く画像がない
		};

		// 同じパスは同じフォントを返す。空なら既定のフォント
		static UIFont* Get(const std::string& path);
		// 文字にパスを指定しないときに使うフォント（ゲームの最初に設定する）
		static void SetDefaultPath(const std::string& path) { sDefaultPath = path; }
		static const std::string& GetDefaultPath() { return sDefaultPath; }
		// 読んだフォントを全部手放す（エンジンの終了時、GPU を片付ける前に呼ぶ。
		//  エンジンは DLL なので、static のまま終了を待つと D3D12 より後に消えて、解放漏れと報告される）
		static void ReleaseAll();

		bool IsValid() const { return mValid; }
		const Glyph& GetGlyph(char32_t c);
		// kBakeSize のときの、ベースラインから上・下の高さと行の間隔
		float GetAscent() const { return mAscent; }
		float GetDescent() const { return mDescent; }
		float GetLineHeight() const { return mAscent - mDescent + mLineGap; }
		// kBakeSize で描いたときの 1em の大きさ（文字の大きさの指定は em なので、表示の倍率は 指定 / これ）
		float GetBakeEm() const { return mEm; }
		// 描き足した文字があれば送り直す。送っている間は前の画像を返す（足したばかりの文字はまだ出ない）
		const Texture* GetTexture();

	private:
		explicit UIFont(const std::string& path);
		bool Bake(char32_t c, Glyph& glyph);

		static inline std::string sDefaultPath = "C:/Windows/Fonts/meiryo.ttc";

		bool mValid = false;
		std::vector<uint8_t> mFile;
		std::shared_ptr<void> mInfo; // stbtt_fontinfo
		float mScale = 1.0f;
		float mAscent = 0.0f, mDescent = 0.0f, mLineGap = 0.0f, mEm = kBakeSize;

		static constexpr int kAtlasSize = 2048;
		std::vector<uint8_t> mPixels; // RGBA（RGB は白、A が文字の形）
		int mPenX = 1, mPenY = 1, mRowHeight = 0;
		bool mDirty = false;
		std::unordered_map<char32_t, Glyph> mGlyphs;
		AssetRef<Texture> mTexture;
		AssetRef<Texture> mPending;
	};
}
