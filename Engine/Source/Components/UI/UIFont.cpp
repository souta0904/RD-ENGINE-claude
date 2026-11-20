#include "RdPch.h"
#include "Components/UI/UIFont.h"
#include "Asset/Texture/Texture.h"
#include "Core/Util/StringUtil.h"
#include <fstream>

// ImGui に入っている stb_truetype をこのファイルの中だけで使う（ImGui 側とぶつからないよう static にする）
#define STBTT_STATIC
#define STB_TRUETYPE_IMPLEMENTATION
#pragma warning(push)
#pragma warning(disable : 4244 4245 4456 4457 4701 4702 4703 6011 6262 6385 6386)
#include "ImGui/imstb_truetype.h"
#pragma warning(pop)

namespace rd
{
	namespace
	{
		constexpr int kPadding = 2;

		std::unordered_map<std::string, std::unique_ptr<UIFont>>& Fonts()
		{
			static std::unordered_map<std::string, std::unique_ptr<UIFont>> fonts;
			return fonts;
		}
	}

	UIFont* UIFont::Get(const std::string& path)
	{
		const std::string& key = path.empty() ? sDefaultPath : path;
		auto& fonts = Fonts();
		auto it = fonts.find(key);
		if (it == fonts.end()) { it = fonts.emplace(key, std::unique_ptr<UIFont>(new UIFont(key))).first; }
		return it->second.get();
	}

	void UIFont::ReleaseAll()
	{
		Fonts().clear();
	}

	UIFont::UIFont(const std::string& path)
	{
		std::ifstream file(ToWide(path), std::ios::binary);
		if (!file)
		{
			Log::Error(LogCat::Asset, "Failed to open font: {}", path);
			return;
		}
		mFile.assign(std::istreambuf_iterator<char>(file), {});
		auto info = std::make_shared<stbtt_fontinfo>();
		int offset = stbtt_GetFontOffsetForIndex(mFile.data(), 0);
		if (offset < 0 || !stbtt_InitFont(info.get(), mFile.data(), offset))
		{
			Log::Error(LogCat::Asset, "Failed to read font: {}", path);
			return;
		}
		mInfo = info;
		mScale = stbtt_ScaleForPixelHeight(info.get(), kBakeSize);
		int ascent, descent, lineGap;
		stbtt_GetFontVMetrics(info.get(), &ascent, &descent, &lineGap);
		// ScaleForPixelHeight は ascent - descent を kBakeSize に合わせる
		mAscent = float(ascent) * mScale;
		mDescent = float(descent) * mScale;
		mLineGap = float(lineGap) * mScale;
		mEm = mScale / stbtt_ScaleForMappingEmToPixels(info.get(), 1.0f);
		mPixels.assign(size_t(kAtlasSize) * kAtlasSize * 4, 0);
		for (size_t i = 0; i < mPixels.size(); i += 4) { mPixels[i] = mPixels[i + 1] = mPixels[i + 2] = 255; }
		mValid = true;
	}

	const UIFont::Glyph& UIFont::GetGlyph(char32_t c)
	{
		auto it = mGlyphs.find(c);
		if (it != mGlyphs.end()) { return it->second; }
		Glyph g;
		if (mValid && !Bake(c, g))
		{
			// アトラスがいっぱいになったら描き直す（今あるものは次から描き足される）
			mGlyphs.clear();
			for (size_t i = 3; i < mPixels.size(); i += 4) { mPixels[i] = 0; }
			mPenX = mPenY = 1;
			mRowHeight = 0;
			Bake(c, g);
		}
		return mGlyphs.emplace(c, g).first->second;
	}

	bool UIFont::Bake(char32_t c, Glyph& g)
	{
		auto* info = static_cast<stbtt_fontinfo*>(mInfo.get());
		int index = stbtt_FindGlyphIndex(info, int(c));
		int advance, lsb;
		stbtt_GetGlyphHMetrics(info, index, &advance, &lsb);
		g.advance = float(advance) * mScale;
		int x0, y0, x1, y1;
		stbtt_GetGlyphBitmapBox(info, index, mScale, mScale, &x0, &y0, &x1, &y1);
		int w = x1 - x0, h = y1 - y0;
		if (w <= 0 || h <= 0) { g.empty = true; return true; }

		if (mPenX + w + kPadding > kAtlasSize)
		{
			mPenX = 1;
			mPenY += mRowHeight + kPadding;
			mRowHeight = 0;
		}
		if (mPenY + h + kPadding > kAtlasSize) { return false; }

		std::vector<uint8_t> bitmap(size_t(w) * h);
		stbtt_MakeGlyphBitmap(info, bitmap.data(), w, h, w, mScale, mScale, index);
		for (int y = 0; y < h; ++y)
		{
			for (int x = 0; x < w; ++x)
			{
				mPixels[(size_t(mPenY + y) * kAtlasSize + (mPenX + x)) * 4 + 3] = bitmap[size_t(y) * w + x];
			}
		}
		float inv = 1.0f / float(kAtlasSize);
		g.uv = Vector4(float(mPenX) * inv, float(mPenY) * inv, float(mPenX + w) * inv, float(mPenY + h) * inv);
		g.offset = Vector2(float(x0), float(y0));
		g.size = Vector2(float(w), float(h));
		g.empty = false;
		mPenX += w + kPadding;
		mRowHeight = std::max(mRowHeight, h);
		mDirty = true;
		return true;
	}

	const Texture* UIFont::GetTexture()
	{
		if (mPending && mPending->IsReady())
		{
			mTexture = std::move(mPending);
			mPending.reset();
		}
		if (mDirty && !mPending)
		{
			mPending = std::make_shared<Texture>();
			mPending->LoadFromPixels(mPixels.data(), kAtlasSize, kAtlasSize, false);
			mDirty = false;
			if (!mTexture) { return nullptr; }
		}
		return mTexture && mTexture->IsReady() ? mTexture.get() : nullptr;
	}
}
