// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "shader_defines.h"

bool isEmissiveShader(float material)
{
    return material == SHADING_EMISSIVE;
}

struct ProcessedGbuffer
{
    half3 diffuseColor;
    half3 specularColor;
    half3 translucencyColor;
    half roughness, linearRoughness;
    float3 normal;

    half metallness;
    half translucency;  // either translucent or metallic

    half extracted_albedo_ao;  // custom
    half ao;                   // custom
    half shadow;
    half3 emissionColor;
    half emission_part;
    float material;
};

struct UnpackedGbuffer
{
    half3 albedo;
    half smoothness;
    float3 normal;

    half metallness;     // either translucent or metallic or emission
    half translucency;   // either translucent or metallic or emission
    half emission_part;  // either translucent or metallic or emission

    half ao;                 // either ao, or emission
    half emission_strength;  // either ao, or emission
    half shadow;
    float material;
    float outline;
};

struct PackedGbuffer
{
    half4 albedo_ao;
    float4 normal_smoothness_material;
    half2 metallTranslucency_shadow;  // processed
};
// Spheremap Transform
float2 encodeNormal(float3 n)
{
    half2 enc = normalize(n.xy) * (sqrt(-n.z*0.5+0.5));
    enc = enc*0.5+0.5;
    return enc;
}
float3 decodeNormal(float2 enc)
{
    float4 nn = float4(enc.xy,0,0)*float4(2,2,0,0) + float4(-1,-1,1,-1);
    half l = dot(nn.xyz,-nn.xyw);
    nn.z = l;
    nn.xy *= sqrt(l);
    return (nn.xyz * 2 + half3(0,0,-1));
}
half luminance(half3 col)
{
    return dot(col, half3(0.299, 0.587, 0.114));
}
half decode_albedo_ao(half3 albedo)
{
    return saturate(luminance(albedo) * (1 / 0.04)) * 0.9 + 0.1;  // anything darker than charcoal is not physical possible, and is shadow
}
PackedGbuffer pack_gbuffer(UnpackedGbuffer gbuffer)
{
    PackedGbuffer gbuf;
    half metallnessOrTranslucency = gbuffer.metallness;
    metallnessOrTranslucency = isEmissiveShader(gbuffer.material) ? gbuffer.emission_part : metallnessOrTranslucency;

    float3 normal_smoothness = float3(encodeNormal(gbuffer.normal.xyz), max(gbuffer.smoothness, 1.0 / 127));
    half material = gbuffer.material * (1.f / 3.0);
    gbuf.normal_smoothness_material = float4(normal_smoothness, material);
    gbuf.albedo_ao = half4(gbuffer.albedo, isEmissiveShader(gbuffer.material) ? gbuffer.emission_strength * (1.0f / MAX_EMISSION) : gbuffer.ao);
    // gbuf.albedo_ao = half4(pow(gbuffer.albedo, 1/2.2), gbuffer.ao);
    // gbuf.metallTranslucency_shadow = (floor(metallnessOrTranslucency*15)*16+floor(gbuffer.shadow*15))*(1.0/255.0);
    gbuf.metallTranslucency_shadow = float2(metallnessOrTranslucency, gbuffer.shadow);
    return gbuf;
}

void unpackNormalSmoothness(float3 normal_smoothness, out float3 normal, out half smoothness)
{
    normal = decodeNormal(normal_smoothness.xy);
    smoothness = abs(normal_smoothness.z);
}

void unpackGbufferNormalSmoothness(PackedGbuffer gbuf, out float3 normal, out half smoothness)
{
    unpackNormalSmoothness(gbuf.normal_smoothness_material.xyz, normal, smoothness);
}

UnpackedGbuffer unpackGbuffer(PackedGbuffer gbuf)
{
    UnpackedGbuffer gbuffer;

    gbuffer.material = floor(gbuf.normal_smoothness_material.w * 3.f);
    // half metallTranslucency_shadow = gbuf.metallTranslucency_shadow.x*(255.0/16.0);
    // half shadow = frac(metallTranslucency_shadow)*(16.0/15.0);
    // half metallnessOrTranslucency = floor(metallTranslucency_shadow)*(1./15);
    half shadow = gbuf.metallTranslucency_shadow.y;
    half metallnessOrTranslucency = gbuf.metallTranslucency_shadow.x;
    gbuffer.albedo = gbuf.albedo_ao.xyz;
    unpackGbufferNormalSmoothness(gbuf, gbuffer.normal, gbuffer.smoothness);

    gbuffer.emission_part = isEmissiveShader(gbuffer.material) ? metallnessOrTranslucency : 0;
    gbuffer.metallness = metallnessOrTranslucency;
    gbuffer.translucency = 0;
    gbuffer.ao = isEmissiveShader(gbuffer.material) ? 1 : gbuf.albedo_ao.w;
    gbuffer.emission_strength = isEmissiveShader(gbuffer.material) ? gbuf.albedo_ao.w * MAX_EMISSION : 0;
    gbuffer.shadow = shadow;
    // gbuffer.diffuseColor = albedo*(1-gbuffer.metallness);
    // half fresnel0Dielectric = 0.04f;//lerp(0.16f,0.01f, smoothness);//sqr((1.0 - refractiveIndex)/(1.0 + refractiveIndex)) for dielectrics;
    // gbuffer.specularColor = lerp(half3(fresnel0Dielectric, fresnel0Dielectric, fresnel0Dielectric), albedo, gbuffer.metallness);
    return gbuffer;
}
ProcessedGbuffer processGbuffer(UnpackedGbuffer gbuf)
{
    ProcessedGbuffer gbuffer;
    gbuffer.material = gbuf.material;
    gbuffer.normal = gbuf.normal;
    gbuffer.linearRoughness = 1 - gbuf.smoothness;
    gbuffer.roughness = max(1e-4, gbuffer.linearRoughness * gbuffer.linearRoughness);
    gbuffer.metallness = gbuf.metallness;
    gbuffer.translucency = gbuf.translucency;  // due to 2 bit encoding *0.75 is correct
    gbuffer.emissionColor = gbuf.emission_strength * gbuf.albedo;
    gbuffer.emission_part = gbuf.emission_part;
    gbuffer.extracted_albedo_ao = decode_albedo_ao(gbuf.albedo);
    gbuffer.diffuseColor = gbuf.albedo - gbuffer.metallness * gbuf.albedo;  //*(1-met)
    gbuffer.shadow = gbuf.shadow;
    gbuffer.translucencyColor = gbuffer.diffuseColor * gbuffer.translucency;

    half fresnel0Dielectric = 0.04f;  // + (gbuf.material == SHADING_NORMAL ? 0.2 * (1-gbuf.shadow) : 0);//lerp(0.16f,0.01f, roughness);//sqr((1.0 - refractiveIndex)/(1.0 + refractiveIndex)) for dielectrics;
    fresnel0Dielectric = lerp(fresnel0Dielectric, 0.01f, gbuffer.roughness * gbuffer.translucency);
    fresnel0Dielectric *= (1 - gbuffer.translucency);
    gbuffer.specularColor = lerp(half3(fresnel0Dielectric, fresnel0Dielectric, fresnel0Dielectric), gbuf.albedo, gbuffer.metallness);
    gbuffer.ao = gbuf.ao;
    return gbuffer;
}

void init_gbuffer(out UnpackedGbuffer result)
{
    result.albedo = result.normal = 0;
    result.smoothness = result.metallness = result.translucency = 0;
    result.emission_part = result.emission_strength = 0;
    result.ao = result.shadow = 1;
    result.material = SHADING_NORMAL;
}
void init_albedo(inout UnpackedGbuffer result, half3 albedo)
{
    result.albedo.xyz = albedo;
}
void init_smoothness(inout UnpackedGbuffer result, half smoothness)
{
    result.smoothness = smoothness;
}

void init_normal(inout UnpackedGbuffer result, float3 norm)
{
    result.normal = norm;
}

void init_metalness(inout UnpackedGbuffer result, half metal)
{
    result.metallness = metal;
}
void init_translucency(inout UnpackedGbuffer result, half translucency)
{
    result.translucency = translucency;
}

void init_ao(inout UnpackedGbuffer result, half ao)
{
    result.ao = ao;
}
void init_shadow(inout UnpackedGbuffer result, half shadow)
{
    result.shadow = shadow;
}
void init_material(inout UnpackedGbuffer result, float material)
{
    result.material = material;
}
void init_emission(inout UnpackedGbuffer result, float emission_strength)
{
    result.emission_strength = emission_strength;
}
void init_emission_part(inout UnpackedGbuffer result, float emission_part)
{
    result.emission_part = emission_part;
}

struct GBUFFER_OUTPUT
{
    half4 albedo_ao : SV_Target0;
    float4 normal_smoothness_material : SV_Target1;
    half4 metallTranslucency_shadow : SV_Target2;
};

GBUFFER_OUTPUT write_gbuffer(PackedGbuffer gbuf)
{
    GBUFFER_OUTPUT gbufOut;
    gbufOut.albedo_ao = gbuf.albedo_ao;
    gbufOut.normal_smoothness_material = gbuf.normal_smoothness_material;
    gbufOut.metallTranslucency_shadow = half4(gbuf.metallTranslucency_shadow,0,0);
    return gbufOut;
}

GBUFFER_OUTPUT encode_gbuffer_raw(UnpackedGbuffer gbuffer)
{
    return write_gbuffer(pack_gbuffer(gbuffer));
}

#define encode_gbuffer(a,b) encode_gbuffer_raw(a)

half3 perturb_normal(half3 localNorm, half3 N, float3 p, float2 uv)
{
    // get edge vectors of the pixel triangle
    float3 dp1 = ddx(p);
    float3 dp2 = ddy(p);
    float2 duv1 = ddx(uv);
    float2 duv2 = ddy(uv);

    // solve the linear system
    float3 dp2perp = cross(N, dp2);
    float3 dp1perp = cross(dp1, N);
    float3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    float3 B = dp2perp * duv1.y + dp1perp * duv2.y;

    // construct a scale-invariant frame
    float invmax = rsqrt(max(dot(T, T), dot(B, B)));
    return half3(localNorm.z * N + (localNorm.x * invmax) * T + (localNorm.y * invmax) * B);
}