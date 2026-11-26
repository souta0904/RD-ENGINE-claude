// ディファードのライティング（画面全体に 1 回描く）
//  G バッファー（Surface.hlsli の GBUFFER 版が書く）と深度から点の位置と質感を戻し、
//  フォワードの PSMain と同じ計算で光を当てて HDR に書く
//  フォワードと違うところ: 物ごとのライトの数の制限（Lights Per Object）はなく、すべての追加のライトを当てる
#include "Common.hlsli"

Texture2D<float4> gGBufferAlbedo : register(t0);   // rgb 色、a 金属感
Texture2D<float4> gGBufferNormal : register(t1);   // xyz 法線、w 粗さ
Texture2D<float4> gGBufferEmission : register(t4); // rgb 発光（Unlit / Flat は色そのもの）、a 種類
Texture2D<float> gDepth : register(t5);

float4 VSMain(uint id : SV_VertexID) : SV_POSITION
{
    float2 uv = float2((id << 1) & 2, id & 2);
    return float4(uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
}

float4 PSMain(float4 svpos : SV_POSITION) : SV_TARGET
{
    int3 p = int3(svpos.xy, 0);
    float depth = gDepth.Load(p);
    // 何も描かれていない所は、フォワードで消すときと同じ色にする（あとで空を描く）
    if (depth >= 1.0f) { return float4(gFrame.skyHorizon, 1.0f); }

    float2 uv = svpos.xy / gFrame.screenSize;
    float4 clip = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, depth, 1.0f);
    float4 world = mul(clip, gFrame.invViewProj);
    float3 worldPos = world.xyz / world.w;

    float4 emission = gGBufferEmission.Load(p);
    uint kind = uint(emission.a + 0.5f);
    if (kind & 1u) { return float4(ApplyFog(emission.rgb, worldPos), 1.0f); }
    bool receiveShadow = (kind & 2u) != 0;

    float4 albedoMetal = gGBufferAlbedo.Load(p);
    float4 normalRough = gGBufferNormal.Load(p);
    float3 albedo = albedoMetal.rgb;
    float metallic = albedoMetal.a;
    float3 N = normalize(normalRough.xyz);
    float roughness = normalRough.w;
    float3 V = normalize(gFrame.cameraPos - worldPos);

    float3 color = ShadeAmbient(N, V, albedo, metallic, roughness);
    for (uint li = 0; li < gFrame.dirLightCount; ++li)
    {
        DirLight light = gLights.dirLights[li];
        float3 L = -light.direction;
        float shadow = (light.castShadow && receiveShadow) ? SampleShadow(worldPos, N, L) : 1.0f;
        color += ShadeLight(N, V, L, light.color * light.intensity * shadow, albedo, metallic, roughness);
    }
    // 頂点ごとに計算するときは G バッファーの発光に入っている
    if (gFrame.additionalLightsPerVertex == 0)
    {
        uint count = gFrame.pointLightCount + gFrame.spotLightCount;
        for (uint k = 0; k < count; ++k)
        {
            float3 L, radiance;
            if (AdditionalLightAll(k, worldPos, L, radiance))
            {
                color += ShadeLight(N, V, L, radiance, albedo, metallic, roughness);
            }
        }
    }

    color += emission.rgb;
    if (gFrame.debugCascades != 0 && gFrame.shadowEnabled != 0) { color *= CascadeDebugColor(worldPos); }
    return float4(ApplyFog(color, worldPos), 1.0f);
}
