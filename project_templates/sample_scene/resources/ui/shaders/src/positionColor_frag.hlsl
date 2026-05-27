#include "cocos_in_out.hlsli"


float4 PSMain( VsOutputPTC input ) : SV_Target
{
    return input.v_fragmentColor;
}
