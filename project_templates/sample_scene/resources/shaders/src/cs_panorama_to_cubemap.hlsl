// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"
#include "cs_common.hlsli"

Texture2D panorama_texture : register(t0);
SamplerState texSampler : register(s0);

RWTexture2D<float4> cubemap_texture_face : register(u0);

GLOBAL_CBUFFER(ConstBuffer) : register(b0)
{
    uint faceIndex;
    uint faceSize;
};

float2 ComputePanoramaTexCoord(float3 direction)
{
    const float2 inverseAtan = float2(0.1591, 0.3183);

    return float2(atan2(direction.z, direction.x), asin(-direction.y)) * inverseAtan + 0.5;
}

[numthreads(CS_ENV_CUBEMAPS_BLOCK_SIZE, CS_ENV_CUBEMAPS_BLOCK_SIZE, 1)]
void CSMain(
    uint3 groupID : SV_GroupID,
    uint3 groupThreadID : SV_GroupThreadID,
    uint3 dispatchThreadID : SV_DispatchThreadID,
    uint  groupIndex : SV_GroupIndex
)
{
    const float2 uv = GetUV(dispatchThreadID.xy, uint2(faceSize, faceSize));

    const float3 direction = GetCubeDirection(faceIndex, uv);

    const float2 panoramaTexCoord = ComputePanoramaTexCoord(direction);

    float4 panoramaSample = panorama_texture.SampleLevel(texSampler, panoramaTexCoord, 0);

    cubemap_texture_face[dispatchThreadID.xy] = panoramaSample;
}