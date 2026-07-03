// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "ColorSpaceUtility.hlsl"
#include "clustered/point_light.hlsli"

struct VsOutput
{
    VS_OUT_POSITION(pos)
    nointerpolation float3 id_texId_scale : TEXCOORD0;
    float4 pos_and_radius : TEXCOORD1;
    float4 color_and_attenuation : TEXCOORD2;
    float4 dir_angle : TEXCOORD3;
};

struct VsInput
{
    float3 pos : POSITION;
};

SYSTEM_CBUFFER(Lights) : register(b1)
{
    uint4 spot_lights_count;
    RenderSpotLight spot_lights_cb[MAX_SPOT_LIGHTS];
}

VsOutput deferred_lights_vs(VsInput input, uint spot_light_index : SV_InstanceID)
{
    VsOutput output;
    RenderSpotLight sl = spot_lights_cb[spot_light_index];
    float4 pos_and_radius = sl.lightPosRadius;
    float4 color_and_attenuation = sl.lightColorAngleScale;
    color_and_attenuation.w = abs(color_and_attenuation.w);
    float4 dir_angle = sl.lightDirectionAngleOffset;
    const float lightAngleScale = color_and_attenuation.a;
    const float lightAngleOffset = dir_angle.a;
    float2 texId_scale = sl.texId_scale.xy;
    float cosOuter = -lightAngleOffset / lightAngleScale;
    float halfTan = sqrt(1 / (cosOuter * cosOuter) - 1);
    float3 ofs;
    if (dot(input.pos, input.pos) > 0)
    {
        ofs = tangent_to_world(normalize(float3(input.pos.xy * halfTan, input.pos.z)), dir_angle.xyz);
    }
    else
    {
        ofs = float3(0, 0, 0);
    }

    float4 worldPos = float4(pos_and_radius.xyz + ofs.xyz * (pos_and_radius.w / cosOuter), 1);
    output.id_texId_scale = float3(spot_light_index, texId_scale);
    output.pos = mul(mvp, worldPos);
    output.color_and_attenuation = color_and_attenuation;
    output.pos_and_radius = pos_and_radius;
    output.dir_angle = dir_angle;
    return output;
}
// ##endif


#include "gbuffer_read.hlsli"

float readGbufferDepth(float2 tc)
{
    return depth_gbuf.SampleLevel(default_sampler, float4(tc, 0, 0).xy, float4(tc, 0, 0).w).r;
}


float4 deferred_lights_ps(VsOutput input HW_USE_SCREEN_POS) :
    SV_Target
{
    float4 screenpos = GET_SCREEN_POS(input.pos);
    half3 result;
    float3 view;
    float2 tc;
    float dist;
    bool is_calculated = false;
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
        if (shouldExit)
        {
            is_calculated = false;  // discard; //discard is faster, but also fails early depth
        }
        else
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

            float4 lightPosRadius = input.pos_and_radius;
            float4 lightColor = input.color_and_attenuation;
            float4 lightDirection = input.dir_angle;
            uint spot_light_index = input.id_texId_scale.x;
            float2 texId_scale = input.id_texId_scale.yz;
#define EXIT_STATEMENT return false

            float lightAngleScale = lightColor.a;
            float lightAngleOffset = lightDirection.a;

            half geomAttenuation;
            float3 dirFromLight, point2light;  // point2light - not normalized
            spot_light_params(worldPos.xyz, lightPosRadius, lightDirection.xyz, lightAngleScale, lightAngleOffset, geomAttenuation, dirFromLight, point2light);

            float NoL = dot(gbuffer.normal, dirFromLight);
            half attenuation = calc_micro_shadow(NoL, gbuffer.ao) * geomAttenuation;
            float ggx_alpha = max(1e-4, gbuffer.linearRoughness * gbuffer.linearRoughness);

#if DYNAMIC_LIGHTS_EARLY_EXIT
    #if DYNAMIC_LIGHTS_SSS
            bool shouldExit = attenuation <= 0;
    // bool shouldExit = attenuation <= 0 || (!isSubSurfaceShader(gbuffer.material) && NoL <= 0);
    #else
            attenuation = saturate(attenuation * NoL);
            bool shouldExit = attenuation == 0;
    #endif
    #if WAVE_INTRINSICS
            shouldExit = (bool)WaveReadFirstLane(WaveAllBitAnd(uint(shouldExit)));
    #endif
            BRANCH
            if (shouldExit)
            {
                is_calculated = false;
            }
            else
            {
#else
    #if !DYNAMIC_LIGHTS_SSS
            attenuation = saturate(attenuation * NoL);
    #endif
#endif

                half spotShadow = 1;
#if SPOT_SHADOWS || defined(SPOT_CONTACT_SHADOWS_CALC)
                float zbias = shadowZBias + shadowSlopeZBias / (abs(NoL) + 0.1);
                float4x4 spotLightTm = getSpotLightTm(spot_light_index);
                float4 lightShadowTC = mul(spotLightTm, float4(worldPos.xyz + (point2light + dirFromLight) * zbias, 1));
                if (lightShadowTC.w > 1e-6)
                {
                    lightShadowTC.xyz /= lightShadowTC.w;
    #if SPOT_SHADOWS
        #ifdef SIMPLE_PCF_SHADOW
                    spotShadow = 1 - dynamic_shadow_sample(lightShadowTC.xy, lightShadowTC.z);
        #else
            #ifndef shadow_frame
                    float shadow_frame = 0;
            #endif
                    spotShadow = 1 - dynamic_shadow_sample_8tap(screenpos, lightShadowTC.xy, lightShadowTC.z, 1.5 * shadowAtlasTexel.x * (0.75 + saturate(0.3 * length(point2light))), shadow_frame);
        #endif
    #endif
    #ifdef SPOT_CONTACT_SHADOWS_CALC
                    SPOT_CONTACT_SHADOWS_CALC
    #endif
                }
                attenuation *= spotShadow;
#endif

#if DYNAMIC_LIGHTS_SSS
                NoL = saturate(NoL);
                // half3 lightBRDF = standardBRDF(NoV, NoL, gbuffer.diffuseColor, ggx_alpha, gbuffer.linearRoughness, specularColor, dynamicLightsSpecularStrength, dirFromLight, view, gbuffer.normal, gbuffer.translucencyColor, gbuffer.translucency);
                half3 lightBRDF = diffuseLambert(gbuffer.diffuseColor) * (NoL)*lightColor.xyz;
                ;

    #if USE_SSSS && SPOT_SHADOWS
                BRANCH if (gbuffer.material == SHADING_SUBSURFACE)
                {
                    SpotlightShadowDescriptor spotlightDesc = spot_lights_ssss_shadow_desc[spot_light_index];
                    BRANCH if (lightShadowTC.w > 1e-6 && spotlightDesc.hasDynamic)
                    {
                        float4 ssssShadowTC = mul(spotLightTm, float4(ssssWorldPos, 1));
                        ssssShadowTC /= ssssShadowTC.w;
                        ShadowDescriptor desc;
                        desc.decodeDepth = spotlightDesc.decodeDepth;
                        desc.meterToUvAtZfar = spotlightDesc.meterToUvAtZfar;
                        desc.uvMinMax = spotlightDesc.uvMinMax;
                        desc.shadowRange = lightPosRadius.w;
                        float ssssTransmittance = ssss_get_transmittance_factor(
                            gbuffer.translucency, tc, dynamic_light_shadows_smp, dynamic_light_shadows_size, ssssShadowTC.xyz, desc);
                        result += gbuffer.diffuseColor * lightColor.rgb * ssss_get_profiled_transmittance(gbuffer.normal, dirFromLight, ssssTransmittance) * geomAttenuation;
                    }
                }
                else
    #endif
                {
                    /*
                    BRANCH if (isSubSurfaceShader(gbuffer.material))
                        lightBRDF += (foliageSSS(NoL, view, dirFromLight) * gbuffer.ao) * gbuffer.translucencyColor;  // can make gbuffer.ao*gbuffer.translucencyColor only once for all lights
                    */
                }
#else
            half3 lightBRDF = standardBRDF_NO_NOL(NoV, NoL, gbuffer.diffuseColor, ggx_alpha, gbuffer.linearRoughness, specularColor, dynamicLightsSpecularStrength, dirFromLight, view, gbuffer.normal);
#endif
                attenuation = applyPhotometryIntensity(-dirFromLight, lightDirection.xyz, texId_scale.x,
                                                       texId_scale.y, attenuation);
                lightBRDF *= attenuation * lightColor.xyz;
#if WAVE_INTRINSICS || !DYNAMIC_LIGHTS_EARLY_EXIT
                FLATTEN
                if (attenuation <= 0)
                    lightBRDF = 0;
#endif

                result = lightBRDF * pointLightsFinalAO;
                is_calculated = true;
            }
        }
    }
    if (is_calculated)
    {
        return float4(result, 1);
    }

    return float4(0, 0, 0, 0);
}