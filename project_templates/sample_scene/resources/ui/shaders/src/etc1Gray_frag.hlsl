#include "cocos_in_out.hlsli"


Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);
Texture2D u_texture1 : register(t1);
SamplerState sampler_1 : register(s1);


float4 PSMain( VsOutputPTC input ) : SV_Target
{
    float4 texColor = u_texture.Sample(sampler_0, input.v_texCoord).bgra;
    texColor.a = u_texture1.Sample(sampler_1, input.v_texCoord).r;
    texColor.rgb *= texColor.a; // premultiply alpha channel

    texColor = input.v_fragmentColor * texColor;
    
    float4 ret;
    ret.rgb = float(0.2126*texColor.r + 0.7152*texColor.g + 0.0722*texColor.b).rrr;
    ret.a = texColor.a;
    
    return ret.bgra;
}
