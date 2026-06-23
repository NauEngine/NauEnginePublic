#include "cocos_in_out.hlsli"


VsOutputPTC VSMain( VsInputPTC input )
{
    VsOutputPTC output = (VsOutputPTC)0;
	
    output.position = input.a_position;
    output.position.z = fromOpenGLDepth(depth, 1);
    output.position.w = 1.0;
    output.v_texCoord = input.a_texCoord;
    output.v_fragmentColor = input.a_color;
    
    return output;
}

