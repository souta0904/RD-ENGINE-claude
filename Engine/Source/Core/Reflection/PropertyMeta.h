#pragma once
#include <cstdint>

namespace rd
{
	// プロパティの付加情報（エディタでの表示に使う。min == max なら範囲なし）
	//  Unity の [Range(0, 1)] や [Tooltip("...")] のような属性にあたる。保存の内容には影響しない
	//
	//  例: v.Property("Speed", mSpeed, { .speed = 0.1f, .min = 0.0f, .max = 10.0f, .tooltip = "1 秒に進む距離" });
	//  （.speed = のように名前を付けて一部だけ書ける。書かなかった項目は下の既定値になる）
	struct RD_API PropertyMeta
	{
		// 表示の種類の印。1 << n で 1 つずつ別のビットに置き、| で組み合わせて使う（例: Angle | ReadOnly）
		enum Flags : uint32_t
		{
			None = 0,
			Angle = 1 << 0,        // 値はラジアンで持ち、画面には度で出す
			HDR = 1 << 1,          // 色を 1 より明るくできる
			ReadOnly = 1 << 2,     // 見るだけで変えられない
			AssetTexture = 1 << 3, // 文字列をアセットのパスとして扱い、その種類のファイルを選ぶ欄にする
			AssetModel = 1 << 4,
			AssetAudio = 1 << 5,
			AssetPrefab = 1 << 6,
			AssetScene = 1 << 7,
			Multiline = 1 << 8,    // 文字列を複数行で入力する
			AssetMaterial = 1 << 9,
			AssetShader = 1 << 10,
		};
		float speed = 0.05f;          // ドラッグしたときの値の変わる速さ
		float min = 0.0f;             // 値の範囲（min == max なら範囲なし）
		float max = 0.0f;
		uint32_t flags = None;
		const char* tooltip = nullptr; // マウスを乗せたときの説明
		// 画面に出す名前（空なら保存に使う名前をそのまま出す。シェーダーの Properties の表示名など）
		const char* label = nullptr;
	};
}
