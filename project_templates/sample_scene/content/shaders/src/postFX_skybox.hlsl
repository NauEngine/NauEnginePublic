// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"

struct VsOutputSkybox
{
    float4 position : SV_Position;
    float3 texCoord : TEXCOORD;
};

TextureCube environmentCubemap : register(t0);
SamplerState sampl : register(s0);

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

VsOutputSkybox VSMain(uint id : SV_VertexID)
{
    VsOutputSkybox output = (VsOutputSkybox)0;

    const float3 position = float3(
        ((id & 0x4) == 0) ? 1.0 : -1.0,
        ((id & 0x2) == 0) ? 1.0 : -1.0,
        ((id & 0x1) == 0) ? 1.0 : -1.0);

    output.texCoord = position;

    float4 viewPositionDiscardTranslation = mul(viewMatrix, float4(position, 0.0f));
    float4 projectedPosition = mul(projectionMatrix, viewPositionDiscardTranslation);
    projectedPosition.z = 0.0f; // since we use ReverseZ, otherwise set to .w

    output.position = projectedPosition;

    return output;
}

float4 PSMain(VsOutputSkybox input) : SV_Target
{
    float4 sky = environmentCubemap.Sample(sampl, input.texCoord);
    sky.a = 0;
    return sky;
}
