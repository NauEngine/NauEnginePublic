// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "shader_defines.h"
#include "in_out.hlsli"
#include "gbuffer_read.hlsli"
#include "pbr.hlsli"
#include "csm.hlsli"

#define DIELECTRIC_F0 float3(0.04, 0.04, 0.04)

TextureCube irradianceMap: register(t4);
TextureCube reflectionMap: register(t5);

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 globtm_inv;
    float4 worldViewPos;
    float4 lightDirection;
    float4 lightColorIntensity;
    float4 envIntensity;
};

VsOutputResolve VSMain(uint id : SV_VertexID)
{
    VsOutputResolve output = (VsOutputResolve)0;

    output.texCoord = float2((id << 1) & 2, id & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);

    return output;
}

half3 getSkyReflection(float linearRoughness, float3 roughReflection, float NoV)
{
    const float lod = linearRoughness * (REFLECTION_MAP_MIP_COUNT - 1);
    return reflectionMap.SampleLevel(default_sampler, roughReflection, lod).rgb;
}

float4 PSMain(VsOutputResolve input) : SV_Target
{
    const float rawDepth = depth_gbuf.SampleLevel(default_sampler, input.texCoord, 0).x;
    if(rawDepth == 0)
    {
        return float4(0,0,0,1);
    }
    
    ProcessedGbuffer gbuffer = readProcessedGbuffer(input.texCoord);

    // depth / viewpos
    float4 farpos = float4(input.texCoord.x * 2 - 1, (1 - input.texCoord.y) * 2 - 1, rawDepth, 1.0);
    float4 worldpos_prj = mul(globtm_inv, farpos);
    float4 worldPos = worldpos_prj / worldpos_prj.w;
    float3 pointToEye = worldViewPos.xyz - worldPos.xyz;

    float3 view = normalize(pointToEye);

    // lights
    float3 lightDir = normalize(lightDirection.xyz);

	// todo: (is needed?????) replace metallic surfaces with albedo of their specular value
	half3 specularColor = gbuffer.specularColor;
    gbuffer.diffuseColor = lerp(gbuffer.diffuseColor, gbuffer.specularColor, gbuffer.metallness);

	float NoL = saturate(dot(gbuffer.normal, lightDir)) + 1e-5;
	float NdotV = dot(gbuffer.normal, view);
	float NoV = abs(NdotV) + 1e-5;
	float ggx_alpha = gbuffer.linearRoughness * gbuffer.linearRoughness;
	
	half shadowTerm = get_csm_shadow(pointToEye, 1.0f).x;
	half3 lightColor = lightColorIntensity.rgb * lightColorIntensity.a;

	const float3 irradiance = irradianceMap.Sample(default_sampler, gbuffer.normal).rgb * envIntensity.x;
	
	const float3 F0 = lerp(DIELECTRIC_F0, gbuffer.diffuseColor, gbuffer.metallness);
	
	const float3 kS = F_SchlickRoughness(F0, NoV, gbuffer.linearRoughness);
    const float3 kD = lerp(float3(1.0, 1.0, 1.0) - kS, float3(0.0, 0.0, 0.0), gbuffer.metallness);

	half3 envAmbientDiffuseLighting = half3(kD * irradiance) * gbuffer.diffuseColor * gbuffer.ao;

	const float3 reflectionVec = 2 * NdotV * gbuffer.normal - view;
	float3 roughR = getRoughReflectionVec(reflectionVec, gbuffer.normal, ggx_alpha);

    half3 enviBRDF = EnvBRDFApprox(specularColor, gbuffer.roughness, NoV);
	half3 envAmbientReflection = getSkyReflection(gbuffer.linearRoughness, roughR, NoV) * enviBRDF * gbuffer.ao;

	half3 result = standardBRDF( NoV, NoL, gbuffer.diffuseColor, ggx_alpha, gbuffer.linearRoughness, specularColor, gbuffer.extracted_albedo_ao, lightDir, view, gbuffer.normal) * shadowTerm * lightColor
		+ envAmbientDiffuseLighting + envAmbientReflection + gbuffer.emissionColor;

    return float4(float3(result), 1.0f);
}
