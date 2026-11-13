// シャドウマップ（深度のみ）
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

float4 VSMain(VSInput input) : SV_POSITION
{
    float4 pos = float4(input.position, 1.0f);
#ifdef SKINNED
    float4x4 skin =
        gPalette[input.joints.x] * input.weights.x +
        gPalette[input.joints.y] * input.weights.y +
        gPalette[input.joints.z] * input.weights.z +
        gPalette[input.joints.w] * input.weights.w;
    pos = mul(pos, skin);
#endif
    float4 wpos = mul(pos, gObject.world);
    // 描いているカスケードの行列（ビューポートでタイルに配置される）
    return mul(wpos, gFrame.shadowViewProj[gFrame.shadowCascade]);
}
