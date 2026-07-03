// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Heavily inspired by https://bgolus.medium.com/the-quest-for-very-wide-outlines-ba82ed442cd9

#include "shader_defines.h"
#include "in_out.hlsli"

GLOBAL_CBUFFER(PropertyBuffer) : register(b0)
{
    uint screenWidth;
    uint screenHeight;
    float jumpStepWidth;
    float4 outlineColor;
};

VsOutput VSMain(uint id : SV_VertexID)
{
    VsOutput output = (VsOutput)0;

    // TODO: Rewrite to use only one triangle in future
    output.texCoord = float2((id << 1) & 2, id & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    output.color = float4(output.texCoord, 1, 0.5);

    return output;
}

// just inside the precision of a R16G16_SNorm to keep encoded range 1.0 >= and > -1.0
#define SNORM16_MAX_FLOAT_MINUS_EPSILON ((float)(32768 - 2) / (float)(32768 - 1))

#define FLOOD_NULL_POS -1.0
#define FLOOD_NULL_POS_FLOAT2 float2(FLOOD_NULL_POS, FLOOD_NULL_POS)

Texture2D outline_tex : register(t0);
SamplerState sampl : register(s0);

float2 PSMainJumpFloodInit(VsOutput input) :
    SV_Target
{
    float2 sceneUV = input.texCoord.xy;
    // integer pixel position
    int2 uvInt = sceneUV * int2(screenWidth, screenHeight);

    // sample silhouette texture for sobel
    half3x3 values;
    [unroll] for (int u = 0; u < 3; u++)
    {
        [unroll] for (int v = 0; v < 3; v++)
        {
            uint2 sampleUV = clamp(uvInt + int2(u - 1, v - 1), int2(0, 0), int2(screenWidth, screenHeight));
            values[u][v] = 1 - outline_tex.Load(int3(sampleUV, 0)).r;
        }
    }
    // calculate output position for this pixel
    float2 outPos = sceneUV;

    // interior, return position
    if (values._m11 > 0.99)
    {
        return outPos;
    }

    // exterior, return no position
    if (values._m11 < 0.01)
    {
        return FLOOD_NULL_POS_FLOAT2;
    }

    // sobel to estimate edge direction
    float2 dir = -float2(
        values[0][0] + values[0][1] * 2.0 + values[0][2] - values[2][0] - values[2][1] * 2.0 - values[2][2],
        values[0][0] + values[1][0] * 2.0 + values[2][0] - values[0][2] - values[1][2] * 2.0 - values[2][2]);

    // if dir length is small, this is either a sub pixel dot or line
    // no way to estimate sub pixel edge, so output position
    if (abs(dir.x) <= 0.005 && abs(dir.y) <= 0.005)
        return outPos;

    // normalize direction
    dir = normalize(dir);

    // sub pixel offset
    float2 offset = dir * (1.0 - values._m11);

    // output encoded offset position
    return uvInt + offset;
}

float2 PSMainJumpFloodStep(VsOutput input) :
    SV_Target
{
    float2 sceneUV = input.texCoord.xy;
    // integer pixel position
    int2 uvInt = sceneUV * int2(screenWidth, screenHeight);

    // initialize best distance at infinity
    const float maxDist = (screenWidth + screenHeight) * (screenWidth + screenHeight);
    float bestDist = maxDist;
    float2 bestCoord;

    // jump samples
    [unroll] for (int u = -1; u <= 1; u++)
    {
        [unroll] for (int v = -1; v <= 1; v++)
        {
            // calculate offset sample position
            int2 offsetUV = uvInt + int2(u, v) * jumpStepWidth;

            // .Load() acts funny when sampling outside of bounds, so don't
            offsetUV = clamp(offsetUV, int2(0, 0), int2(screenWidth, screenHeight));

            // decode position from buffer
            float2 offsetPos = outline_tex.Load(int3(offsetUV, 0)).rg;

            // the offset from current position
            float2 disp = uvInt - offsetPos * int2(screenWidth, screenHeight);

            // square distance
            float dist = dot(disp, disp);

            // if offset position isn't a null position or is closer than the best
            // set as the new best and store the position
            if (offsetPos.y != FLOOD_NULL_POS && dist < bestDist)
            {
                bestDist = dist;
                bestCoord = offsetPos;
            }
        }
    }

    // if not valid best distance output null position, otherwise output encoded position
    return maxDist == bestDist ? FLOOD_NULL_POS_FLOAT2 : bestCoord;
}

Texture2D outlineMask_tex : register(t1);

float4 PSMainJumpFloodResult(VsOutput input) :
    SV_Target
{
    float2 sceneUV = input.texCoord.xy;
    // integer pixel position
    int2 uvInt = sceneUV * int2(screenWidth, screenHeight);

    // load encoded position
    float2 encodedPos = outline_tex.Load(int3(uvInt, 0)).rg;
    float isInside = outlineMask_tex.Load(int3(uvInt, 0)).r;

    // early out if outside
    if (isInside < 0.9)
        return half4(0, 0, 0, 0);
    // early out if null position
    if (encodedPos.y == -1)
        return half4(0, 0, 0, 0);

    // decode closest position
    float2 nearestPos = encodedPos* int2(screenWidth, screenHeight);

    // current pixel position
    float2 currentPos = uvInt;

    // distance in pixels to closest position
    half dist = length(nearestPos - currentPos);

    // calculate outline
    // + 1.0 is because encoded nearest position is half a pixel inset
    // not + 0.5 because we want the anti-aliased edge to be aligned between pixels
    // distance is already in pixels, so this is already perfectly anti-aliased!
    half outline = clamp(jumpStepWidth - dist + 1.0, 0, jumpStepWidth);

    half outlineAlpha = outline/jumpStepWidth;

    // apply outline to alpha
    half4 col = outlineColor;
    col.a *= outlineAlpha;

    return col;
}