// ポストエフェクト（ブルーム・トーンマップ・コピー）
cbuffer Constants : register(b0)
{
    float4x4 gUnused;
    float4 gParams;// 用途はパスごと
};

Texture2D<float4> gTexture0 : register(t0);
Texture2D<float4> gTexture1 : register(t1);
SamplerState gLinearClamp : register(s0);
SamplerState gPointClamp : register(s1);

struct VSOutput
{
    float4 svpos : SV_POSITION;
    float2 uv : TEXCOORD0;
};

VSOutput FullscreenVS(uint id : SV_VertexID)
{
    VSOutput o;
    o.uv = float2((id << 1) & 2, id & 2);
    o.svpos = float4(o.uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f), 0.0f, 1.0f);
    return o;
}

// 13 タップのダウンサンプル（Call of Duty 方式、ちらつきを抑える）
float3 Downsample13(float2 uv)
{
    float w, h;
    gTexture0.GetDimensions(w, h);
    float2 t = 1.0f / float2(w, h);
    float3 a = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(-2, -2), 0).rgb;
    float3 b = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(0, -2), 0).rgb;
    float3 c = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(2, -2), 0).rgb;
    float3 d = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(-2, 0), 0).rgb;
    float3 e = gTexture0.SampleLevel(gLinearClamp, uv, 0).rgb;
    float3 f = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(2, 0), 0).rgb;
    float3 g = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(-2, 2), 0).rgb;
    float3 h2 = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(0, 2), 0).rgb;
    float3 i = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(2, 2), 0).rgb;
    float3 j = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(-1, -1), 0).rgb;
    float3 k = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(1, -1), 0).rgb;
    float3 l = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(-1, 1), 0).rgb;
    float3 m = gTexture0.SampleLevel(gLinearClamp, uv + t * float2(1, 1), 0).rgb;
    float3 result = e * 0.125f;
    result += (a + c + g + i) * 0.03125f;
    result += (b + d + f + h2) * 0.0625f;
    result += (j + k + l + m) * 0.125f;
    return result;
}

// gParams.x = しきい値, gParams.y = ソフトニー
float4 BloomPrefilterPS(VSOutput input) : SV_TARGET
{
    float3 color = Downsample13(input.uv);
    color = min(color, 64.0f.xxx);// 極端に明るい画素による点滅を防ぐ
    float brightness = max(color.r, max(color.g, color.b));
    float knee = gParams.x * gParams.y;
    float soft = clamp(brightness - gParams.x + knee, 0.0f, 2.0f * knee);
    soft = soft * soft / (4.0f * knee + 1e-5f);
    float contribution = max(soft, brightness - gParams.x) / max(brightness, 1e-5f);
    return float4(color * contribution, 1.0f);
}

float4 BloomDownPS(VSOutput input) : SV_TARGET
{
    return float4(Downsample13(input.uv), 1.0f);
}

// テントフィルタでアップサンプル（加算合成で重ねる）, gParams.x = 半径
float4 BloomUpPS(VSOutput input) : SV_TARGET
{
    float w, h;
    gTexture0.GetDimensions(w, h);
    float2 t = gParams.x / float2(w, h);
    float3 s = gTexture0.SampleLevel(gLinearClamp, input.uv + float2(-t.x, -t.y), 0).rgb;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(0, -t.y), 0).rgb * 2.0f;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(t.x, -t.y), 0).rgb;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(-t.x, 0), 0).rgb * 2.0f;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv, 0).rgb * 4.0f;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(t.x, 0), 0).rgb * 2.0f;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(-t.x, t.y), 0).rgb;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(0, t.y), 0).rgb * 2.0f;
    s += gTexture0.SampleLevel(gLinearClamp, input.uv + float2(t.x, t.y), 0).rgb;
    return float4(s / 16.0f, 1.0f);
}

// ACES（Narkowicz の近似）
float3 ACESFilm(float3 x)
{
    return saturate((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f));
}

// gParams.x = 露出, gParams.y = ブルーム強度, gParams.z = ビネット, gParams.w = トーンマップ有効
float4 TonemapPS(VSOutput input) : SV_TARGET
{
    float3 hdr = gTexture0.SampleLevel(gPointClamp, input.uv, 0).rgb;
    float3 bloom = gTexture1.SampleLevel(gLinearClamp, input.uv, 0).rgb;
    float3 color = (hdr + bloom * gParams.y) * gParams.x;
    if (gParams.w > 0.5f) { color = ACESFilm(color); }
    // ビネット
    float2 d = input.uv - 0.5f;
    color *= 1.0f - dot(d, d) * gParams.z;
    // 出力先は sRGB の RTV なのでリニアのまま書き込む（ハードウェアで変換される）
    return float4(saturate(color), 1.0f);
}

float4 BlitPS(VSOutput input) : SV_TARGET
{
    return gTexture0.SampleLevel(gLinearClamp, input.uv, 0);
}
