#pragma once

namespace rd
{
	class Texture;

	// テクスチャを市松模様の上に、縦横比を保って幅いっぱいに描く（マウスを乗せると拡大鏡）
	RD_API void DrawTexturePreview(const Texture& texture, float maxHeight);
}
