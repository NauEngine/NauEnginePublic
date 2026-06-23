// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"
#include "in_out.hlsli"

Texture2D tex1 : register(t0);
Texture2D tex2 : register(t1);

SamplerState sampl1 : register(s0);
SamplerState sampl2 : register(s1);

#ifndef GBUFFER_VARIANT

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 mvp;
};
cbuffer PropertyBuffer : register(b1)
{
    float blendFactor;
};

VsOutput VSMain(VsInput input)
{
    VsOutput output;

    const float3 LightPos = {0, 15, -30};

    const float3 lightVec = normalize(LightPos - input.position);
    const float diffuse = saturate(max(dot(lightVec, input.normal), 0.2) * 1.3);

    output.position = mul(mvp, float4(input.position, 1.0f));
    output.color = float4(diffuse, diffuse, diffuse, 1.0f);

    output.texCoord = input.texCoord;

    return output;
}

float4 PSMain(VsOutput input) : SV_Target
{
    const float4 color1 = tex1.Sample(sampl1, input.texCoord);
    const float4 color2 = tex2.Sample(sampl2, input.texCoord);
    const float4 result = lerp(color1, color2, blendFactor);

    return result * input.color;
}

#else

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 mvp;
    float4 worldViewPos;
};
cbuffer PropertyBuffer : register(b1)
{
    float blendFactor;
};

VsOutputGBuff VSMain(VsInput input)
{
    VsOutputGBuff output;

    float3 pos = input.position;
    output.position = mul(mvp, float4(input.position, 1.0f));
    output.norm = input.normal;
    output.texCoord = input.texCoord;
    output.p2e = worldViewPos.xyz - pos;

    return output;
}

#include "gbuffer_base.hlsli"

GBUFFER_OUTPUT PSMain(VsOutputGBuff input)
{
    float4 screenpos = input.position;
    UnpackedGbuffer result;
    init_gbuffer(result);

    const float4 color1 = tex1.Sample(sampl1, input.texCoord);
    const float4 color2 = tex2.Sample(sampl2, input.texCoord);

    half4 albedo = half4(lerp(color1, color2, blendFactor));

    half4 normal_smoothness = half4(0, 1, 0, 1);
    float3 normal;
    normal.xyz = (normal_smoothness.xyz * 2 - 1);
    init_albedo(result, albedo.xyz);
    init_smoothness(result, normal_smoothness.a);
    init_normal(result, perturb_normal(normal, normalize(input.norm), input.p2e, input.texCoord));

    init_metalness(result, 0);
    init_ao(result, 1);
    return encode_gbuffer(result, screenpos);
}

#endif