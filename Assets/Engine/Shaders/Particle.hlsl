// ビルボードパーティクル（インスタンシング）
#include "Common.hlsli"

struct ParticleInstance
{
    float3 position;
    float size;
    float4 color;
    float rotation;
    float3 pad;
};
StructuredBuffer<ParticleInstance> gParticles : register(t2);
Texture2D<float4> gTexture : register(t0);

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
    float3 worldPos : POSITION;
};

VSOutput VSMain(uint vid : SV_VertexID, uint iid : SV_InstanceID)
{
    static const float2 kCorners[6] = { float2(-1, 1), float2(1, 1), float2(-1, -1), float2(-1, -1), float2(1, 1), float2(1, -1) };
    ParticleInstance p = gParticles[iid];
    float2 c = kCorners[vid];
    float s, co;
    sincos(p.rotation, s, co);
    float2 r = float2(c.x * co - c.y * s, c.x * s + c.y * co) * p.size * 0.5f;
    float3 wpos = p.position + gFrame.cameraRight * r.x + gFrame.cameraUp * r.y;

    VSOutput o;
    o.svpos = mul(float4(wpos, 1.0f), gFrame.viewProj);
    o.uv = c * float2(0.5f, -0.5f) + 0.5f;
    o.color = p.color;
    o.worldPos = wpos;
    return o;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 color = gTexture.Sample(gSamplerClamp, input.uv) * input.color * gMaterial.baseColor;
    color.rgb += gMaterial.emissive * color.a;
    color.rgb = ApplyFog(color.rgb, input.worldPos);
    return color;
}
