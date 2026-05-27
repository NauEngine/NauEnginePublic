// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"
#include "in_out.hlsli"

// Constant buffer for test local material properties.
cbuffer TestBuffer : register(b1)
{
    float4 color;
};

Texture2D tex : register(t0);
SamplerState sampl : register(s0);

#ifndef GBUFFER_VARIANT

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 mvp;
};

VsOutput VSMain(VsInput input)
{
    const float3 LightPos = {0, 15, -30};
    const float3 lightVec = normalize(LightPos - input.position);
    const float diffuse = saturate(max(dot(lightVec, input.normal), 0.2) * 1.3);

    VsOutput output;

    output.position = mul(mvp, float4(input.position, 1.0f));
    output.color = float4(diffuse, diffuse, diffuse, 1.0f);
    output.texCoord = input.texCoord;

    return output;
}

float4 PSMain(VsOutput input) :
    SV_Target
{
    float4 albedo = tex.Sample(sampl, input.texCoord);
    return albedo * input.color;
}

float4 PSMainColored(VsOutput input) : SV_Target
{
    float4 albedo = tex.Sample(sampl, input.texCoord);
    return albedo * input.color * color;
}
#else

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 mvp;
    float4 worldViewPos;
};

VsOutputGBuff VSMain(VsInput input)
{
    VsOutputGBuff output;

    float3 pos = input.position;
    output.position = mul(mvp, float4(input.position, 1.0f));
    output.norm = input.normal;
    output.p2e = worldViewPos.xyz - pos;
    output.texCoord = input.texCoord;

    return output;
}

#include "gbuffer_base.hlsli"

GBUFFER_OUTPUT PSMain(VsOutputGBuff input)
{
    float4 screenpos = input.position;
    UnpackedGbuffer result;
    init_gbuffer(result);
    half4 albedo_roughness = tex.Sample(sampl, input.texCoord);
    albedo_roughness.a = 1;  // roughness
    half4 normal_smoothness = half4(0, 1, 0, 1);
    float3 normal;
    normal.xyz = (normal_smoothness.xyz * 2 - 1);
    // init_albedo_roughness(result, albedo_roughness);
    init_albedo(result, albedo_roughness.xyz);
    init_smoothness(result, normal_smoothness.a);
    //init_normal(result, perturb_normal(normal, normalize(input.norm), input.p2e, input.texCoord));
    init_normal(result, normalize(input.norm));
    init_metalness(result, 0);
    init_ao(result, 1);
    return encode_gbuffer(result, screenpos);
}

GBUFFER_OUTPUT PSMainColored(VsOutputGBuff input)
{
    float4 screenpos = input.position;

    UnpackedGbuffer result;
    init_gbuffer(result);
    float4 albedo = tex.Sample(sampl, input.texCoord);
    init_albedo(result, albedo.xyz);

    return encode_gbuffer(result, screenpos);
}
#endif
