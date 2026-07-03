#include "shader_defines.h"
#include "common_types.hlsli"

RWStructuredBuffer<PixelData> ResultBuffer : register(u0);

Texture2D<uint4> UIDTexture : register(t0);
Texture2D<float> DepthTexture : register(t1);

GLOBAL_CBUFFER(CoordBuffer) : register(b0)
{
    int2 viewportCoords;
};

[numthreads(1, 1, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID) 
{
    uint4 guid = UIDTexture.Load(int3(viewportCoords, 0));
    float depth = DepthTexture.Load(int3(viewportCoords, 0));

    PixelData result;
    
    result.uid = guid;
    result.depth = depth;

    ResultBuffer[0] = result;
}
