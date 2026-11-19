// 縞模様のサンプル（サーフェスシェーダーの書き方の見本）
//  動く縞・縁の光・頂点の波を、マテリアルの画面の値で調整できる。
//  下の Properties に書いた値が、マテリアルの「Shader Properties」に並ぶ（変数はエンジンが宣言する）。
//  書き換えたら、エディタで Ctrl+R（Reload Shaders）を押すと読み直す。
/* Properties {
    _StripeColor ("Stripe Color", Color) = (1, 0.45, 0.1, 1)
    [HDR] _RimColor ("Rim Glow", Color) = (0.3, 0.8, 2.0, 1)
    _Stripes ("Stripe Count", Int) = 8
    _Width ("Stripe Width", Range(0, 1)) = 0.5
    _Speed ("Scroll Speed", Float) = 0.5
    _Direction ("Direction", Vector) = (0, 1, 0, 0)
    [Toggle] _Wave ("Wave Vertices", Float) = 0
    _WaveHeight ("Wave Height", Range(0, 0.5)) = 0.08
    _Mask ("Stripe Mask", 2D) = "white" {}
} */

// 頂点も動かすので、include の前に SURFACE_VERTEX を定義する
#define SURFACE_VERTEX
#include "Surface.hlsli"

// 頂点を法線の向きに波打たせる（Wave Vertices がオンのときだけ）。影は元の形のまま
void SurfaceVertex(inout SurfaceVertexData v)
{
    if (_Wave > 0.5f)
    {
        v.position += v.normal * sin(v.position.y * 6.0f + Time() * 3.0f) * _WaveHeight;
    }
}

// o にはマテリアルの標準の値（Base Color やテクスチャ）が入っているので、その上に縞と縁の光を足す
void Surface(SurfaceInput i, inout SurfaceOutput o)
{
    float3 dir = normalize(_Direction.xyz + 1e-5f);
    float t = dot(i.worldPos, dir) * float(_Stripes) * 0.5f + Time() * _Speed;
    float stripe = step(frac(t), _Width);
    float mask = _Mask.Sample(gSamplerWrap, i.uv).r;
    o.albedo = lerp(o.albedo, _StripeColor.rgb, stripe * mask);

    float rim = pow(1.0f - saturate(dot(i.normal, i.viewDir)), 3.0f);
    o.emission += _RimColor.rgb * rim;
}
