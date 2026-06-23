// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"

#include "shader_global.hlsli"
#include "gbuffer_base.hlsli"

struct VsInput
{
    float3 Position : POSITION;
    float3 Normal : NORMAL;
    float2 texCoord : TEXCOORD0; 
    float4 Tangent : TANGENT;

    float4 BoneWeights : BLENDWEIGHT;
    uint4 BoneIndices : BLENDINDICES;
};

struct VsOutput
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
    float2 texCoord  : TEXCOORD0;
    float3 tangent : TANGENT;
};

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metalRoughnessAoTex : register(t2);

SamplerState albedoSampler : register(s0);
SamplerState normalSampler : register(s1);
SamplerState metalRoughnessAoSampler : register(s2);

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    matrix vp;
    matrix BonesTransforms[NAU_MAX_SKINNING_BONES_COUNT];
    matrix BonesNormalTransforms[NAU_MAX_SKINNING_BONES_COUNT];
};

VsOutput VSMain(VsInput input)
{
    VsOutput output;

    float4 skinnedPos = float4(0.0, 0.0, 0.0, 0.0);
    float3 skinnedNormal = float3(0.0, 0.0, 0.0);
    float3 skinnedTangent = float3(0.0, 0.0, 0.0);
    
    [unroll]
    for (int i = 0; i < 4; i++)
    {
        const uint boneIndex = input.BoneIndices[i];
        if (boneIndex < NAU_MAX_SKINNING_BONES_COUNT)
        {
            skinnedPos += input.BoneWeights[i] * mul(BonesTransforms[boneIndex], float4(input.Position, 1.0));
            skinnedNormal += input.BoneWeights[i] * mul(BonesNormalTransforms[boneIndex], float4(input.Normal, 0.0)).xyz;
            skinnedTangent += input.BoneWeights[i] * mul(BonesTransforms[boneIndex], float4(input.Tangent.xyz, 0.0)).xyz;
        }
    }

    output.position = mul(vp, skinnedPos);
    output.texCoord = input.texCoord;
    output.normal = normalize(skinnedNormal);
    output.tangent = normalize(skinnedTangent);

    return output;
}

GBUFFER_OUTPUT PSMain(VsOutput input)
{
    half3 albedo = albedoTex.Sample(albedoSampler, input.texCoord).rgb;
    half4 metalRoughnessAo = metalRoughnessAoTex.Sample(metalRoughnessAoSampler, input.texCoord);
    float3 tangentNormal = normalTex.Sample(normalSampler, input.texCoord).xyz * 2.0f - 1.0f;

    float3 worldSpaceNormal = normalize(TangentToWorld(tangentNormal, GetTBN(input.normal, input.tangent)));
    float4 screenpos = input.position;
    
    UnpackedGbuffer result;
    init_gbuffer(result);

    init_albedo(result, albedo);
    init_ao(result, saturate(metalRoughnessAo.r + 0.1f));
    
    init_normal(result, worldSpaceNormal);
    init_smoothness(result, 1.0f - metalRoughnessAo.g);
    init_metalness(result, metalRoughnessAo.b);

    return encode_gbuffer(result, screenpos);
}
