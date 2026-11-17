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
    float3 vertexLight : TEXCOORD1;// 頂点ごとに計算した追加のライト（拡散）
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
    o.vertexLight = gFrame.additionalLightsPerVertex != 0 ? VertexLighting(o.worldPos, o.normal) : 0.0f;
    return o;
}

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
    if ((gMaterial.flags & kMatAlphaClip) && base.a < gMaterial.alphaCutoff)
    {
        discard;
    }
    if (gMaterial.flags & kMatUnlit)
    {
        return float4(ApplyFog(base.rgb + emissive, input.worldPos), base.a);
    }
    if (gMaterial.flags & kMatFlat)
    {
        return float4(ApplyFog(ShadeFlat(base.rgb, input.worldPos) + emissive, input.worldPos), base.a);
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
    // 追加のライト（点光源・スポットライト）。頂点ごとなら頂点で求めた拡散にアルベドを掛けるだけ
    if (gFrame.additionalLightsPerVertex != 0)
    {
        color += input.vertexLight * base.rgb * (1.0f - metallic);
    }
    else
    {
        uint count = AdditionalLightCount();
        for (uint k = 0; k < count; ++k)
        {
            float3 L, radiance;
            if (AdditionalLight(k, input.worldPos, L, radiance))
            {
                color += ShadeLight(N, V, L, radiance, base.rgb, metallic, roughness);
            }
        }
    }

    color += emissive;
    if (gFrame.debugCascades != 0 && gFrame.shadowEnabled != 0) { color *= CascadeDebugColor(input.worldPos); }
    color = ApplyFog(color, input.worldPos);
    return float4(color, base.a);
}
