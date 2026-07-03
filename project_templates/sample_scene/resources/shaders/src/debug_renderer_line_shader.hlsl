#include "shader_defines.h"

struct ConstantData
{
    float4x4 WorldViewProj;
    float4 Color;
};

SYSTEM_CBUFFER(SceneBuffer) : register(b0)
{
    ConstantData ConstData;
};

struct VS_IN
{
    float4 pos : POSITION0;
    float4 col : COLOR0;
};

struct PS_IN
{
    float4 pos : SV_POSITION;
    float4 col : COLOR;
};

PS_IN VSMain( VS_IN input )
{
    PS_IN output = (PS_IN)0;
    
    output.pos = mul(ConstData.WorldViewProj, float4(input.pos.xyz, 1.0f));
    output.col = input.col;
    
    return output;
}

float4 PSMain( PS_IN input ) : SV_Target
{
    float4 col = input.col;
    return col;
}
