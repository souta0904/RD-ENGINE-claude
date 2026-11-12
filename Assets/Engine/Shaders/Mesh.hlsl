// メッシュ描画（SKINNED を定義するとスキニング版）
#include "Common.hlsli"

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
};

#ifdef SKINNED
StructuredBuffer<float4x4> gPalette : register(t2);
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
    VSOutput o;
    float4 wpos = mul(pos, gObject.world);
    o.worldPos = wpos.xyz;
    o.svpos = mul(wpos, gFrame.viewProj);
    o.normal = normalize(mul(float4(nrm, 0.0f), gObject.worldInvTranspose).xyz);
    o.tangent = float4(normalize(mul(float4(tan, 0.0f), gObject.world).xyz), input.tangent.w);
    o.uv = input.uv;
    return o;
}

Texture2D<float4> gAlbedo : register(t0);
Texture2D<float4> gNormalMap : register(t1);

float4 PSMain(VSOutput input, bool frontFace : SV_IsFrontFace) : SV_TARGET
{
    float4 base = gMaterial.baseColor;
    if (gMaterial.flags & kMatHasAlbedo)
    {
        base *= gAlbedo.Sample(gSamplerWrap, input.uv);
    }
    base *= gObject.tint;
    if ((gMaterial.flags & kMatAlphaClip) && base.a < gMaterial.alphaCutoff)
    {
        discard;
    }
    if (gMaterial.flags & kMatUnlit)
    {
        return float4(ApplyFog(base.rgb + gMaterial.emissive, input.worldPos), base.a);
    }

    float3 N = normalize(input.normal);
    if (!frontFace) { N = -N; }// 両面描画の裏面
    if (gMaterial.flags & kMatHasNormal)
    {
        float3 T = normalize(input.tangent.xyz - N * dot(input.tangent.xyz, N));
        float3 B = cross(N, T) * input.tangent.w;
        float3 tn = gNormalMap.Sample(gSamplerWrap, input.uv).xyz * 2.0f - 1.0f;
        N = normalize(tn.x * T + tn.y * B + tn.z * N);
    }
    float3 V = normalize(gFrame.cameraPos - input.worldPos);
    float metallic = gMaterial.metallic;
    float roughness = clamp(gMaterial.roughness, 0.04f, 1.0f);

    float3 color = ShadeAmbient(N, V, base.rgb, metallic, roughness);

    // 平行光源
    for (uint i = 0; i < gFrame.dirLightCount; ++i)
    {
        DirLight light = gLights.dirLights[i];
        float3 L = -light.direction;
        float shadow = 1.0f;
        if (light.castShadow && (gMaterial.flags & kMatReceiveShadow))
        {
            shadow = SampleShadow(input.worldPos, N, L);
        }
        color += ShadeLight(N, V, L, light.color * light.intensity * shadow, base.rgb, metallic, roughness);
    }
    // 点光源
    for (uint p = 0; p < gFrame.pointLightCount; ++p)
    {
        PointLight light = gLights.pointLights[p];
        float3 toLight = light.position - input.worldPos;
        float dist = length(toLight);
        if (dist >= light.range) { continue; }
        float3 L = toLight / dist;
        // 窓関数付きの逆二乗減衰
        float falloff = saturate(1.0f - pow(dist / light.range, 4.0f));
        float att = falloff * falloff / (dist * dist + 1.0f);
        color += ShadeLight(N, V, L, light.color * light.intensity * att, base.rgb, metallic, roughness);
    }
    // スポットライト
    for (uint s = 0; s < gFrame.spotLightCount; ++s)
    {
        SpotLight light = gLights.spotLights[s];
        float3 toLight = light.position - input.worldPos;
        float dist = length(toLight);
        if (dist >= light.range) { continue; }
        float3 L = toLight / dist;
        float falloff = saturate(1.0f - pow(dist / light.range, 4.0f));
        float att = falloff * falloff / (dist * dist + 1.0f);
        float cone = saturate((dot(-L, light.direction) - light.cosOuter) / max(light.cosInner - light.cosOuter, 1e-4f));
        color += ShadeLight(N, V, L, light.color * light.intensity * att * cone * cone, base.rgb, metallic, roughness);
    }

    color += gMaterial.emissive;
    color = ApplyFog(color, input.worldPos);
    return float4(color, base.a);
}
