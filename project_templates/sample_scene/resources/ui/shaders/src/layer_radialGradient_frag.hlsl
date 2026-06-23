#include "cocos_in_out.hlsli"


float4 PSMain( VsOutputPTC input ) : SV_Target
{
   float d = distance(input.v_position.xy, u_center) / u_radius;
    if (d <= 1.0)
    {
        if (d <= u_expand)
        {
            return u_startColor.bgra;
        }
        else
        {
            return lerp(u_startColor, u_endColor, (d - u_expand) / (1.0 - u_expand)).bgra;
        }
    }
    else
    {
        return float4(0.0, 0.0, 0.0, 0.0);
    }
}
