// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

struct InstanceData
{
    float4x4 worldMatrix;
    float4x4 normalMatrix;
    uint4 uid;
    uint isHighlighted;
    uint3 dummy;
};

struct PixelData
{
    uint4 uid;
    float depth;
};
