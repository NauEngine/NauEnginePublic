#include "cocos_in_out.hlsli"


Texture2D u_texture : register(t0);
SamplerState sampler_0 : register(s0);


float4 PSMain( VsOutputPTC input ) : SV_Target
{
    float4 sample = u_texture.Sample(sampler_0, input.v_texCoord);
    // fontAlpha == 1 means the area of solid text (without edge)
    // fontAlpha == 0 means the area outside text, including outline area
    // fontAlpha == (0, 1) means the edge of text
    float fontAlpha = sample.a;

    // outlineAlpha == 1 means the area of 'solid text' and 'solid outline'
    // outlineAlpha == 0 means the transparent area outside text and outline
    // outlineAlpha == (0, 1) means the edge of outline
    float outlineAlpha = sample.r;

    if (u_effectType == 0) // draw text
    {
        return (input.v_fragmentColor * float4(u_textColor.rgb, u_textColor.a * fontAlpha)).bgra;
    }
    else if (u_effectType == 1) // draw outline
    {
        // multipy (1.0 - fontAlpha) to make the inner edge of outline smoother and make the text itself transparent.
        return (input.v_fragmentColor * float4(u_effectColor.rgb, u_effectColor.a * outlineAlpha * (1.0 - fontAlpha))).bgra;
    }
    else // draw shadow
    {
        return (input.v_fragmentColor * float4(u_effectColor.rgb, u_effectColor.a * outlineAlpha)).bgra;
    }
}
