#include "cocos_in_out.hlsli"


Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);
Texture2D u_texture1 : register(t1);
SamplerState sampler_1 : register(s1);


float4 PSMain( VsOutputPTC input ) : SV_Target
{
    float4 texColor = float4(u_texture.Sample(sampler_0, input.v_texCoord).bgr, u_texture1.Sample(sampler_1, input.v_texCoord).r);

    texColor.rgb *= texColor.a; // Premultiply with Alpha channel

    return (texColor * input.v_fragmentColor).bgra;
}
