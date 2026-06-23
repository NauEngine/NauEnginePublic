#include "cocos_in_out.hlsli"

Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);

float4 PSMain(VsOutputPTC input) : SV_Target
{
    float4 color = u_texture.Sample(sampler_0, input.v_texCoord).bgra;
    // the texture use dual channel 16-bit output for distance_map
    // float dist = color.b+color.g/256.0;
    //  the texture use single channel 8-bit output for distance_map
    float dist = color.a;
    // TODO: Implementation 'fwidth' for glsl 1.0
    // float width = fwidth(dist);
    // assign width for constant will lead to a little bit fuzzy,it's temporary measure.
    float width = 0.04;
    float alpha = smoothstep(0.5 - width, 0.5 + width, dist) * u_textColor.a;
    return (input.v_fragmentColor * float4(u_textColor.rgb, alpha)).bgra;
}