// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved
#include "gbuffer_base.hlsli"

Texture2D albedo_gbuf: register(t0);
Texture2D normal_gbuf: register(t1);
Texture2D material_gbuf: register(t2);
Texture2D depth_gbuf: register(t3);

SamplerState default_sampler : register(s0);

PackedGbuffer readPackedGbuffer(float2 tc)
{
    PackedGbuffer gbuf;
    gbuf.albedo_ao = albedo_gbuf.SampleLevel(default_sampler, tc, 0);
    gbuf.normal_smoothness_material = normal_gbuf.SampleLevel(default_sampler, tc, 0);
    gbuf.metallTranslucency_shadow = material_gbuf.SampleLevel(default_sampler, tc, 0).xy;
    return gbuf;
}

ProcessedGbuffer readProcessedGbuffer(float2 tc)
{
    return processGbuffer(unpackGbuffer(readPackedGbuffer(tc)));
}