// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "clustered/point_light.hlsli"

struct VsInput
{
    float3 pos : POSITION;
};

struct VsOutput
{
    VS_OUT_POSITION(pos)
    float4 pos_and_radius : TEXCOORD1;
    float4 color_and_attenuation : TEXCOORD2;
    float3 worldPos : TEXCOORD3;
    float3 worldPos2 : TEXCOORD4;
#if OMNI_SHADOWS
    float4 shadow_tc_to_atlas : TEXCOORD3;
#endif
};

SYSTEM_CBUFFER(Lights) :
    register(b1)
{
    uint4 omni_lights_count;
    RenderOmniLight omni_lights_cb[MAX_OMNI_LIGHTS];
};


#include "gbuffer_read.hlsli"

float readGbufferDepth(float2 tc)
{
    return depth_gbuf.SampleLevel(default_sampler, float4(tc, 0, 0).xy, float4(tc, 0, 0).w).r;
}

half3 perform_point_light(float3 worldPos, float3 view, float NoV, ProcessedGbuffer gbuffer, half3 specularColor, half dynamicLightsSpecularStrength, half ao, float4 pos_and_radius, float4 color_and_attenuation, float4 shadowTcToAtlas, float2 screenpos)
{
#if DYNAMIC_LIGHTS_EARLY_EXIT
    #define EXIT_STATEMENT return 0
#endif

    float3 point2light = pos_and_radius.xyz - worldPos.xyz;
    float distSqFromLight = dot(point2light, point2light);
    float ggx_alpha = max(1e-4, pow2(gbuffer.linearRoughness));

#if LAMBERT_LIGHT
    float radius2 = pow2(pos_and_radius.w);
    #if DYNAMIC_LIGHTS_EARLY_EXIT
    bool shouldExit = distSqFromLight >= radius2;
        #if WAVE_INTRINSICS
    shouldExit = (bool)WaveReadFirstLane(WaveAllBitAnd(uint(shouldExit)));
        #endif
    BRANCH
    if (shouldExit)
        EXIT_STATEMENT;
    #endif
    float invSqrRad = rcp(radius2);
    float attenuation = smoothDistanceAtt(distSqFromLight, invSqrRad) * color_and_attenuation.w;

    float3 lightDir = point2light * rsqrt(0.0000001 + distSqFromLight);
    float NoL = saturate(dot(gbuffer.normal, lightDir));
    half shadowTerm = attenuation;  // no shadows
    shadowTerm *= calc_micro_shadow(NoL, ao);

    #if OMNI_SHADOWS
    shadowTerm *= getOmniShadow(shadowTcToAtlas, pos_and_radius, worldPos, NoL, screenpos);
    #endif

    #if !DYNAMIC_LIGHTS_SPECULAR
    half3 lightBRDF = diffuseLambert(gbuffer.diffuseColor) * (NoL * shadowTerm) * color_and_attenuation.xyz;
    #else
    half3 diffuse = diffuseLambert(gbuffer.diffuseColor);

    float3 H = normalize(view + lightDir);
    float NoH = saturate(dot(gbuffer.normal, H));
    float VoH = saturate(dot(view, H));
    float D = BRDF_distribution(ggx_alpha, NoH) * dynamicLightsSpecularStrength;
    float G = NoL > 0 ? BRDF_geometricVisibility(ggx_alpha, NoV, NoL, VoH) : 0;
    float3 F = BRDF_fresnel(specularColor, VoH);
    half3 result = (diffuse + (D * G) * F) * NoL;
        #if DYNAMIC_LIGHTS_SSS
    if (isSubSurfaceShader(gbuffer.material))
        result += (foliageSSS(NoL, view, lightDir) * gbuffer.ao) * gbuffer.translucencyColor;  // can make gbuffer.ao*gbuffer.translucencyColor only once for all lights
        #endif
    half3 lightBRDF = result * shadowTerm * color_and_attenuation.xyz;
    #endif
#else
    float NoL = dot(gbuffer.normal, point2light);
    float invSqrRad = rcp(pow2(pos_and_radius.w));
    float attenuation = getDistanceAtt(distSqFromLight, invSqrRad) * color_and_attenuation.w;
    float rcpDistFromLight = rsqrt(0.0000001 + distSqFromLight);
    NoL *= rcpDistFromLight;
    attenuation *= calc_micro_shadow(NoL, ao);

    #if DYNAMIC_LIGHTS_EARLY_EXIT
    bool shouldExit = min(attenuation, NoL) <= 0;
        #if WAVE_INTRINSICS
    shouldExit = (bool)WaveReadFirstLane(WaveAllBitAnd(uint(shouldExit)));
        #endif
    BRANCH
    if (shouldExit)
        EXIT_STATEMENT;
    #endif

    #if OMNI_SHADOWS
    attenuation *= getOmniShadow(shadowTcToAtlas, pos_and_radius, worldPos, NoL, screenpos);
    #endif

    float3 lightDir = point2light * rcpDistFromLight;
    half3 result = standardBRDF(NoV, NoL, gbuffer.diffuseColor, ggx_alpha, gbuffer.linearRoughness, specularColor, dynamicLightsSpecularStrength, lightDir, view, gbuffer.normal, gbuffer.translucencyColor, gbuffer.translucency);
    #if !DYNAMIC_LIGHTS_EARLY_EXIT
    result = NoL > 0 ? result : 0;
    #endif
    #if DYNAMIC_LIGHTS_SSS
    if (isSubSurfaceShader(gbuffer.material))
        result += (foliageSSS(NoL, view, lightDir) * gbuffer.ao) * gbuffer.translucencyColor;  // can make gbuffer.ao*gbuffer.translucencyColor only once for all lights
    #endif
    half3 lightBRDF = result * attenuation * color_and_attenuation.xyz;
#endif

    return lightBRDF;
#if DYNAMIC_LIGHTS_EARLY_EXIT
    #undef EXIT_STATEMENT
#endif
}

half getOmniLightFade(RenderOmniLight ol, float3 worldPos)
{
    float3 boxPos = half3(ol.boxR0.w, ol.boxR1.w, ol.boxR2.w);
    float3 boxDiff = worldPos - boxPos;
    float3 box = 2 * (ol.boxR0.xyz * boxDiff.x + ol.boxR1.xyz * boxDiff.y + ol.boxR2.xyz * boxDiff.z);
    box = saturate(abs(box));
    const float FADEOUT_DIST = 0.05;
    box = 1 - box;
    float fadeout = min3(box.x, box.y, box.z);
    float fadelimit = FADEOUT_DIST;
    return fadeout <= fadelimit ? fadeout / fadelimit : 1;
}

#include "ColorSpaceUtility.hlsl"
#include "fast_shader_trig.hlsli"

float3 inv_octahedral_mapping(float2 tc, float zoom, bool rotate)
{
    tc = (tc * 2 - 1) / zoom;
    if (rotate)
        tc = float2(tc.x - tc.y, tc.x + tc.y) / 2;
    float3 dir = float3(tc.xy, 1.0 - (abs(tc.x) + abs(tc.y)));
    if (dir.z < 0)
        dir.xy = float2(-(abs(dir.y) - 1) * sign(dir.x), -(abs(dir.x) - 1) * sign(dir.y));
    return normalize(dir);
}

half2 octahedral_mapping(half3 co, float zoom, bool rotate)
{
    co /= dot(half3(1, 1, 1), abs(co));

#if SHADER_COMPILER_HLSL2021
    co.xy = co.y < 0.0 ? (1.0 - abs(co.zx)) * (select(co.xz < 0, float2(-1, -1), float2(1, 1))) : co.xz;
#else
    co.xy = co.y < 0.0 ? (1.0 - abs(co.zx)) * (co.xz < 0 ? float2(-1, -1) : float2(1, 1)) : co.xz;
#endif

    if (rotate)
    {
        float tempX = co.x;
        co.x = (co.x + co.y);
        co.y = (co.y - tempX);
    }
    co.x *= zoom;
    co.y *= zoom;
    return co.xy * 0.5 + 0.5;
}

half getOmniLightIntensity(RenderOmniLight ol, float3 worldPos)
{
    return 1;
}

float4 getFinalColor(RenderOmniLight ol, float3 worldPos)
{
    return ol.colorFlags * getOmniLightFade(ol, worldPos) * getOmniLightIntensity(ol, worldPos);
}

VsOutput deferred_lights_vs(VsInput input, uint omni_light_index : SV_InstanceID)
{
    VsOutput output;
    RenderOmniLight ol = omni_lights_cb[omni_light_index];
    float4 pos_and_radius = ol.posRadius;
    float3 worldPos = pos_and_radius.xyz + input.pos.xyz * pos_and_radius.w * 1.15;
    float4 color_and_attenuation = getFinalColor(ol, worldPos);
    output.pos = mul(mvp, float4(worldPos, 1));
    output.color_and_attenuation = color_and_attenuation;
    output.pos_and_radius = pos_and_radius;
    output.worldPos = worldPos;
    output.worldPos2 = worldPos - world_view_pos.xyz;
#if OMNI_SHADOWS
    output.shadow_tc_to_atlas = getOmniLightShadowData(index);
#endif
#if LIGHT_LIMIT_SIZE
    output.pos_and_radius.w = min(output.pos_and_radius.w, LIGHT_LIMIT_SIZE);
#endif
    return output;
}



float4 deferred_lights_ps(VsOutput input) :
    SV_Target
{
    float4 screenpos = GET_SCREEN_POS(input.pos);
    half3 result;
    float3 view;
    float2 tc;
    float dist, w;
    {
        tc = screen_pos_to_tc(screenpos.xy);

        const float rawDepth = depth_gbuf.SampleLevel(default_sampler, tc, 0).x;
        float4 farpos = float4(tc.x * 2 - 1, (1 - tc.y) * 2 - 1, rawDepth, 1.0);
        float4 worldpos_prj = mul(globtm_inv, farpos);
        float4 worldPos = worldpos_prj / worldpos_prj.w;

        float3 pointToEye = worldPos.xyz - world_view_pos.xyz;

        float4 pos_and_radius = input.pos_and_radius;
#if OMNI_SHADOWS
        float4 shadowTcToAtlas = input.shadow_tc_to_atlas;
#else
        float4 shadowTcToAtlas = float4(0, 0, 0, 0);
#endif
        float3 moveFromPos = pos_and_radius.xyz - worldPos.xyz;
        view = 0;
        dist = 0;

        bool shouldExit = dot(moveFromPos, moveFromPos) > pos_and_radius.w * pos_and_radius.w;
#if WAVE_INTRINSICS
        shouldExit = (bool)WaveReadFirstLane(WaveAllBitAnd(uint(shouldExit)));
#endif
        BRANCH
        if (!shouldExit)
        {
            ProcessedGbuffer gbuffer = readProcessedGbuffer(tc);

            float distSq = dot(pointToEye, pointToEye);
            float invRsqrt = rsqrt(distSq);
            view = pointToEye * invRsqrt;
            dist = rcp(invRsqrt);
            float NdotV = dot(gbuffer.normal, view);
            float3 reflectionVec = 2 * NdotV * gbuffer.normal - view;
            float NoV = abs(NdotV) + 1e-5;

            half dynamicLightsSpecularStrength = gbuffer.extracted_albedo_ao;
            half ssao = 1;                    // fixme: we should use SSAO here!
            half enviAO = gbuffer.ao * ssao;  // we still modulate by albedo color, so we don't need micro AO
            half pointLightsFinalAO = (enviAO * 0.5 + 0.5);
            half specularAOcclusion = computeSpecOcclusion(saturate(NdotV), enviAO, gbuffer.linearRoughness * gbuffer.linearRoughness);  // dice spec occlusion
            half3 specularColor = gbuffer.specularColor * (specularAOcclusion * gbuffer.extracted_albedo_ao);

            float4 color_and_attenuation = input.color_and_attenuation;
            result = perform_point_light(worldPos.xyz, view, NoV, gbuffer, gbuffer.specularColor, dynamicLightsSpecularStrength, gbuffer.ao, pos_and_radius, color_and_attenuation, shadowTcToAtlas, screenpos.xy);
            result *= pointLightsFinalAO;

            return float4(result, 1);
        }
    }

    return float4(0, 0, 0, 0);
}