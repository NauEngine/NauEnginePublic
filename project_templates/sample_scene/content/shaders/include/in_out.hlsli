// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

struct VsInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
};

struct VsOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD;
};

struct VsVFXOutput
{
    float4 position : SV_Position;
    float4 color : COLOR;
    float2 texCoord : TEXCOORD;
    int frameID : TEXCOORD1;
    int columns : TEXCOORD2;
    int rows : TEXCOORD3;
};

struct VsInputLit
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texCoord : TEXCOORD0;
    float4 tangent : TANGENT;
};

struct VsOutputLit
{
    float4 position : SV_Position;
    float3 norm : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
};

struct VsOutputLitForward
{
    float4 position : SV_Position;
    float3 norm : NORMAL;
    float2 texCoord : TEXCOORD;
    float3 tangent : TANGENT;
    float3 worldPos : TEXCOORD1;
    float4 color : COLOR;
};

struct VsInputZPrepass
{
    float3 position : POSITION;
};

struct VsInputZPrepassSkinned
{
    float3 position : POSITION;
    float4 boneWeights : BLENDWEIGHT;
    uint4 boneIndices : BLENDINDICES;
};

struct VsOutputGBuff
{
    float4 position : SV_Position;
    float3 norm : NORMAL;
    float3 tangent : TANGENT;
    float2 texCoord : TEXCOORD;
    float3 p2e : TEXCOORD1;
};

struct VsOutputZPrepass
{
    float4 position : SV_Position;
    nointerpolation uint4 uid : TEXCOORD0;
};

struct VsOutputResolve
{
    float4 position : SV_Position;
    float2 texCoord     : TEXCOORD0;
};
