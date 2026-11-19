// ==================================================
// サーフェスシェーダーの土台（Unity のサーフェスシェーダーと同じ考え方）
//
//  自分のシェーダーは、このファイルを include して Surface 関数を 1 つ書くだけでよい。
//  頂点の変換・ライト・影・霧はエンジンが行い、Surface は「その点の色や質感」だけを決める。
//
//    #include "Surface.hlsli"
//    void Surface(SurfaceInput i, inout SurfaceOutput o)
//    {
//        o.albedo *= _Color.rgb;          // o にはマテリアルの標準の値が入っている
//    }
//
//  頂点を動かしたいときは、include の前に SURFACE_VERTEX を定義して SurfaceVertex を書く
//  （影は元の形のまま落ちる）。
//
//    #define SURFACE_VERTEX
//    #include "Surface.hlsli"
//    void SurfaceVertex(inout SurfaceVertexData v) { v.position.y += sin(v.position.x + Time()); }
//
//  マテリアルで設定する値は、ファイルの先頭の Properties の注釈に書く（エンジンが読んで変数を作る）。
//
//  Mesh.hlsl（標準のシェーダー）も NO_SURFACE_FUNCTION を定義してこのファイルを使う
// ==================================================
#ifndef RD_SURFACE_HLSLI
#define RD_SURFACE_HLSLI
#include "Common.hlsli"

// ---------------------------------------------------------------- Surface 関数に渡すもの

// 頂点を動かす関数に渡す値（モデルの空間。スキンメッシュはスキニングの後）
struct SurfaceVertexData
{
    float3 position;
    float3 normal;
    float2 uv;
};

// 点の情報
struct SurfaceInput
{
    float3 worldPos;   // ワールド座標
    float3 normal;     // ワールドの法線（法線マップをかける前。裏面ならカメラ側へ向けたもの）
    float3 viewDir;    // 点からカメラへの向き（長さ 1）
    float2 uv;
    float4 screenPos;  // xy: 画面のピクセル位置、z: 深度
    bool frontFace;
};

// Surface 関数が書き換える値（はじめはマテリアルの標準の値が入っている）
struct SurfaceOutput
{
    float3 albedo;     // 色（アルベドマップと Base Color、ティントを掛けたもの）
    float alpha;       // 不透明度（Alpha Clip と半透明に使う）
    float3 normal;     // ワールドの法線（法線マップをかけたもの）
    float3 emission;   // 発光（ライトに関係なく足される）
    float metallic;
    float roughness;
};

// 経過時間（秒）。アニメーションに使う
float Time() { return gFrame.time; }

// ---------------------------------------------------------------- 頂点シェーダー

struct VSInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
#ifdef SKINNED
    uint4 joints : BLENDINDICES;
    float4 weights : BLENDWEIGHT;
#endif
};

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float3 worldPos : POSITION;
    float3 normal : NORMAL;
    float4 tangent : TANGENT;
    float2 uv : TEXCOORD0;
    float3 vertexLight : TEXCOORD1;// 頂点ごとに計算した追加のライト（拡散）
};

#ifdef SKINNED
StructuredBuffer<float4x4> gPalette : register(t2);
#endif

#ifdef SURFACE_VERTEX
void SurfaceVertex(inout SurfaceVertexData v);
#endif

VSOutput VSMain(VSInput input)
{
    float4 pos = float4(input.position, 1.0f);
    float3 nrm = input.normal;
    float3 tan = input.tangent.xyz;
#ifdef SKINNED
    float4x4 skin =
        gPalette[input.joints.x] * input.weights.x +
        gPalette[input.joints.y] * input.weights.y +
        gPalette[input.joints.z] * input.weights.z +
        gPalette[input.joints.w] * input.weights.w;
    pos = mul(pos, skin);
    nrm = mul(float4(nrm, 0.0f), skin).xyz;
    tan = mul(float4(tan, 0.0f), skin).xyz;
#endif
    float2 uv = input.uv;
#ifdef SURFACE_VERTEX
    SurfaceVertexData v;
    v.position = pos.xyz;
    v.normal = nrm;
    v.uv = uv;
    SurfaceVertex(v);
    pos = float4(v.position, 1.0f);
    nrm = v.normal;
    uv = v.uv;
#endif
    VSOutput o;
    float4 wpos = mul(pos, gObject.world);
    o.worldPos = wpos.xyz;
    o.svpos = mul(wpos, gFrame.viewProj);
    o.normal = normalize(mul(float4(nrm, 0.0f), gObject.worldInvTranspose).xyz);
    o.tangent = float4(normalize(mul(float4(tan, 0.0f), gObject.world).xyz), input.tangent.w);
    o.uv = uv;
    o.vertexLight = gFrame.additionalLightsPerVertex != 0 ? VertexLighting(o.worldPos, o.normal) : 0.0f;
    return o;
}

// ---------------------------------------------------------------- ピクセルシェーダー

Texture2D<float4> gAlbedo : register(t0);
Texture2D<float4> gNormalMap : register(t1);
Texture2D<float4> gEmissiveMap : register(t4);

// ワールド座標で 3 方向から貼り、法線の向きで混ぜる（Unity の Triplanar ノードと同じ。混ぜ方の鋭さは 1）
float4 SampleTriplanar(Texture2D<float4> tex, float3 worldPos, float3 normal, float tile)
{
    float3 p = worldPos * tile;
    float3 w = abs(normal);
    w /= max(w.x + w.y + w.z, 1e-5f);
    return tex.Sample(gSamplerWrap, p.zy) * w.x + tex.Sample(gSamplerWrap, p.xz) * w.y + tex.Sample(gSamplerWrap, p.xy) * w.z;
}

// Flat: 面ごとの平らな法線で、先頭の平行光源に向いているほど白く、背くほど影の色にする。縁はリムの色で光らせる
//  ライトの色や環境光・影は使わない（Unity の mouse/Flat と同じ）
float3 ShadeFlat(float3 base, float3 worldPos)
{
    float3 N = normalize(cross(ddy(worldPos), ddx(worldPos)));
    float3 V = normalize(gFrame.cameraPos - worldPos);
    if (dot(N, V) < 0.0f) { N = -N; }// 三角形の巻き方によらずカメラ側を向ける
    float ndl = gFrame.dirLightCount > 0 ? saturate(dot(N, -gLights.dirLights[0].direction)) : 1.0f;
    float3 color = base * lerp(gMaterial.shadowColor, 1.0f.xxx, ndl);
    float rim = pow(1.0f - saturate(dot(N, V)), gMaterial.rimPower);
    return color + gMaterial.rimColor * rim;
}

#ifndef NO_SURFACE_FUNCTION
void Surface(SurfaceInput i, inout SurfaceOutput o);
#endif

// マテリアルの標準の値で SurfaceOutput を埋め、Surface 関数で書き換えてから光を当てる
float4 PSMain(VSOutput input, bool frontFace : SV_IsFrontFace) : SV_TARGET
{
    float3 emissive = gMaterial.emissive;
    if (gMaterial.flags & kMatHasEmissive)
    {
        emissive *= gEmissiveMap.Sample(gSamplerWrap, input.uv).rgb;
    }
    float4 base = gMaterial.baseColor;
    if (gMaterial.flags & kMatHasAlbedo)
    {
        base *= (gMaterial.flags & kMatTriplanar)
            ? SampleTriplanar(gAlbedo, input.worldPos, normalize(input.normal), gMaterial.triplanarTile)
            : gAlbedo.Sample(gSamplerWrap, input.uv);
    }
    base *= gObject.tint;

    float3 N = normalize(input.normal);
    if (!frontFace) { N = -N; }// 両面描画の裏面
    float3 geometricN = N;
    if (gMaterial.flags & kMatHasNormal)
    {
        float3 T = normalize(input.tangent.xyz - N * dot(input.tangent.xyz, N));
        float3 B = cross(N, T) * input.tangent.w;
        float3 tn = gNormalMap.Sample(gSamplerWrap, input.uv).xyz * 2.0f - 1.0f;
        N = normalize(tn.x * T + tn.y * B + tn.z * N);
    }

    SurfaceOutput o;
    o.albedo = base.rgb;
    o.alpha = base.a;
    o.normal = N;
    o.emission = emissive;
    o.metallic = gMaterial.metallic;
    o.roughness = gMaterial.roughness;
#ifndef NO_SURFACE_FUNCTION
    SurfaceInput si;
    si.worldPos = input.worldPos;
    si.normal = geometricN;
    si.viewDir = normalize(gFrame.cameraPos - input.worldPos);
    si.uv = input.uv;
    si.screenPos = input.svpos;
    si.frontFace = frontFace;
    Surface(si, o);
#endif

    if ((gMaterial.flags & kMatAlphaClip) && o.alpha < gMaterial.alphaCutoff)
    {
        discard;
    }
    if (gMaterial.flags & kMatUnlit)
    {
        return float4(ApplyFog(o.albedo + o.emission, input.worldPos), o.alpha);
    }
    if (gMaterial.flags & kMatFlat)
    {
        return float4(ApplyFog(ShadeFlat(o.albedo, input.worldPos) + o.emission, input.worldPos), o.alpha);
    }

    N = normalize(o.normal);
    float3 V = normalize(gFrame.cameraPos - input.worldPos);
    float metallic = saturate(o.metallic);
    float roughness = clamp(o.roughness, 0.04f, 1.0f);

    float3 color = ShadeAmbient(N, V, o.albedo, metallic, roughness);

    // 平行光源
    for (uint li = 0; li < gFrame.dirLightCount; ++li)
    {
        DirLight light = gLights.dirLights[li];
        float3 L = -light.direction;
        float shadow = 1.0f;
        if (light.castShadow && (gMaterial.flags & kMatReceiveShadow))
        {
            shadow = SampleShadow(input.worldPos, N, L);
        }
        color += ShadeLight(N, V, L, light.color * light.intensity * shadow, o.albedo, metallic, roughness);
    }
    // 追加のライト（点光源・スポットライト）。頂点ごとなら頂点で求めた拡散にアルベドを掛けるだけ
    if (gFrame.additionalLightsPerVertex != 0)
    {
        color += input.vertexLight * o.albedo * (1.0f - metallic);
    }
    else
    {
        uint count = AdditionalLightCount();
        for (uint k = 0; k < count; ++k)
        {
            float3 L, radiance;
            if (AdditionalLight(k, input.worldPos, L, radiance))
            {
                color += ShadeLight(N, V, L, radiance, o.albedo, metallic, roughness);
            }
        }
    }

    color += o.emission;
    if (gFrame.debugCascades != 0 && gFrame.shadowEnabled != 0) { color *= CascadeDebugColor(input.worldPos); }
    color = ApplyFog(color, input.worldPos);
    return float4(color, o.alpha);
}

#endif
