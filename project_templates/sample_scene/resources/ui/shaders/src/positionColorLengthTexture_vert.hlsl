#include "cocos_in_out.hlsli"

VsOutputPTC VSMain(VsInputPTC input)
{
    VsOutputPTC output = (VsOutputPTC)0;

    output.position = mul(u_MVPMatrix, input.a_position);
    output.v_texCoord = input.a_texCoord;
    output.v_fragmentColor = float4(input.a_color.rgb * input.a_color.a * u_alpha, input.a_color.a * u_alpha);

    output.position.z = fromOpenGLDepth(output.position.z, output.position.w);
    return output;
}
