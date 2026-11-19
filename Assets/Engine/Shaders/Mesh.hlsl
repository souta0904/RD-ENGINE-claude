// メッシュ描画の標準のシェーダー（SKINNED を定義するとスキニング版）
//  中身はサーフェスシェーダーの土台（Surface.hlsli）そのもの。Surface 関数で書き換えず、マテリアルの値だけで描く
#define NO_SURFACE_FUNCTION
#include "Surface.hlsli"
