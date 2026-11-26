// ==================================================
// 共通定義（C++ 側は Render/RenderTypes.h と一致させること）
// ==================================================
#ifndef RD_COMMON_HLSLI
#define RD_COMMON_HLSLI

static const float PI = 3.14159265f;
static const uint kMaxDirLights = 4;
static const uint kMaxPointLights = 32;
static const uint kMaxSpotLights = 16;
static const uint kMaxShadowCascades = 4;
static const uint kMaxObjectLights = 8;
static const uint kObjectLightSpot = 0x8000u;

// マテリアルのフラグ
static const uint kMatHasAlbedo = 1u << 0;
static const uint kMatHasNormal = 1u << 1;
static const uint kMatUnlit = 1u << 2;
static const uint kMatAlphaClip = 1u << 3;
static const uint kMatReceiveShadow = 1u << 4;
static const uint kMatHasEmissive = 1u << 5;
static const uint kMatFlat = 1u << 6;
static const uint kMatTriplanar = 1u << 7;

struct ObjectData
{
    float4x4 world;
    float4x4 worldInvTranspose;
    float4 tint;// エディタの選択表示などで乗算する色
    uint4 objectLights[2];// この物に効く追加のライトの番号（kObjectLightSpot が立っていればスポットライト）
    uint objectLightCount;
    uint3 objectPad;
};

struct FrameData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4x4 invViewProj;
    float4x4 shadowViewProj[kMaxShadowCascades];
    float4 cascadeSplits;// 各カスケードの奥の境界（ビュー空間の深さ）
    float4 cascadeTexelSizes;
    float3 cameraPos;
    float time;
    float3 cameraRight;
    float shadowBias;
    float3 cameraUp;
    float shadowStrength;
    float3 ambientSky;
    uint cascadeCount;
    float3 ambientGround;
    float fogDensity;
    float3 fogColor;
    float fogStart;
    float3 skyZenith;
    uint dirLightCount;
    float3 skyHorizon;
    uint pointLightCount;
    float2 screenSize;
    uint spotLightCount;
    uint shadowEnabled;
    uint shadowCascade;// シャドウパスで描いているカスケード
    uint debugCascades;// カスケードを色分けして表示
    float shadowFadeStart;// ここから先は影を薄くする
    float framePad;
    float4 ambientEquator;// w が 1 なら空・地平線・地面の 3 色の環境光
    uint additionalLightsPerVertex;// 1 なら点光源・スポットライトを頂点ごとに計算する
    uint lightsPerObject;// 0 より大きければ、物ごとに選んだライト（objectLights）だけを使う
    uint2 framePad2;
};

struct MaterialData
{
    float4 baseColor;
    float3 emissive;
    float metallic;
    float roughness;
    float alphaCutoff;
    uint flags;
    float triplanarTile;
    float3 shadowColor;
    float rimPower;
    float3 rimColor;
    float materialPad;
};

struct DirLight
{
    float3 direction;// 光が進む向き
    float intensity;
    float3 color;
    uint castShadow;
};

struct PointLight
{
    float3 position;
    float range;
    float3 color;
    float intensity;
};

struct SpotLight
{
    float3 position;
    float range;
    float3 direction;
    float intensity;
    float3 color;
    float cosInner;
    float cosOuter;
    float3 pad;
};

struct LightData
{
    DirLight dirLights[kMaxDirLights];
    PointLight pointLights[kMaxPointLights];
    SpotLight spotLights[kMaxSpotLights];
};

ConstantBuffer<ObjectData> gObject : register(b0);
ConstantBuffer<FrameData> gFrame : register(b1);
ConstantBuffer<MaterialData> gMaterial : register(b2);
ConstantBuffer<LightData> gLights : register(b3);

SamplerState gSamplerWrap : register(s0);
SamplerState gSamplerClamp : register(s1);
SamplerComparisonState gSamplerShadow : register(s2);

// ==================================================
// PBR（Cook-Torrance GGX）
// ==================================================
float D_GGX(float NoH, float a)
{
    float a2 = a * a;
    float f = (NoH * a2 - NoH) * NoH + 1.0f;
    return a2 / (PI * f * f + 1e-7f);
}

float V_SmithGGXCorrelated(float NoV, float NoL, float a)
{
    float a2 = a * a;
    float gv = NoL * sqrt(NoV * NoV * (1.0f - a2) + a2);
    float gl = NoV * sqrt(NoL * NoL * (1.0f - a2) + a2);
    return 0.5f / (gv + gl + 1e-7f);
}

float3 F_Schlick(float3 f0, float VoH)
{
    return f0 + (1.0f - f0) * pow(1.0f - VoH, 5.0f);
}

// 1 つの光源からの寄与（radiance はライトの色 x 強度 x 減衰）
float3 ShadeLight(float3 N, float3 V, float3 L, float3 radiance, float3 albedo, float metallic, float roughness)
{
    float3 H = normalize(V + L);
    float NoL = saturate(dot(N, L));
    float NoV = max(dot(N, V), 1e-4f);
    float NoH = saturate(dot(N, H));
    float VoH = saturate(dot(V, H));
    float a = roughness * roughness;

    float3 f0 = lerp(0.04f.xxx, albedo, metallic);
    float3 F = F_Schlick(f0, VoH);
    float3 spec = D_GGX(NoH, a) * V_SmithGGXCorrelated(NoV, NoL, a) * F;
    float3 diffuse = (1.0f - F) * (1.0f - metallic) * albedo / PI;
    return (diffuse + spec) * radiance * NoL;
}

// 環境光（半球ライト + 近似した環境 BRDF）
float3 EnvBRDFApprox(float3 f0, float roughness, float NoV)
{
    const float4 c0 = float4(-1, -0.0275, -0.572, 0.022);
    const float4 c1 = float4(1, 0.0425, 1.04, -0.04);
    float4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NoV)) * r.x + r.y;
    float2 ab = float2(-1.04, 1.04) * a004 + r.zw;
    return f0 * ab.x + ab.y;
}

// 環境光の色（2 色は地面から空へ、3 色は地平線を挟んで空と地面へ混ぜる）
//  3 色のときは、向いた半球の光を余弦で平均したように少しぼかす（真上を向いても地平線の色が 1/3 入る）
float3 Hemisphere(float3 dir)
{
    if (gFrame.ambientEquator.w < 0.5f)
    {
        return lerp(gFrame.ambientGround, gFrame.ambientSky, dir.y * 0.5f + 0.5f);
    }
    float y = dir.y * (2.0f / 3.0f);
    return y >= 0.0f ? lerp(gFrame.ambientEquator.xyz, gFrame.ambientSky, y) : lerp(gFrame.ambientEquator.xyz, gFrame.ambientGround, -y);
}

float3 ShadeAmbient(float3 N, float3 V, float3 albedo, float metallic, float roughness)
{
    float NoV = max(dot(N, V), 1e-4f);
    float3 f0 = lerp(0.04f.xxx, albedo, metallic);
    float3 diffuse = Hemisphere(N) * albedo * (1.0f - metallic);
    float3 R = reflect(-V, N);
    float3 spec = Hemisphere(R) * EnvBRDFApprox(f0, roughness, NoV);
    return diffuse + spec;
}

// ==================================================
// 追加のライト（点光源・スポットライト）
//  物ごとに選んだライトだけを使うときは objectLights の番号、全部使うときは点光源 → スポットライトの通し番号
// ==================================================
uint AdditionalLightCount()
{
    return gFrame.lightsPerObject > 0 ? gObject.objectLightCount : gFrame.pointLightCount + gFrame.spotLightCount;
}

// 点光源（spot なら スポットライト）の id 番の向き L と、届く光（色 x 強度 x 減衰）。届かなければ false
bool LightById(bool spot, uint id, float3 worldPos, out float3 L, out float3 radiance)
{
    L = 0.0f;
    radiance = 0.0f;
    // 窓関数付きの逆二乗減衰
    if (spot)
    {
        SpotLight light = gLights.spotLights[id];
        float3 toLight = light.position - worldPos;
        float dist = length(toLight);
        if (dist >= light.range) { return false; }
        L = toLight / dist;
        float falloff = saturate(1.0f - pow(dist / light.range, 4.0f));
        float att = falloff * falloff / (dist * dist + 1.0f);
        float cone = saturate((dot(-L, light.direction) - light.cosOuter) / max(light.cosInner - light.cosOuter, 1e-4f));
        radiance = light.color * light.intensity * att * cone * cone;
    }
    else
    {
        PointLight light = gLights.pointLights[id];
        float3 toLight = light.position - worldPos;
        float dist = length(toLight);
        if (dist >= light.range) { return false; }
        L = toLight / dist;
        float falloff = saturate(1.0f - pow(dist / light.range, 4.0f));
        float att = falloff * falloff / (dist * dist + 1.0f);
        radiance = light.color * light.intensity * att;
    }
    return true;
}

// k 番目の追加のライト（物ごとに選んだものがあればその中から）
bool AdditionalLight(uint k, float3 worldPos, out float3 L, out float3 radiance)
{
    if (gFrame.lightsPerObject > 0)
    {
        uint raw = gObject.objectLights[k / 4][k % 4];
        return LightById((raw & kObjectLightSpot) != 0, raw & ~kObjectLightSpot, worldPos, L, radiance);
    }
    bool spot = k >= gFrame.pointLightCount;
    return LightById(spot, spot ? k - gFrame.pointLightCount : k, worldPos, L, radiance);
}

// k 番目の追加のライト（物ごとの選び方を使わず、点光源 → スポットライトの通し番号。ディファード用）
bool AdditionalLightAll(uint k, float3 worldPos, out float3 L, out float3 radiance)
{
    bool spot = k >= gFrame.pointLightCount;
    return LightById(spot, spot ? k - gFrame.pointLightCount : k, worldPos, L, radiance);
}

// 頂点ごとに計算するときの追加のライト（拡散だけ。あとでアルベドを掛ける）
float3 VertexLighting(float3 worldPos, float3 N)
{
    float3 sum = 0.0f;
    uint count = AdditionalLightCount();
    for (uint k = 0; k < count; ++k)
    {
        float3 L, radiance;
        if (AdditionalLight(k, worldPos, L, radiance)) { sum += radiance * saturate(dot(N, L)) / PI; }
    }
    return sum;
}

// ==================================================
// 影（カスケードシャドウ + PCF 3x3）
//  シャドウマップは 2x2 のタイルに分かれていて、i 番目のカスケードは
//  (i % 2, i / 2) のタイルに入っている
// ==================================================
Texture2D<float> gShadowMap : register(t3);

// カメラからの深さで使うカスケードを選ぶ（範囲外なら cascadeCount）
uint SelectCascade(float viewDepth)
{
    uint cascade = 0;
    [unroll] for (uint i = 0; i < kMaxShadowCascades; ++i)
    {
        if (i < gFrame.cascadeCount && viewDepth > gFrame.cascadeSplits[i]) { cascade = i + 1; }
    }
    return cascade;
}

float SampleShadow(float3 worldPos, float3 N, float3 L)
{
    if (gFrame.shadowEnabled == 0) { return 1.0f; }
    float viewDepth = mul(float4(worldPos, 1.0f), gFrame.view).z;
    uint cascade = SelectCascade(viewDepth);
    if (cascade >= gFrame.cascadeCount) { return 1.0f; }

    // 法線方向に少しずらしてシャドウアクネを減らす（テクセルが大きいカスケードほど大きくずらす）
    float slope = 1.0f - saturate(dot(N, L));
    float3 p = worldPos + N * gFrame.cascadeTexelSizes[cascade] * (1.0f + 4.0f * slope);
    float4 sp = mul(float4(p, 1.0f), gFrame.shadowViewProj[cascade]);
    sp.xyz /= sp.w;
    float2 uv = sp.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f) || sp.z > 1.0f) { return 1.0f; }

    float w, h;
    gShadowMap.GetDimensions(w, h);
    float2 texel = 1.0f / float2(w, h);
    // タイルへ配置し、PCF が隣のタイルを読まないよう 1.5 テクセル内側に収める
    float2 tileMin = float2(cascade % 2, cascade / 2) * 0.5f;
    float2 tileUv = clamp(tileMin + uv * 0.5f, tileMin + texel * 1.5f, tileMin + 0.5f - texel * 1.5f);
    float depth = sp.z - gFrame.shadowBias;
    float sum = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, tileUv + float2(x, y) * texel, depth);
        }
    }
    float shadow = sum / 9.0f;
    // 影が届く距離の端では、急に切れないよう薄くしていく
    float lastSplit = gFrame.cascadeSplits[gFrame.cascadeCount - 1];
    float fade = saturate((viewDepth - gFrame.shadowFadeStart) / max(lastSplit - gFrame.shadowFadeStart, 1e-3f));
    return lerp(1.0f, shadow, gFrame.shadowStrength * (1.0f - fade));
}

// カスケードの色分け（Render Settings の Show Cascades）
float3 CascadeDebugColor(float3 worldPos)
{
    static const float3 kColors[5] = { float3(1, 0.4, 0.4), float3(0.4, 1, 0.4), float3(0.4, 0.6, 1), float3(1, 1, 0.4), float3(1, 1, 1) };
    float viewDepth = mul(float4(worldPos, 1.0f), gFrame.view).z;
    return kColors[min(SelectCascade(viewDepth), 4u)];
}

// ==================================================
// フォグ
// ==================================================
float3 ApplyFog(float3 color, float3 worldPos)
{
    if (gFrame.fogDensity <= 0.0f) { return color; }
    float dist = max(length(worldPos - gFrame.cameraPos) - gFrame.fogStart, 0.0f);
    float f = 1.0f - exp(-gFrame.fogDensity * dist);
    return lerp(color, gFrame.fogColor, saturate(f));
}

#endif
