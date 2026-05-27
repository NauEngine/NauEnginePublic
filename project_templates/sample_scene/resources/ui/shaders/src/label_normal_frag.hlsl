#include "cocos_in_out.hlsli"

Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);

float4 PSMain(VsOutputPTC input) : SV_Target
{
    return float4(
        (input.v_fragmentColor*u_textColor.bgra).rgb,                       // RGB from uniform
        (u_texture.Sample(sampler_0, input.v_texCoord) * u_textColor).a // A from texture & uniform
    ).bgra;
}