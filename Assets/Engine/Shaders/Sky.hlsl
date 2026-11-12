// 空（グラデーション + 太陽）
#include "Common.hlsli"

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float2 ndc : TEXCOORD0;
};

VSOutput VSMain(uint id : SV_VertexID)
{
    // 画面全体を覆う三角形（最も奥の深度）
    float2 uv = float2((id << 1) & 2, id & 2);
    VSOutput o;
    o.ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
    o.svpos = float4(o.ndc, 1.0f, 1.0f);
    return o;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 world = mul(float4(input.ndc, 1.0f, 1.0f), gFrame.invViewProj);
    float3 dir = normalize(world.xyz / world.w - gFrame.cameraPos);

    float h = dir.y;
    float3 color = lerp(gFrame.skyHorizon, gFrame.skyZenith, pow(saturate(h), 0.5f));
    // 地平線より下は地面色へ
    color = lerp(color, gFrame.ambientGround * 0.5f, saturate(-h * 4.0f));

    if (gFrame.dirLightCount > 0)
    {
        DirLight sun = gLights.dirLights[0];
        float d = saturate(dot(dir, -sun.direction));
        color += sun.color * (pow(d, 2000.0f) * 50.0f + pow(d, 32.0f) * 0.3f) * sun.intensity * 0.2f;
    }
    return float4(color, 1.0f);
}
