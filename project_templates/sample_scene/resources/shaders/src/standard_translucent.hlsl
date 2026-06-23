// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"

#include "in_out.hlsli"
#include "common_types.hlsli"

#include "shader_global.hlsli"
#include "pbr.hlsli"

#define DIELECTRIC_F0 float3(0.04, 0.04, 0.04)

Texture2D albedoTex : register(t0);
Texture2D normalTex : register(t1);
Texture2D metalRoughnessAoTex : register(t2);

SamplerState albedoSampler : register(s0);
SamplerState normalSampler : register(s1);
SamplerState metalRoughnessAoSampler : register(s2);

half luminance(half3 col)
{
    return dot(col, half3(0.299, 0.587, 0.114));
}

#ifndef INSTANCED

    GLOBAL_CBUFFER(SceneBuffer) : register(b0)
    {
        float4x4 mvp;
        float4x4 worldMatrix;
        float4x4 normalMatrix;  
        float4 worldViewPos;
    };

    cbuffer ColorBuffer : register(b1)
    {
        float4 color;
    };

    VsOutputLitForward VSMain(VsInputLit input)
    {
        VsOutputLitForward output;

        output.position = mul(mvp, float4(input.position, 1.0f));
        output.norm = normalize(mul(normalMatrix, float4(input.normal, 0.0f)).xyz);
        output.tangent = normalize(mul(worldMatrix, float4(input.tangent.xyz, 0.0f)).xyz);
        output.texCoord = input.texCoord;
        
        output.worldPos = mul(worldMatrix, float4(input.position, 1.0f)).xyz;
        output.color = color;

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

    cbuffer ColorBuffer : register(b2)
    {
        float4 color;
    };

    StructuredBuffer<InstanceData> instanceBuffer : register(t0);

    VsOutputLitForward VSMain(VsInputLit input, uint instID : SV_InstanceID)
    {
        VsOutputLitForward output;

        const uint instIdx = instanceBaseID.x + instID;
        output.position = mul(mul(vp, instanceBuffer[instIdx].worldMatrix), float4(input.position, 1.0f));
        output.norm = mul(instanceBuffer[instIdx].normalMatrix, float4(input.normal.xyz, 0.0f)).xyz;
        output.tangent = normalize(mul(instanceBuffer[instIdx].worldMatrix, float4(input.tangent.xyz, 0.0f)).xyz);
        output.texCoord = input.texCoord;
        
        output.worldPos = mul(instanceBuffer[instIdx].worldMatrix, float4(input.position, 1.0f)).xyz;
        output.color = color;

        return output;
    }

#endif // !INSTANCED

float4 PSMain(VsOutputLitForward input) : SV_Target
{
    half4 albedo = albedoTex.Sample(albedoSampler, input.texCoord);
    half4 metalRoughnessAo = metalRoughnessAoTex.Sample(metalRoughnessAoSampler, input.texCoord);
    float3 tangentNormal = normalTex.Sample(normalSampler, input.texCoord).xyz * 2.0f - 1.0f;

    const float3 diffuseColor = float3(albedo.rgb);
    const float linearRoughness = float(metalRoughnessAo.b); // todo: NAU-1797: srgb/linear ?
    const float metallness = float(metalRoughnessAo.g);
    const float ao = 1.0f;

    float3 worldSpaceNormal = normalize(TangentToWorld(tangentNormal, GetTBN(input.norm, input.tangent)));

    float3 pointToEye = worldViewPos.xyz - input.worldPos;
    float3 view = normalize(pointToEye);

    // lights
    float3 lightDir = normalize(float3(0.5f, -0.5f, 0.0f)); // todo: NAU-1797 pass direct light dir

    float NoL = saturate(dot(worldSpaceNormal, lightDir)) + 1e-5;
    float NdotV = dot(worldSpaceNormal, view);
    float NoV = abs(NdotV) + 1e-5;
    float ggx_alpha = linearRoughness * linearRoughness;
    
    half3 lightColor = half3(1.0, 1.0, 1.0); // todo: NAU-1797 pass direct light color

    const float3 reflectionVec = 2 * NdotV * worldSpaceNormal - view;
    float3 roughR = getRoughReflectionVec(reflectionVec, worldSpaceNormal, ggx_alpha);

    float opacity = albedo.a; // todo: NAU-1797 make color4 material prop, pass as Per-Instance Data or as regular CB-member (non-instanced)
    
    half fresnel0Dielectric = 0.04f;
    fresnel0Dielectric = lerp(fresnel0Dielectric, 0.01f, linearRoughness * opacity);
    fresnel0Dielectric *= (1 - opacity);
    half3 specularColor = lerp(half3(fresnel0Dielectric, fresnel0Dielectric, fresnel0Dielectric), albedo.rgb, half(metallness));

    half specularStrength = saturate(luminance(albedo.rgb) * (1 / 0.04)) * 0.9 + 0.1;

    half3 result = standardBRDF(NoV, NoL, diffuseColor, ggx_alpha, linearRoughness, specularColor, specularStrength, lightDir, view, half3(worldSpaceNormal)) * lightColor;

    return float4(float3(result), 1.0f) * input.color;
}
