// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "shader_defines.h"
#include "in_out.hlsli"

Texture2D frame_tex : register(t0);
SamplerState sampl : register(s0);

GLOBAL_CBUFFER(PropertyBuffer) : register(b0)
{
    float exposure;
};

VsOutput VSMain(uint id : SV_VertexID)
{
    VsOutput output = (VsOutput)0;

    //TODO: Rewrite to use only one triangle in future
    output.texCoord = float2((id << 1) & 2, id & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    output.color = float4(output.texCoord, 1, 0.5);

    return output;
}

float3 ReinhardMain(float3 texColor)
{
    float3 retColor = texColor / (texColor + 1);  // Reinhard
    // retColor = pow(retColor,1.0f/2.2f); // Gamma
    return retColor;
}

#include <ColorSpaceUtility.hlsl>
float3 FilmicMain(float3 texColor)
{
    float3 x = max(0, texColor - 0.004);  // Filmic Curve
    float3 retColor = (x * (6.2 * x + .5)) / (x * (6.2 * x + 1.7) + 0.06);
    // retColor = pow(retColor,2.2f); // Gamma
    return retColor;
}
float3 Uncharted2Tonemap(float3 x)
{
    float A = 0.15;
    float B = 0.50;
    float C = 0.10;
    float D = 0.20;
    float E = 0.02;
    float F = 0.30;
    float W = 11.2;

    return ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
}

float3 UnchartedMain(float3 texColor)
{
    float ExposureBias = 2.0f;
    float3 curr = Uncharted2Tonemap(ExposureBias * texColor);
    float3 W = 5;
    float3 whiteScale = 1.0f / Uncharted2Tonemap(W);
    float3 color = curr * whiteScale;

    return color;
}
// #define tonemap FilmicMain
half grainFromUV(float2 uv)
{
    return frac(sin(uv.x + uv.y * 543.31) * 493013.0);
}
#define tex2Dlod(tex, sampler, uv) tex.SampleLevel(sampler, (uv).xy, (uv).w)

#if USE_UNCHARTED_TONEMAP
#define tonemap(color)  UnchartedMain(color)
#elif USE_FILMIC_TONEMAP
#define tonemap(color)  FilmicMain(color)
#elif USE_REINHARD_TONEMAP
#define tonemap(color)  ReinhardMain(color)
#else 
#define tonemap(color) (color)
#endif
    
half3 sampleSceneColor(float2 tc)
{
    return tex2Dlod(frame_tex, sampl, float4(tc, 0, 0)).xyz;
}
float4 PSMain(VsOutput input) :
    SV_Target
{
// #define USE_COLOR_FRINGE 1
// #define USE_GRAIN_JITTER 1
#if USE_COLOR_FRINGE
    float2 screenPos = input.texCoord.xy * 2 - 1;
    float2 sceneUVJitter = float2(0.0, 0.0);
    #if USE_GRAIN_JITTER
    float2 sceneUV = input.texCoord.xy;
    float grainRandomFull = 1.1;
    half grain = grainFromUV(sceneUV + grainRandomFull);
    float grainIntensity = 1;
    float2 grainUV = sceneUV + 0.5 / float2(1920, 1080);
    sceneUV = lerp(sceneUV, grainUV.xy, (1.0 - grain * grain) * grainIntensity);
    sceneUVJitter = sceneUV.xy - input.texCoord.xy;
    #else
    float2 sceneUV = input.texCoord.xy;
    #endif

    float3 chromatic_aberration_params = float3(0.1, 0.1, 0.15);
    float2 caScale = chromatic_aberration_params.xy;
    float caStartOffset = chromatic_aberration_params.z;

    float4 uvRG;
    uvRG = screenPos.xyxy - sign(screenPos.xy).xyxy * saturate(abs(screenPos.xy) - caStartOffset).xyxy * caScale.xxyy;
    uvRG = uvRG * 0.5 + 0.5;

    half3 frame = sampleSceneColor(uvRG.xy + sceneUVJitter.xy);
    frame.g = sampleSceneColor(uvRG.zw + sceneUVJitter.xy).g;
    frame.b = sampleSceneColor(sceneUV).b;
#else
    half3 frame = sampleSceneColor(input.texCoord);  // fixed exposure of 0.25!
#endif
    return tonemap(frame * exposure).rgbr;  // we write to srgb, so accurateLinearToSRGB not needed
}