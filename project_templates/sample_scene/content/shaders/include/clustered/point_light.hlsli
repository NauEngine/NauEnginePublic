#include "shader_defines.h"
#include "shader_global.hlsli"
#include "pbr.hlsli"
#include "ColorSpaceUtility.hlsl"
#include "shader_global.hlsli"
#include "atmosphere/functions.hlsli"
#include "punctualLightsMath.hlsli"

#define TEX_ID_MULTIPLIER (1 << 5)
#define MAX_OMNI_LIGHTS 256
#define MAX_SPOT_LIGHTS 256

#define DYNAMIC_LIGHTS_SSS 1
#define LAMBERT_LIGHT 1
#define DYNAMIC_LIGHTS_EARLY_EXIT 1

struct RenderOmniLight
{
    float4 posRadius;
    float4 colorFlags;
    float4 direction__tex_scale;
    float4 boxR0;
    float4 boxR1;
    float4 boxR2;
    float4 posRelToOrigin_cullRadius;
};

struct RenderSpotLight
{
  float4 lightPosRadius;
  float4 lightColorAngleScale; //AngleScale sign bit contains contact_shadow bit
  float4 lightDirectionAngleOffset;
  float4 texId_scale;
};

GLOBAL_CBUFFER(LightConstBuffer) : register(b0)
{
    float4x4 mvp;
    float4x4 globtm_inv;
    float4 screen_pos_to_texcoord; 
    float4 world_view_pos;
};


/* TODO: support Photometry
Texture2DArray photometry_textures_tex : register(t4);
SamplerState photometry_textures_tex_samplerstate : register(s4);
*/


#ifndef M_PI
#define M_PI (3.14159265358979323846)
#endif

/* TODO: support Photometry
half2 getPhotometryTexCoords(half3 lightDir, half3 dir, float zoom, bool rotate)
{
    half3 side = abs(lightDir.x) < 0.707106781186548
        ? half3(1, 0, 0)
        : half3(0, 0, 1);
    half3 forward = normalize(cross(side, lightDir));
    side = cross(lightDir, forward);
#if USE_OCTAHEDRAL_MAPPING
      half2 tc = octahedral_mapping(half3(dot(side, dir), dot(lightDir, dir), dot(forward, dir)), zoom, rotate);
#else
    half phi = atan2(dot(forward, dir), dot(side, dir));
    half2 tc = half2(phi / (2 * M_PI), acosFast4(dot(lightDir, dir)) / M_PI * zoom);
#endif
    return tc;
}

float getPhotometryValue(float3 light_dir, float3 dir, float zoom, bool rotated, float texId)
{
    half2 tc = getPhotometryTexCoords(light_dir, dir, zoom, rotated);
      BRANCH
    if (any(or(tc < 0, tc > 1)))
        return 0;
    float3 photometry = tex3Dlod(
        photometry_textures_tex,
        half4(tc, texId, 0)).rgb;
    return SRGBToLinear_Fast(photometry.r).r;
}
*/

half applyPhotometryIntensity(float3 lightToWorldDir, float3 lightDir, float texId, float texScale_rotation, float currentAttenuation)
{
      return currentAttenuation;
//      ##if photometry_textures_tex != NULL
//      ##if photometry_textures_tex != NULL
    //bool rotated = texScale_rotation < 0;
    //float zoom = abs(texScale_rotation);
    //  BRANCH
    //if (texId < 0 || currentAttenuation < 0.0001)
    //    return currentAttenuation;
    //return getPhotometryValue(lightDir, lightToWorldDir, zoom, rotated, texId) * currentAttenuation;
//      ##else
}


// from http://advances.realtimerendering.com/other/2016/naughty_dog/index.html
//  http://advances.realtimerendering.com/other/2016/naughty_dog/NaughtyDog_TechArt_Final.pdf
half calc_micro_shadow(half NoL, half AO)
{
    return (half) saturate(abs(NoL) + half(2.0) * pow2(AO) - half(1.0));
}

float3x3 axis_matrix(float3 right, float3 up, float3 forward)
{
    float3 xaxis = right;
    float3 yaxis = up;
    float3 zaxis = forward;
    return float3x3(
        xaxis.x, yaxis.x, zaxis.x, 
        xaxis.y, yaxis.y, zaxis.y, 
        xaxis.z, yaxis.z, zaxis.z
    );
}

float3 tangent_to_world(float3 vec, float3 tangentZ)
{
    float3 up = abs(tangentZ.z) < 0.999 ? float3(0, 0, 1) : float3(1, 0, 0);
    float3 tangentX = normalize(cross(up, tangentZ));
    float3 tangentY = cross(tangentZ, tangentX);
    return tangentX * vec.x + tangentY * vec.y + tangentZ * vec.z;
}

float2 screen_pos_to_tc(float2 screen_pos)
{
    return screen_pos * screen_pos_to_texcoord.xy + screen_pos_to_texcoord.zw;
}
