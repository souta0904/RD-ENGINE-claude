// 2D/デバッグ用のシンプルなシェーダ群（Simple ルートシグネチャ）
//  b0: ルート定数（行列 + 追加パラメータ）
cbuffer Constants : register(b0)
{
    float4x4 gMatrix;
    float4 gParams;
};

Texture2D<float4> gTexture0 : register(t0);
Texture2D<float4> gTexture1 : register(t1);
SamplerState gLinearClamp : register(s0);
SamplerState gPointClamp : register(s1);
SamplerState gLinearWrap : register(s2);

// --------------------------------------------------
// デバッグ線
// --------------------------------------------------
struct LineVSInput
{
    float3 position : POSITION;
    float4 color : COLOR;
};
struct LineVSOutput
{
    float4 svpos : SV_POSITION;
    float4 color : COLOR;
};

LineVSOutput LineVS(LineVSInput input)
{
    LineVSOutput o;
    o.svpos = mul(float4(input.position, 1.0f), gMatrix);
    o.color = input.color;
    return o;
}

float4 LinePS(LineVSOutput input) : SV_TARGET
{
    return input.color;
}

// --------------------------------------------------
// スプライト
// --------------------------------------------------
struct SpriteVSInput
{
    float2 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};
struct SpriteVSOutput
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR;
};

SpriteVSOutput SpriteVS(SpriteVSInput input)
{
    SpriteVSOutput o;
    o.svpos = mul(float4(input.position, 0.0f, 1.0f), gMatrix);
    o.uv = input.uv;
    o.color = input.color;
    return o;
}

float4 SpritePS(SpriteVSOutput input) : SV_TARGET
{
    return gTexture0.Sample(gLinearClamp, input.uv) * input.color;
}
