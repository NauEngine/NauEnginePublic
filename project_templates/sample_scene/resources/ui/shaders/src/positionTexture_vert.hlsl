#include "cocos_in_out.hlsli"

VsOutputPTC VSMain( VsInputPT input )
{
    VsOutputPTC output = (VsOutputPTC)0;
	
    output.position = mul(u_MVPMatrix, input.a_position);
    output.v_texCoord = input.a_texCoord;

    output.position.z = fromOpenGLDepth(output.position.z, output.position.w);
    return output;
}
