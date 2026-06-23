#include "cocos_in_out.hlsli"



VsOutputPTC VSMain( VsInputP input )
{
    VsOutputPTC output = (VsOutputPTC)0;
	
    output.position = mul(u_MVPMatrix, input.a_position);
    output.v_position = input.a_position;

    output.position.z = fromOpenGLDepth(output.position.z, output.position.w);
    return output;
}
