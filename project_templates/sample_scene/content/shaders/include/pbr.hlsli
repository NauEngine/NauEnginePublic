// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#ifndef __BRDF_COMMON__
#define __BRDF_COMMON__

#include <diffuse_brdf.hlsli>
#include <specular_brdf.hlsli>
#include <envi_brdf.hlsli>

// Diffuse model
#define DIFFUSE_LAMBERT 0
#define DIFFUSE_OREN_NAYAR 1
#define DIFFUSE_BURLEY 2
#define DIFFUSE_BURLEY_FIXED 3
#define DIFFUSE_CHAN 4

#ifndef BRDF_DIFFUSE
#define BRDF_DIFFUSE DIFFUSE_BURLEY_FIXED//DIFFUSE_OREN_NAYAR//DIFFUSE_BURLEY//DIFFUSE_OREN_NAYAR//DIFFUSE_BURLEY//DIFFUSE_LAMBERT//
#endif

// Microfacet distribution function

#define SPEC_D_BLINN 0
#define SPEC_D_BECKMANN 1
#define SPEC_D_GGX 2

#ifndef BRDF_SPEC_D
#define BRDF_SPEC_D SPEC_D_GGX
#endif

// Geometric attenuation or shadowing
#define SPEC_G_IMPLICIT 0
#define SPEC_G_NEUMANN 1
#define SPEC_G_KELEMEN 2
#define SPEC_G_SHLICK 3
#define SPEC_G_SMITH_GGX 4
#define SPEC_G_SMITH_CORRELATED 5
#define SPEC_G_SMITH_CORRELATED_APPROX 6

#ifndef BRDF_SPEC_G
#define BRDF_SPEC_G SPEC_G_SMITH_CORRELATED
#endif

// Fresnel
#define SPEC_F_NONE 0
#define SPEC_F_SHLICK 1
#define SPEC_F_FRESNEL 2
#ifndef BRDF_SPEC_F
#define BRDF_SPEC_F SPEC_F_SHLICK
#endif

// Sheen
#ifndef SHEEN_SPECULAR
#define SHEEN_SPECULAR 0
#endif

float3 BRDF_diffuse(float3 diffuseColor, float linearRoughness, float NoV, float NoL, float VoH)
{
#if   BRDF_DIFFUSE == DIFFUSE_LAMBERT
  return diffuseLambert( diffuseColor );
#elif BRDF_DIFFUSE == DIFFUSE_OREN_NAYAR
  return diffuseOrenNayar( diffuseColor, linearRoughness, NoV, NoL, VoH );
#elif BRDF_DIFFUSE == DIFFUSE_BURLEY
  return diffuseBurley( diffuseColor, linearRoughness, NoV, NoL, VoH );
#elif BRDF_DIFFUSE == DIFFUSE_BURLEY_FIXED
  return diffuseBurleyFixed( diffuseColor, linearRoughness, NoV, NoL, VoH );
#elif BRDF_DIFFUSE == DIFFUSE_CHAN
  //#error call different BRDF_diffuse, with NoH
  return 0;
#endif
}

float3 BRDF_diffuse(float3 diffuseColor, float linearRoughness, float NoV, float NoL, float VoH, float NoH)
{
#if   BRDF_DIFFUSE == DIFFUSE_LAMBERT
  return diffuseLambert( diffuseColor );
#elif BRDF_DIFFUSE == DIFFUSE_OREN_NAYAR
  return diffuseOrenNayar( diffuseColor, linearRoughness, NoV, NoL, VoH );
#elif BRDF_DIFFUSE == DIFFUSE_BURLEY
  return diffuseBurley( diffuseColor, linearRoughness, NoV, NoL, VoH );
#elif BRDF_DIFFUSE == DIFFUSE_BURLEY_FIXED
  return diffuseBurleyFixed( diffuseColor, linearRoughness, NoV, NoL, VoH );
#elif BRDF_DIFFUSE == DIFFUSE_CHAN
  return diffuseChan( diffuseColor, linearRoughness*linearRoughness, NoV, NoL, VoH, NoH);
#endif
}

float BRDF_distribution(float ggx_alpha, float NoH)
{
#if   BRDF_SPEC_D == SPEC_D_BLINN
  return distributionBlinn( ggx_alpha, NoH );
#elif BRDF_SPEC_D == SPEC_D_BECKMANN
  return distributionBeckmann( ggx_alpha, NoH );
#elif BRDF_SPEC_D == SPEC_D_GGX
  return distributionGGX( ggx_alpha, NoH );
#endif
}

// Vis = G / (4*NoL*NoV)
float BRDF_geometricVisibility(float ggx_alpha, float NoV, float NoL, float VoH)
{
#if   BRDF_SPEC_G == SPEC_G_IMPLICIT
  return geometryImplicit();
#elif BRDF_SPEC_G == SPEC_G_NEUMANN
  return geometryNeumann( NoV, NoL );
#elif BRDF_SPEC_G == SPEC_G_KELEMEN
  return geometryKelemen( VoH );
#elif BRDF_SPEC_G == SPEC_G_SHLICK
  return geometrySchlick( ggx_alpha, NoV, NoL );
#elif BRDF_SPEC_G == SPEC_G_SMITH_GGX
  return geometrySmith( ggx_alpha, NoV, NoL );
#elif BRDF_SPEC_G == SPEC_G_SMITH_CORRELATED
  return geometrySmithCorrelated( ggx_alpha, NoV, NoL );
#elif BRDF_SPEC_G == SPEC_G_SMITH_CORRELATED_APPROX
  return geometrySmithCorrelatedApprox( ggx_alpha, NoV, NoL );
#endif
}

float3 BRDF_fresnel(float3 specularColor, float VoH)
{
#if   BRDF_SPEC_F == 0
  return fresnelNone( specularColor );
#elif BRDF_SPEC_F == 1
  return fresnelSchlick( specularColor, VoH );
#elif BRDF_SPEC_F == 2
  return fresnelFresnel( specularColor, VoH );
#endif
}

float3 BRDF_specular(float ggx_alpha, float NoV, float NoL, float VoH, float NoH, half sheenStrength, half3 sheenColor)
{
    float D = BRDF_distribution(ggx_alpha, NoH);
    float G = BRDF_geometricVisibility(ggx_alpha, NoV, NoL, VoH);
    float3 result = D * G;

    return result;
}

//from http://www.frostbite.com/wp-content/uploads/2014/11/course_notes_moving_frostbite_to_pbr_v2.pdf (original version had bug in code!)
float computeSpecOcclusion(float saturated_NdotV, float AO, float ggx_alpha)
{
    return saturate(pow(saturated_NdotV + AO, exp2(-16.0 * ggx_alpha - 1.0)) - 1 + AO);
}
//ggx_alpha = linearRoughness*linearRoughness

// ggx_alpha = linearRoughness*linearRoughness
half3 standardBRDF_NO_NOL(float NoV, float NoL, half3 baseDiffuseColor, half ggx_alpha, half linearRoughness, half3 specularColor, half specularStrength, float3 lightDir, float3 view, half3 normal, float3 sheenColor, float translucency)
{
#if SPECULAR_DISABLED && BRDF_DIFFUSE == DIFFUSE_LAMBERT
    return diffuseLambert(baseDiffuseColor);
#else
    float3 H = normalize(view + lightDir);
    float NoH = saturate(dot(normal, H));
    float VoH = saturate(dot(view, H));
    half3 diffuse = BRDF_diffuse(baseDiffuseColor, linearRoughness, NoV, NoL, VoH);
#if !SPECULAR_DISABLED
    float3 specular = BRDF_specular(ggx_alpha, NoV, NoL, VoH, NoH, translucency, sheenColor) * specularStrength;
    float3 F = BRDF_fresnel(specularColor, VoH);
    return (diffuse + F * specular);
#else
    return diffuse;
#endif
#endif
}

half3 standardBRDF(float NoV, float NoL, half3 baseDiffuseColor, half ggx_alpha, half linearRoughness, half3 specularColor, half specularStrength, float3 lightDir, float3 view, half3 normal, float3 sheenColor, float translucency)
{
    return standardBRDF_NO_NOL(NoV, NoL, baseDiffuseColor, ggx_alpha, linearRoughness, specularColor, specularStrength, lightDir, view, normal, sheenColor, translucency) * NoL;
}

half3 standardBRDF_NO_NOL(float NoV, float NoL, half3 baseDiffuseColor, half ggx_alpha, half linearRoughness, half3 specularColor, half specularStrength, float3 lightDir, float3 view, half3 normal)
{
    return standardBRDF_NO_NOL(NoV, NoL, baseDiffuseColor, ggx_alpha, linearRoughness, specularColor, specularStrength, lightDir, view, normal, float3(0, 0, 0), 0);
}

half3 standardBRDF(float NoV, float NoL, half3 baseDiffuseColor, half ggx_alpha, half linearRoughness, half3 specularColor, half specularStrength, float3 lightDir, float3 view, half3 normal)
{
    return standardBRDF_NO_NOL(NoV, NoL, baseDiffuseColor, ggx_alpha, linearRoughness, specularColor, specularStrength, lightDir, view, normal, float3(0, 0, 0), 0) * NoL;
}

// //////////// TODO: NAU-1797 Unify code above with other parts (for example see distributionGGX in specular_brdf.hlsli)
uint ReverseBits32(uint bits)
{
    bits = (bits << 16) | (bits >> 16);
    bits = ((bits & 0x55555555) << 1) | ((bits & 0xAAAAAAAA) >> 1);
    bits = ((bits & 0x33333333) << 2) | ((bits & 0xCCCCCCCC) >> 2);
    bits = ((bits & 0x0F0F0F0F) << 4) | ((bits & 0xF0F0F0F0) >> 4);
    bits = ((bits & 0x00FF00FF) << 8) | ((bits & 0xFF00FF00) >> 8);
    return bits;
}

float2 Hammersley(uint i, uint N)
{
    const float E1 = frac(float(i) / N);
    const float E2 = ReverseBits32(i) * 2.3283064365386963e-10;
    return float2(E1, E2);
}

float3 ImportanceSampleGGX(float2 E, float a2)
{
    const float phi = 2.0 * PI * E.x;
    const float cosTheta = sqrt((1.0 - E.y) / (1.0 + (a2 - 1.0) * E.y));
    const float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    float3 H;
    H.x = sinTheta * cos(phi);
    H.y = sinTheta * sin(phi);
    H.z = cosTheta;
    
    return H;
}

float D_GGX(float a2, float NoH)
{
    const float d = (NoH * a2 - NoH) * NoH + 1.0;
    return a2 / (PI * d * d);
}

float ImportancePdfGGX(float cosTheta, float a2)
{
    return cosTheta * D_GGX(a2, cosTheta);
}

float SpecularPdf(float NoH, float a2, float VoH)
{
    return ImportancePdfGGX(NoH, a2) / max(4.0 * VoH, 0.000001);
}

float Luminance(float3 color)
{
    return dot(color, float3(0.2126, 0.7152, 0.0722));
}

#endif