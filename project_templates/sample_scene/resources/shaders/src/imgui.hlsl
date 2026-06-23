// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "shader_defines.h"

GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4x4 mvp;
};

Texture2D tex : register(t0);
SamplerState sampl : register(s0);

struct VsInput
{
    float2 pos : POSITION;
    float2 uv : TEXCOORD0;
    float4 col : COLOR0;
};

struct VsOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};


VsOutput imgui_vs(VsInput v)
{
  VsOutput o;
  o.pos = mul(mvp, float4(v.pos.xy, 0.f, 1.f));
  o.uv = v.uv;
  o.col = v.col.bgra; // dagor supports vertex color in BGRA format, but ImGui supplies it as RGBA
  return o;
}


float4 imgui_ps(VsOutput i) : SV_Target
{
    return i.col * tex.Sample(sampl, i.uv);
}

