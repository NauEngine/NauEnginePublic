// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"
#include "shader_global.hlsli"
#include "cs_common.hlsli"

#define SAMPLE_DELTA 0.05

TextureCube environmentCubemap : register(t0);
SamplerState texSampler : register(s0);

RWTexture2D<float4> irradiance_map_face : register(u0);

GLOBAL_CBUFFER(ConstBuffer) : register(b0)
{
    uint faceIndex;
    uint faceSize;
};

[numthreads(CS_ENV_CUBEMAPS_BLOCK_SIZE, CS_ENV_CUBEMAPS_BLOCK_SIZE, 1)]
void CSMain(
    uint3 groupID : SV_GroupID,
    uint3 groupThreadID : SV_GroupThreadID,
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint  groupIndex : SV_GroupIndex
)
{
    const float2 uv = GetUV(dispatchThreadID.xy, uint2(faceSize, faceSize));

    const float3 N = GetCubeDirection(faceIndex, uv);

    const float3x3 TBN = GetTBN(N);

    uint sampleCount = 0;
    float3 irradiance = float3(0.0, 0.0, 0.0);

    for (float phi = 0.0; phi < 2.0 * PI; phi += SAMPLE_DELTA)
    {
        for (float theta = 0.0; theta < 0.5 * PI; theta += SAMPLE_DELTA)
        {
            const float3 tangentDirection = float3(
                sin(theta) * cos(phi),
                sin(theta) * sin(phi),
                cos(theta));

            const float3 worldDirection = TangentToWorld(tangentDirection, TBN);

            irradiance += environmentCubemap.SampleLevel(texSampler, worldDirection, 5).rgb
             * cos(theta) * sin(theta);

            ++sampleCount;
        }
    }

    irradiance = PI * irradiance / float(sampleCount);

    irradiance_map_face[dispatchThreadID.xy] = float4(irradiance, 1.0f);
}