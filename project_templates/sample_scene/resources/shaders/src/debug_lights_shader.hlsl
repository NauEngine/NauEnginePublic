// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#include "shader_defines.h"
#include "shader_global.hlsli"
#include "clustered/point_light.hlsli"

struct VsInput
{
    float3 pos : POSITION;
};

struct VsOutput
{
    VS_OUT_POSITION(pos)
};

SYSTEM_CBUFFER(OmniLights) :
    register(b1)
{
    uint4 omni_lights_count;
    RenderOmniLight omni_lights_cb[MAX_OMNI_LIGHTS];
};

VsOutput debug_point_lights_vs(VsInput input, uint omni_light_index : SV_InstanceID)
{
    VsOutput output;
    RenderOmniLight ol = omni_lights_cb[omni_light_index];
    float4 pos_and_radius = ol.posRadius;
    float3 worldPos = pos_and_radius.xyz + input.pos.xyz * pos_and_radius.w * 1.15;
    output.pos = mul(mvp, float4(worldPos, 1));
    return output;
}

float4 debug_point_lights_ps(VsOutput input) :
    SV_Target
{
    return float4(1, 0, 0, 0);
}

SYSTEM_CBUFFER(SpotLights) : register(b1)
{
    uint4 spot_lights_count;
    RenderSpotLight spot_lights_cb[MAX_SPOT_LIGHTS];
}

VsOutput debug_spot_lights_vs(VsInput input, uint spot_light_index : SV_InstanceID)
{
    VsOutput output;
    RenderSpotLight sl = spot_lights_cb[spot_light_index];
    float4 pos_and_radius = sl.lightPosRadius;
    float4 color_and_attenuation = sl.lightColorAngleScale;
    color_and_attenuation.w = abs(color_and_attenuation.w);
    float4 dir_angle = sl.lightDirectionAngleOffset;
    const float lightAngleScale = color_and_attenuation.a;
    const float lightAngleOffset = dir_angle.a;
    float2 texId_scale = sl.texId_scale.xy;
    float cosOuter = -lightAngleOffset / lightAngleScale;
    float halfTan = sqrt(1 / (cosOuter * cosOuter) - 1);
    float3 ofs;
    if (dot(input.pos, input.pos) > 0)
    {
        ofs = tangent_to_world(normalize(float3(input.pos.xy * halfTan, input.pos.z)), dir_angle.xyz);
    }
    else
    {
        ofs = float3(0, 0, 0);
    }
    float4 worldPos = float4(pos_and_radius.xyz + ofs.xyz * (pos_and_radius.w / cosOuter), 1);
    output.pos = mul(mvp, worldPos);
    return output;
}

float4 debug_spot_lights_ps(VsOutput input) : SV_Target
{
    return float4(1, 0, 0, 0);
}