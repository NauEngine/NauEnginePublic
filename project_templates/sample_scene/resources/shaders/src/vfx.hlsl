// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"
#include "in_out.hlsli"

struct InstanceData
{
    float4x4 worldMatrix;
    int frameID;
    float4 color;
    uint3 dummy;
};

// Structure buffer for instance (InstanceData)
StructuredBuffer<InstanceData> instanceBuffer : register(t1);

Texture2D tex : register(t0);
SamplerState sampl : register(s0);

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 view;
    float4x4 projection;
};

GLOBAL_CBUFFER(AtlasBuffer) : register(b1)
{
    int frames_y;
    int frames_x;
};

VsVFXOutput VSMain(VsInput input, uint instanceId : SV_InstanceID)
{
    InstanceData instanceData = instanceBuffer[instanceId];

    float4x4 model = instanceData.worldMatrix;

    float4 origin = float4(0.0, 0.0, 0.0, 1.0);
    float4 world_origin = mul(model, origin);
    float4 view_origin = mul(view, world_origin);
    float4 world_to_view_translation = view_origin - world_origin;

    float4 world_pos = mul(model, float4(input.position, 1.0));
    float4 view_pos = world_pos + world_to_view_translation;
    float4 clip_pos = mul(projection, view_pos);

    VsVFXOutput output;

    output.position = clip_pos;
    output.color = instanceData.color;
    output.frameID = instanceData.frameID;
    output.texCoord = input.texCoord;
    //output.columns = frames_y;
    //output.rows = frames_x;

    return output;
}

float4 PSMain(VsVFXOutput input) : SV_Target
{
    int currentFrame = input.frameID;

    //int columns = input.columns;
    //int rows = input.rows;

    int columns = 1;
    int rows = 1;

    float frameWidth = 1.0 / columns;
    float frameHeight = 1.0 / rows;

    int frameX = currentFrame % columns;
    int frameY = currentFrame / columns;

    float2 frameOrigin = float2(frameX * frameWidth, (float(rows - 1 - frameY)) * frameHeight);
    float2 frameUV = frameOrigin + input.texCoord * float2(frameWidth, frameHeight);

    float4 albedo = tex.Sample(sampl, frameUV);
    return albedo * input.color;
}
