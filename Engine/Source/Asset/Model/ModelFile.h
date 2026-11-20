#pragma once
#include "Asset/Model/ModelData.h"
#include <string>

// エンジン独自のモデルファイル .rdmodel の読み書き
//  描画とアニメーションに使うものだけを入れたバイナリ。fbx などと違って解析がいらないので読み込みも速い。
//
//  並び順（数値はすべてリトルエンディアン。文字列は「長さ u32 + UTF-8」、配列は「個数 u32 + 中身」）
//    ヘッダ        "RDMD"、版 u32、フラグ u32（1: スキニングあり、2: スケルトンあり）
//    情報ブロック  全体の AABB、マテリアル（テクスチャは .rdmodel からの相対パス）、サブメッシュ、スケルトン、クリップ
//    形状ブロック  頂点数・インデックス数、頂点、スキン、インデックス
//  ブロックは「元の大きさ u32 + 保存した大きさ u32 + 中身」で、中身は XPRESS で圧縮する（縮まなければそのまま）。
//  アニメーションだけ読むときは形状ブロックを読まない。
//
//  小さくするための工夫
//    法線・接線は 16 ビット 2 個（八面体の符号化）、ウェイトは 16 ビット、関節の番号は 16 ビット
//    インデックスは前との差、アニメーションは前後と同じ値のキーを省く。位置と UV は元の精度のまま
namespace rd::ModelFile
{
	inline constexpr const char* kExtension = ".rdmodel";

	// data のマテリアルのテクスチャのパスは、書き出す先からの相対パスにしておくこと（"#linear" は付けない）
	//  途中で失敗しても元のファイルを壊さないよう、一時ファイルに書いてから置き換える
	RD_API bool Write(const std::string& path, const ModelData& data, std::string& error);

	// テクスチャのパスは読み込める形（.rdmodel の場所を基準にしたパス、法線マップは "#linear" 付き）にして返す
	RD_API bool Read(const std::string& path, bool animOnly, ModelData& out, std::string& error);
}
