// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"

Texture2D tex : register(t0);
SamplerState sampl : register(s0);

GLOBAL_CBUFFER(GlobalBuffer) : register(b0)
{
    float4x4 vp;
};

SYSTEM_CBUFFER(BillboardBuffer) : register(b1)
{
    float3 worldPosition;
    float scPercentSize;
    uint4 uid;
    float4 aspectRatio;
};

struct VsOutput
{
    float4 position : SV_POSITION;
    float2 texCoord : TEXCOORD0;
    nointerpolation uint4 uid : TEXCOORD1;
};

VsOutput billboardsVS(uint id : SV_VertexID)
{
    VsOutput output = (VsOutput)0;

    output.texCoord = float2(id & 1, (id & 2) >> 1);
    output.position = mul(vp, float4(worldPosition, 1.0f));
    output.position = output.position / output.position.w; // to ndc we go

    float2 offset = output.texCoord * float2(2*scPercentSize, -2*scPercentSize) + float2(-scPercentSize, scPercentSize);
    offset.y *= aspectRatio.x;
    output.position.xy += offset;

    output.uid = uid;

    return output;
}


struct PsOutput
{
    float4 color : SV_Target0;
    uint4 uid    : SV_Target1;
};

PsOutput billboardsPS(VsOutput input)
{
    PsOutput output;
    output.color = tex.Sample(sampl, input.texCoord);
    output.uid = input.uid;
    clip(output.color.a - 0.01f);
    return output;
}
