// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

cbuffer UIConstBuffer : register(b1)
{
    float4x4 u_MVPMatrix;
    float4 u_textColor;
    float4 u_effectColor;

    float4 u_startColor;
    float4 u_endColor;
    float4 u_color;
    
    float2 u_center;
    float u_radius;
    float u_expand;
    uint u_effectType;
    float u_alpha_value;
    float u_alpha;
    float depth;
    // float4 Color;
};

struct VsInputP
{
    float4 a_position : POSITION;
};

struct VsInputPC
{
    float4 a_position : POSITION;
    float4 a_color : COLOR;
};

struct VsInputPT
{
    float4 a_position : POSITION;
    float2 a_texCoord : TEXCOORD;
};

struct VsInputPTC
{
    float4 a_position : POSITION;
    float4 a_color : COLOR;
    float2 a_texCoord : TEXCOORD;
};

struct VsOutputPTC
{
    float4 position : SV_POSITION;
    float4 v_position : POSITION;
    float4 v_fragmentColor : COLOR;
    float2 v_texCoord : TEXCOORD;
};

float fromOpenGLDepth(float z, float w)
{
    return (z+w)/2;
}