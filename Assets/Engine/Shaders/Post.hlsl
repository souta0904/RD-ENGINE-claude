// ポストエフェクト（ブルーム・トーンマップ・コピー）
cbuffer Constants : register(b0)
{
    float4 gExtra[4];// C++ では行列の場所。トーンマップでは色の調整とレトロ表示の値
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

// Neutral（Unity の Neutral トーンマップと同じ曲線）
float3 NeutralCurve(float3 x, float a, float b, float c, float d, float e, float f)
{
    return ((x * (a * x + c * b) + d * e) / (x * (a * x + b) + d * f)) - e / f;
}

float3 NeutralTonemap(float3 x)
{
    const float a = 0.2f, b = 0.29f, c = 0.24f, d = 0.272f, e = 0.02f, f = 0.3f;
    const float whiteLevel = 5.3f;
    float3 whiteScale = 1.0f / NeutralCurve(whiteLevel.xxx, a, b, c, d, e, f);
    x = NeutralCurve(x * whiteScale, a, b, c, d, e, f);
    return saturate(x * whiteScale);
}

// コントラストは Unity と同じく LogC の空間で中間の灰色を中心に伸ばす
static const float kLogCCut = 0.011361f, kLogCA = 5.555556f, kLogCB = 0.047996f, kLogCC = 0.244161f;
static const float kLogCD = 0.386036f, kLogCE = 5.301883f, kLogCF = 0.092819f;
static const float kLogCMidGray = 0.4135884f;

// 境目で式を切り替える（step(edge, x) は x >= edge で 1）
float3 LinearToLogC(float3 x)
{
    float3 hi = kLogCC * log10(max(kLogCA * x + kLogCB, 1e-6f)) + kLogCD;
    float3 lo = kLogCE * x + kLogCF;
    return lerp(lo, hi, step(kLogCCut, x));
}

float3 LogCToLinear(float3 x)
{
    float3 hi = (pow(10.0f, (x - kLogCD) / kLogCC) - kLogCB) / kLogCA;
    float3 lo = (x - kLogCF) / kLogCE;
    return lerp(lo, hi, step(kLogCE * kLogCCut + kLogCF, x));
}

float3 LinearToSRGB(float3 c)
{
    float3 hi = 1.055f * pow(max(c, 0.0f), 1.0f / 2.4f) - 0.055f;
    return lerp(c * 12.92f, hi, step(0.0031308f, c));
}

float3 SRGBToLinear(float3 c)
{
    float3 hi = pow(max((c + 0.055f) / 1.055f, 0.0f), 2.4f);
    return lerp(c / 12.92f, hi, step(0.04045f, c));
}

static const float kBayer4x4[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

// gParams.x = 露出, gParams.y = ブルーム強度, gParams.z = ビネット, gParams.w = トーンマップの方式（0 = ACES, 1 = Neutral, 2 = なし）
// gExtra[0] = (露出の倍率, コントラスト, 彩度, -), gExtra[1] = (レトロ表示の縦のピクセル数（0 なら使わない）, 色の段階数, -, -)
float4 TonemapPS(VSOutput input) : SV_TARGET
{
    float2 uv = input.uv;
    // レトロ表示: 縦 retroHeight ピクセルの升目の中心の色を使う
    float retroHeight = gExtra[1].x;
    float2 cell = 0.0f;
    if (retroHeight > 0.5f)
    {
        float w, h;
        gTexture0.GetDimensions(w, h);
        float2 res = float2(round(retroHeight * w / h), retroHeight);
        cell = floor(uv * res);
        uv = (cell + 0.5f) / res;
    }

    float3 hdr = gTexture0.SampleLevel(gPointClamp, uv, 0).rgb;
    float3 bloom = gTexture1.SampleLevel(gLinearClamp, uv, 0).rgb;
    float3 color = (hdr + bloom * gParams.y) * gParams.x * gExtra[0].x;

    // 色の調整（コントラスト → 彩度）
    color = max(LogCToLinear((LinearToLogC(max(color, 0.0f)) - kLogCMidGray) * gExtra[0].y + kLogCMidGray), 0.0f);
    float luma = dot(color, float3(0.2126729f, 0.7151522f, 0.0721750f));
    color = max(luma + gExtra[0].z * (color - luma), 0.0f);

    if (gParams.w < 0.5f) { color = ACESFilm(color); }
    else if (gParams.w < 1.5f) { color = NeutralTonemap(color); }
    // ビネット
    float2 d = uv - 0.5f;
    color *= 1.0f - dot(d, d) * gParams.z;
    color = saturate(color);

    // レトロ表示: sRGB の段階に、4x4 のベイヤー行列でディザをかけて減らす
    if (retroHeight > 0.5f)
    {
        uint2 p = uint2(cell) % 4;
        float threshold = (kBayer4x4[p.x + p.y * 4] + 0.5f) / 16.0f;
        float levels = gExtra[1].y - 1.0f;
        float3 s = LinearToSRGB(color);
        s = floor(s * levels + threshold) / levels;
        color = SRGBToLinear(saturate(s));
    }
    // 出力先は sRGB の RTV なのでリニアのまま書き込む（ハードウェアで変換される）
    return float4(color, 1.0f);
}

float4 BlitPS(VSOutput input) : SV_TARGET
{
    return gTexture0.SampleLevel(gLinearClamp, input.uv, 0);
}
