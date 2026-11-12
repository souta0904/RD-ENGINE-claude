// ==================================================
// 共通定義（C++ 側は Render/RenderTypes.h と一致させること）
// ==================================================
#ifndef RD_COMMON_HLSLI
#define RD_COMMON_HLSLI

static const float PI = 3.14159265f;
static const uint kMaxDirLights = 4;
static const uint kMaxPointLights = 32;
static const uint kMaxSpotLights = 16;

// マテリアルのフラグ
static const uint kMatHasAlbedo = 1u << 0;
static const uint kMatHasNormal = 1u << 1;
static const uint kMatUnlit = 1u << 2;
static const uint kMatAlphaClip = 1u << 3;
static const uint kMatReceiveShadow = 1u << 4;

struct ObjectData
{
    float4x4 world;
    float4x4 worldInvTranspose;
    float4 tint;// エディタの選択表示などで乗算する色
};

struct FrameData
{
    float4x4 view;
    float4x4 proj;
    float4x4 viewProj;
    float4x4 invViewProj;
    float4x4 shadowViewProj;
    float3 cameraPos;
    float time;
    float3 cameraRight;
    float shadowBias;
    float3 cameraUp;
    float shadowStrength;
    float3 ambientSky;
    float shadowTexelSize;
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
};

struct MaterialData
{
    float4 baseColor;
    float3 emissive;
    float metallic;
    float roughness;
    float alphaCutoff;
    uint flags;
    float pad;
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

float3 Hemisphere(float3 dir)
{
    return lerp(gFrame.ambientGround, gFrame.ambientSky, dir.y * 0.5f + 0.5f);
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
// 影（PCF 3x3）
// ==================================================
Texture2D<float> gShadowMap : register(t3);

float SampleShadow(float3 worldPos, float3 N, float3 L)
{
    if (gFrame.shadowEnabled == 0) { return 1.0f; }
    // 法線方向に少しずらしてシャドウアクネを減らす
    float slope = 1.0f - saturate(dot(N, L));
    float3 p = worldPos + N * gFrame.shadowTexelSize * (1.0f + 4.0f * slope);
    float4 sp = mul(float4(p, 1.0f), gFrame.shadowViewProj);
    sp.xyz /= sp.w;
    float2 uv = sp.xy * float2(0.5f, -0.5f) + 0.5f;
    if (any(uv < 0.0f) || any(uv > 1.0f) || sp.z > 1.0f) { return 1.0f; }

    float w, h;
    gShadowMap.GetDimensions(w, h);
    float2 texel = 1.0f / float2(w, h);
    float depth = sp.z - gFrame.shadowBias;
    float sum = 0.0f;
    [unroll] for (int y = -1; y <= 1; ++y)
    {
        [unroll] for (int x = -1; x <= 1; ++x)
        {
            sum += gShadowMap.SampleCmpLevelZero(gSamplerShadow, uv + float2(x, y) * texel, depth);
        }
    }
    float shadow = sum / 9.0f;
    return lerp(1.0f, shadow, gFrame.shadowStrength);
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
