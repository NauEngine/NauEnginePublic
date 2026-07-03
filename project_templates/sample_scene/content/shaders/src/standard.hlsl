// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "shader_defines.h"

#include "in_out.hlsli"
#include "common_types.hlsli"

#include "shader_global.hlsli"
#include "gbuffer_base.hlsli"

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metalRoughnessAoTex : register(t2);

SamplerState albedoSampler : register(s0);
SamplerState normalSampler : register(s1);
SamplerState metalRoughnessAoSampler : register(s2);

#ifndef INSTANCED

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 mvp;
    float4x4 worldMatrix;
    float4x4 normalMatrix;
    float4 worldViewPos;
};

VsOutputGBuff VSMain(VsInputLit input)
{
    VsOutputGBuff output;

    output.position = mul(mvp, float4(input.position, 1.0f));
    output.norm = normalize(mul(normalMatrix, float4(input.normal, 0.0f)).xyz);
    output.tangent = normalize(mul(worldMatrix, float4(input.tangent.xyz, 0.0f)).xyz);
    output.texCoord = input.texCoord;
    output.texCoord.y = 1.0f - output.texCoord.y;

    return output;
}
#else // !INSTANCED

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 vp;
    float4 worldViewPos;
};

cbuffer InstanceDataBuffer : register(b1)
{
    float4 instanceBaseID;
};

StructuredBuffer<InstanceData> instanceBuffer : register(t0);

VsOutputGBuff VSMain(VsInputLit input, uint instID : SV_InstanceID)
{
    VsOutputGBuff output;

    const uint instIdx = instanceBaseID.x + instID;
    output.position = mul(mul(vp, instanceBuffer[instIdx].worldMatrix), float4(input.position, 1.0f));
    output.norm = mul(instanceBuffer[instIdx].normalMatrix, float4(input.normal.xyz, 0.0f)).xyz;
    output.tangent = normalize(mul(instanceBuffer[instIdx].worldMatrix, float4(input.tangent.xyz, 0.0f)).xyz);
    output.texCoord = input.texCoord;
    output.texCoord.y = 1.0f - output.texCoord.y;

    return output;
}

#endif // !INSTANCED

GBUFFER_OUTPUT PSMain(VsOutputGBuff input)
{
    half3 albedo = albedoTex.Sample(albedoSampler, input.texCoord).rgb;
    half4 metalRoughnessAo = metalRoughnessAoTex.Sample(metalRoughnessAoSampler, input.texCoord);

    float3 tangentNormal = normalTex.Sample(normalSampler, input.texCoord).xyz * 2.0f - 1.0f;
    float3 worldSpaceNormal = normalize(TangentToWorld(tangentNormal, GetTBN(input.norm, input.tangent)));

    float4 screenpos = input.position;
    
    UnpackedGbuffer result;
    init_gbuffer(result);

    init_albedo(result, albedo);

#ifdef EMISSIVE
    init_material(result, SHADING_EMISSIVE);
    init_emission(result, 0.666f); // test value
#else
    init_ao(result, saturate(metalRoughnessAo.r + 0.1f));
#endif

    init_normal(result, worldSpaceNormal);
    init_smoothness(result, 1.0f - metalRoughnessAo.g);
    init_metalness(result, metalRoughnessAo.b);

    return encode_gbuffer(result, screenpos);
}

