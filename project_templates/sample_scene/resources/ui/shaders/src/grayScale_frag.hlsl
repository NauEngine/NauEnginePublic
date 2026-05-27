#include "cocos_in_out.hlsli"

Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);


float4 PSMain( VsOutputPTC input ) : SV_Target
{
    float4 c =  u_texture.Sample(sampler_0, input.v_texCoord).bgra;
     c = input.v_fragmentColor * c;
    float4 result;
    result.xyz = float(0.2126*c.r + 0.7152*c.g + 0.0722*c.b).xxx;
    result.w = c.w;
    return result.bgra;
}
