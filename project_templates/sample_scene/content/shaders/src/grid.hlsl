// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "shader_defines.h"


GLOBAL_CBUFFER(SceneBuffer) : register(b0)
{
    float4 worldViewPos;
    float4x4 vp;
}


struct PS_IN
{
	float4 pos : SV_POSITION;
    float4 worldPos : TEXCOORD0;
};



PS_IN VSMain(uint id : SV_VertexID )
{
	PS_IN output = (PS_IN)0;
	
    float2 inds = float2(id & 1, (id & 2) >> 1);
    float2 xy = inds * float2(2, -2) + float2(-1, 1);
    output.worldPos = float4(xy.x * 10000, 0, xy.y * 10000, 1.0f);

    output.pos = mul(vp, output.worldPos);
	
	return output;
}


float gridTextureGradBox(in float2 p, in float2 ddx, in float2 ddy)
{
    const float N = 100.0f;
    // filter kernel
    float2 w = max(abs(ddx), abs(ddy)) + 0.01f;
    p += 0.5f / N; // middle line offset

    float2 a = p + 0.5 * w;
    float2 b = p - 0.5 * w;
    float2 i = (floor(a) + min(frac(a) * N, 1.0) -
              floor(b) - min(frac(b) * N, 1.0)) / (N * w);
    return (1.0 - i.x) * (1.0 - i.y);
}


float4 PSMain( PS_IN input ) : SV_Target
{
    float level = log10(abs(worldViewPos.y));
    float alpha = level < 1.0f ? 0.0f : 1.0f - frac(level);

    level = clamp(floor(level), 0.0f, 4.0f);

    float2 uv = input.worldPos.xz;
    float2 dx = ddx(uv);
    float2 dy = ddy(uv);
    
    float scale0 = pow(0.1f, level);
    float t0 = 1.0f - gridTextureGradBox(uv * scale0, dx * scale0, dy * scale0);

    float scale1 = pow(0.1f, level-1);
    float t1 = 1.0f - gridTextureGradBox(uv * scale1, dx * scale1, dy * scale1);

    float t = lerp(t0, t1, alpha);
    float3 color = float3(0.3f, 0.3f, 0.3f);

    float g = 0.01 * pow(10.0f, level);
    if (abs(input.worldPos.x) < g)
        color = float3(0.8f, 0.0f, 0.0f);
    if (abs(input.worldPos.z) < g)
        color = float3(0.0f, 0.0f, 0.8f);

    return float4(color, t);
}
