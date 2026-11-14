// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Composites the UI layer (ui_layer, premultiplied alpha) over the back buffer.
// The blend is PremultipliedOver (ui_composite.nmat_json): result = ui.rgb + scene.rgb * (1 - ui.a).

#include "shader_defines.h"
#include "in_out.hlsli"

Texture2D ui_layer : register(t0);

VsOutput VSMain(uint id : SV_VertexID)
{
    VsOutput output = (VsOutput)0;

    // Full-screen quad, drawn by PostFxRenderer as PRIM_TRISTRIP with 4 vertices (same as postFX_shader.hlsl).
    output.texCoord = float2((id << 1) & 2, id & 2);
    output.position = float4(output.texCoord * float2(2, -2) + float2(-1, 1), 0, 1);
    output.color = float4(1, 1, 1, 1);

    return output;
}

float4 PSMain(VsOutput input) : SV_Target
{
    // The layer has the size of the back buffer: one texel per pixel, no filtering.
    return ui_layer.Load(int3(input.position.xy, 0));
}
