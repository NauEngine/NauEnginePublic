// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_global.hlsli"

static float3 CUBE_FACES_N[CUBE_FACE_COUNT] = {
    UNIT_X,
    -UNIT_X,
    UNIT_Y,
    -UNIT_Y,
    UNIT_Z,
    -UNIT_Z
};
static float3 CUBE_FACES_T[CUBE_FACE_COUNT] = {
    -UNIT_Z,
    UNIT_Z,
    UNIT_X,
    UNIT_X,
    UNIT_X,
    -UNIT_X
};
static float3 CUBE_FACES_B[CUBE_FACE_COUNT] = {
    -UNIT_Y,
    -UNIT_Y,
    UNIT_Z,
    -UNIT_Z,
    -UNIT_Y,
    -UNIT_Y
};

float2 GetUV(uint2 id, uint2 imageSize)
{
    const float2 pixelSize = 1.0f / imageSize;
    return pixelSize * id + pixelSize * 0.5;
}

float3 GetCubeDirection(uint curFaceIndex, float2 uv)
{
    const float2 xy = uv * 2.0 - 1.0;

    const float3 normal = CUBE_FACES_N[curFaceIndex];
    const float3 tangent = CUBE_FACES_T[curFaceIndex];
    const float3 binormal = CUBE_FACES_B[curFaceIndex];
    
    return normalize(normal + xy.x * tangent + xy.y * binormal);
}

float CosThetaWorld(float3 N, float3 v)
{
    return max(dot(N, v), 0.0);
}