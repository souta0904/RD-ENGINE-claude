// 縁取り（Unity の mouse/Outline と同じ見た目。ステンシルの代わりに印のテクスチャを使う）
//  1. MaskPS: 物の形を印のテクスチャ（R8）に 1 で塗る（深度は見ない）
//  2. OutlinePS: 法線の向きへ画面で gMaterial.metallic ピクセル広げた形を描き、印のある所（物そのもの）は描かない
//  色は gObject.tint、点滅の周期は gMaterial.roughness 秒（0 なら点滅しない）。深度を見ないので壁越しにも見える
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

#ifdef SKINNED
StructuredBuffer<float4x4> gPalette : register(t2);
#endif

Texture2D<float> gMask : register(t0);

void Skin(inout float4 pos, inout float3 nrm, VSInput input)
{
#ifdef SKINNED
    float4x4 skin =
        gPalette[input.joints.x] * input.weights.x +
        gPalette[input.joints.y] * input.weights.y +
        gPalette[input.joints.z] * input.weights.z +
        gPalette[input.joints.w] * input.weights.w;
    pos = mul(pos, skin);
    nrm = mul(float4(nrm, 0.0f), skin).xyz;
#endif
}

float4 MaskVS(VSInput input) : SV_POSITION
{
    float4 pos = float4(input.position, 1.0f);
    float3 nrm = input.normal;
    Skin(pos, nrm, input);
    return mul(mul(pos, gObject.world), gFrame.viewProj);
}

float MaskPS() : SV_TARGET
{
    return 1.0f;
}

float4 OutlineVS(VSInput input) : SV_POSITION
{
    float4 pos = float4(input.position, 1.0f);
    float3 nrm = input.normal;
    Skin(pos, nrm, input);
    float4 positionCS = mul(mul(pos, gObject.world), gFrame.viewProj);
    float3 normalWS = normalize(mul(float4(nrm, 0.0f), gObject.worldInvTranspose).xyz);
    float2 normalCS = mul(float4(normalWS, 0.0f), gFrame.viewProj).xy;
    float len = length(normalCS);
    if (len > 1e-6f)
    {
        // Unity と同じく、クリップ空間で w を掛けて画面上で一定のピクセル数だけ外へ出す
        float2 offset = normalCS / len * gMaterial.metallic * 2.0f / gFrame.screenSize;
        positionCS.xy += offset * positionCS.w;
    }
    return positionCS;
}

float4 OutlinePS(float4 svpos : SV_POSITION) : SV_TARGET
{
    if (gMask.Load(int3(svpos.xy, 0)) > 0.5f) { discard; }
    float blinkSec = gMaterial.roughness;
    float blink = blinkSec > 0.0f ? 0.5f - 0.5f * cos(gFrame.time * 6.28318530718f / blinkSec) : 1.0f;
    return float4(gObject.tint.rgb, gObject.tint.a * blink);
}
