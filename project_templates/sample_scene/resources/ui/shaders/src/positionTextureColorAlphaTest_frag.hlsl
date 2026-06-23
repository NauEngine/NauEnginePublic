#include "cocos_in_out.hlsli"


Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);


float4 PSMain( VsOutputPTC input ) : SV_Target
{
    float4 texColor = u_texture.Sample(sampler_0, input.v_texCoord).bgra;
    if ( texColor.a <= u_alpha_value )
        discard;

    return (texColor * input.v_fragmentColor).bgra;
}
