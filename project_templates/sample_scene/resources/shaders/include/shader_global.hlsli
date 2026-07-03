// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#ifndef SHADER_GLOBAL_HLSL
#define SHADER_GLOBAL_HLSL 1

#include "hardware_defines.hlsli"

float pow2(float a) {return a*a;}
float pow4(float a) {return pow2(a*a);}
float pow8(float a) {return pow4(a*a);}
float2 pow2_vec2(float2 a) {return a*a;}
float3 pow2_vec3(float3 a) {return a*a;}
float4 pow2_vec4(float4 a) {return a*a;}

float2 pow2(float2 a) {return a*a;}
float3 pow2(float3 a) {return a*a;}
float4 pow2(float4 a) {return a*a;}
float4 pow4(float4 a) {return pow2(a*a);}
float4 pow8(float4 a) {return pow4(a*a);}
float4 pow16(float4 a) {return pow8(a*a);}
float pow5(float a) {float v = a*a; v*=v; return v*a;}

half pow2h(half a) {return a*a;}
half pow4h(half a) {return pow2h(a*a);}
half pow5h(half a){half a4=a*a; a4*=a4; return a4*a;}

float clampedPow(float X,float Y) { return pow(max(abs(X),0.000001f),Y); }

float3 TangentToWorld(float3 v, float3x3 TBN)
{
    return mul(TBN, v);
}

float3x3 GetTBN(float3 N)
{
    float3x3 TBN;

    float3 T = cross(N, UNIT_Y);
    T = lerp(cross(N, UNIT_X), T, step(EPSILON, dot(T, T)));
    T = normalize(T);

    const float3 B = normalize(cross(N, T));

    return float3x3( // TODO: NAU-1797 think about this transpose, refactor
        float3(T.x, B.x, N.x),
        float3(T.y, B.y, N.y),
        float3(T.z, B.z, N.z));
}

float3x3 GetTBN(float3 N, float3 T)
{
    T = normalize(T - dot(T, N) * N);
    const float3 B = cross(N, T);

    return float3x3( // TODO: NAU-1797 think about this transpose, refactor
        float3(T.x, B.x, N.x),
        float3(T.y, B.y, N.y),
        float3(T.z, B.z, N.z));
}

float linearize_z(float rawDepth, float2 decode_depth)
{
    return rcp(decode_depth.x + decode_depth.y * rawDepth);
}

#endif