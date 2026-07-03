// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"
#include "shader_global.hlsli"
#include "cs_common.hlsli"
#include "pbr.hlsli"

#define SAMPLE_COUNT 2048
#define MAX_LUMINANCE 25.0

TextureCube environmentCubemap : register(t0);
SamplerState texSampler : register(s0);

RWTexture2D<float4> reflection_map_face : register(u0);

GLOBAL_CBUFFER(ConstBuffer) : register(b0)
{
    uint faceIndex;
    uint faceSize;
    float roughness;
};

[numthreads(CS_ENV_CUBEMAPS_BLOCK_SIZE, CS_ENV_CUBEMAPS_BLOCK_SIZE, 1)]
void CSMain(
    uint3 groupID : SV_GroupID,
    uint3 groupThreadID : SV_GroupThreadID,
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint  groupIndex : SV_GroupIndex
)
{
    const uint2 imageSize = uint2(faceSize, faceSize);
    const float2 uv = GetUV(dispatchThreadID.xy, imageSize);

    const float3 N = GetCubeDirection(faceIndex, uv);
    const float3 V = N;

    const float3x3 TBN = GetTBN(N);

    const float a = roughness * roughness;
    const float a2 = max(a * a, EPSILON);

    float3 result = float3(0.0, 0.0, 0.0);
    float totalWeight = 0.0;

    for (uint i = 0; i < SAMPLE_COUNT; ++i)
    {
        const float2 Xi = Hammersley(i, SAMPLE_COUNT);

        const float3 H = TangentToWorld(ImportanceSampleGGX(Xi, a2), TBN);

        const float3 L = -reflect(V, H);

        const float NoL = CosThetaWorld(N, L);
        const float NoH = CosThetaWorld(N, H);
        const float VoH = max(dot(V, H), 0.0);

        if (NoL > 0.0)
        {
            const float pdf = SpecularPdf(NoH, a2, VoH); 

            const float saTexel  = 4.0 * PI / (6.0 * imageSize.x * imageSize.y);
            const float saSample = 1.0 / (float(SAMPLE_COUNT) * pdf + EPSILON);

            const float lod = roughness == 0.0 ? 0.0 : 0.5 * log2(saSample / saTexel); 

            float3 irradiance = environmentCubemap.SampleLevel(texSampler, L, lod).rgb;
            irradiance /= max(Luminance(irradiance) / MAX_LUMINANCE, 1.0);

            result += irradiance * NoL;
            totalWeight += NoL;
        }
    }

    result /= totalWeight;

    reflection_map_face[dispatchThreadID.xy] = float4(result, 1.0);
}