// Multi-pass depth of field adapted from Mikhanil/Multi-GPU-Render-DX12.
// Colors remain linear HDR throughout; the existing Nau post-process performs tonemapping.

#define THREADS_X 8
#define THREADS_Y 8

static const float kGoldenAngle = 2.39996323f;

bool isStatusMarker(uint2 pixel, uint2 size)
{
    const uint2 markerMin = max(uint2(1, 1), size / 128);
    const uint2 markerMax = max(markerMin + 1, size / 12);
    return all(pixel >= markerMin) && all(pixel < markerMax);
}

float activeFocusDistance(Texture2D<float> linearDepth, float fallbackDistance, float farZ, float autoFocus)
{
    uint width;
    uint height;
    linearDepth.GetDimensions(width, height);
    const float centerDistance = linearDepth[uint2(width / 2, height / 2)] * farZ;
    return autoFocus > 0.5f && centerDistance > 1e-4f ? centerDistance : fallbackDistance;
}

float2 sunflowerSample(uint index, uint tapCount, float2 rotation)
{
    const float radius = sqrt((float(index) + 0.5f) / float(tapCount));
    const float angle = float(index) * kGoldenAngle;
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    return float2(cosine * rotation.x - sine * rotation.y,
                  cosine * rotation.y + sine * rotation.x) * radius;
}

#if DOF_PASS_DEPTH

cbuffer DofDepthParams : register(b0)
{
    float nearZ;
    float farZ;
};

Texture2D<float> NdcDepth : register(t0);
RWTexture2D<float> LinearDepthOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    LinearDepthOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    // Nau uses reverse-Z: 1 at the near plane, 0 at the far plane.
    const float ndc = NdcDepth[id.xy];
    const float linearZ = (nearZ * farZ) / max(nearZ + ndc * (farZ - nearZ), 1e-6f);
    LinearDepthOut[id.xy] = saturate(linearZ / farZ);
}

#elif DOF_PASS_COC

cbuffer DofCoCParams : register(b0)
{
    float focusDistance;
    float focusRange;
    float farZ;
    float autoFocus;
    float maxCoC;
};

Texture2D<float> LinearDepth : register(t0);
RWTexture2D<float> CoCOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    CoCOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    const float focus = activeFocusDistance(LinearDepth, focusDistance, farZ, autoFocus);
    const float linearZ = LinearDepth[id.xy] * farZ;
    CoCOut[id.xy] = saturate(abs(linearZ - focus) / max(focusRange, 1e-4f)) * maxCoC;
}

#elif DOF_PASS_HOLES

cbuffer DofHolesParams : register(b0)
{
    float focusDistance;
    float focusRange;
    float farZ;
    float autoFocus;
};

Texture2D<float> LinearDepth : register(t0);
RWTexture2D<uint> OcclusionOut : register(u0);

uint depthLayer(float linearZ, float focus, float range)
{
    if (linearZ < focus - range) return 0;
    if (linearZ > focus + range) return 2;
    return 1;
}

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    OcclusionOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    const float focus = activeFocusDistance(LinearDepth, focusDistance, farZ, autoFocus);
    const uint layer = depthLayer(LinearDepth[id.xy] * farZ, focus, focusRange);
    if (layer == 0)
    {
        OcclusionOut[id.xy] = 0;
        return;
    }

    static const int2 offsets[8] = {
        int2(-1, -1), int2(0, -1), int2(1, -1), int2(-1, 0),
        int2(1, 0), int2(-1, 1), int2(0, 1), int2(1, 1)
    };

    uint occluded = 0;
    [unroll]
    for (uint index = 0; index < 8; ++index)
    {
        const int2 candidate = int2(id.xy) + offsets[index];
        if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
        if (depthLayer(LinearDepth[candidate] * farZ, focus, focusRange) < layer)
        {
            occluded = 1;
            break;
        }
    }
    OcclusionOut[id.xy] = occluded;
}

#elif DOF_PASS_PYRAMID1

Texture2D<float4> ColorIn : register(t0);
Texture2D<uint> OcclusionMask : register(t1);
RWTexture2D<float4> PyramidOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint outWidth;
    uint outHeight;
    PyramidOut.GetDimensions(outWidth, outHeight);
    if (id.x >= outWidth || id.y >= outHeight) return;

    uint inWidth;
    uint inHeight;
    ColorIn.GetDimensions(inWidth, inHeight);
    const uint2 base = id.xy * 2;
    float4 visible = 0;
    uint visibleCount = 0;
    [unroll]
    for (uint y = 0; y < 2; ++y)
    {
        [unroll]
        for (uint x = 0; x < 2; ++x)
        {
            const uint2 source = min(base + uint2(x, y), uint2(inWidth, inHeight) - 1);
            const float4 color = ColorIn[source];
            if (OcclusionMask[source] == 0)
            {
                visible += color;
                ++visibleCount;
            }
        }
    }
    PyramidOut[id.xy] = visibleCount > 0 ? visible / float(visibleCount) : ColorIn[min(base, uint2(inWidth, inHeight) - 1)];
}

#elif DOF_PASS_PYRAMID2

Texture2D<float4> PyramidIn : register(t0);
RWTexture2D<float4> PyramidOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint outWidth;
    uint outHeight;
    PyramidOut.GetDimensions(outWidth, outHeight);
    if (id.x >= outWidth || id.y >= outHeight) return;

    uint inWidth;
    uint inHeight;
    PyramidIn.GetDimensions(inWidth, inHeight);
    const uint2 base = id.xy * 2;
    float4 color = 0;
    [unroll]
    for (uint y = 0; y < 2; ++y)
    {
        [unroll]
        for (uint x = 0; x < 2; ++x)
        {
            color += PyramidIn[min(base + uint2(x, y), uint2(inWidth, inHeight) - 1)];
        }
    }
    PyramidOut[id.xy] = color * 0.25f;
}

#elif DOF_PASS_PATCH2

Texture2D<float4> ColorIn : register(t0);
Texture2D<uint> OcclusionMask : register(t1);
RWTexture2D<float4> FilledOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    FilledOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    uint fullWidth;
    uint fullHeight;
    ColorIn.GetDimensions(fullWidth, fullHeight);
    const uint2 fullCoord = min(id.xy * 4u, uint2(fullWidth, fullHeight) - 1u);
    if (OcclusionMask[fullCoord] == 0)
    {
        FilledOut[id.xy] = ColorIn[min(id.xy, uint2(fullWidth, fullHeight) - 1)];
        return;
    }

    float4 best = ColorIn[min(id.xy, uint2(fullWidth, fullHeight) - 1)];
    uint2 seed = id.xy;
    [loop]
    for (uint iteration = 0; iteration < 4; ++iteration)
    {
        static const int radii[4] = {8, 4, 2, 1};
        int radius = radii[iteration];
        static const int2 offsets[4] = {int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1)};
        [unroll]
        for (uint index = 0; index < 4; ++index)
        {
            const int2 candidate = int2(id.xy) + offsets[index];
            if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
            const uint2 candidateFull = min(uint2(candidate) * 4u, uint2(fullWidth, fullHeight) - 1u);
            if (OcclusionMask[candidateFull] == 0)
            {
                best = ColorIn[min(uint2(candidate), uint2(fullWidth, fullHeight) - 1)];
                radius = max(1, radius / 2);
                break;
            }
        }
        [loop]
        for (int search = 0; search < 4; ++search)
        {
            const int2 randomValue = int2(((seed.x * 1664525u + 1013904223u) / 65536u) & 31u,
                                          ((seed.y * 22695477u + 1u) / 65536u) & 31u);
            seed += randomValue + search;
            const int2 offset = (randomValue % (2 * radius + 1)) - radius;
            const int2 candidate = clamp(int2(id.xy) + offset, int2(0, 0), int2(width, height) - 1);
            const uint2 candidateFull = min(uint2(candidate) * 4u, uint2(fullWidth, fullHeight) - 1u);
            if (OcclusionMask[candidateFull] == 0)
            {
                best = ColorIn[min(uint2(candidate), uint2(fullWidth, fullHeight) - 1)];
                break;
            }
        }
    }
    FilledOut[id.xy] = best;
}

#elif DOF_PASS_PATCH1

Texture2D<float4> ColorIn : register(t0);
Texture2D<float4> CoarseFilled : register(t1);
Texture2D<uint> OcclusionMask : register(t2);
RWTexture2D<float4> FilledOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    FilledOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    uint fullWidth;
    uint fullHeight;
    ColorIn.GetDimensions(fullWidth, fullHeight);
    const uint2 fullCoord = min(id.xy * 2u, uint2(fullWidth, fullHeight) - 1u);
    if (OcclusionMask[fullCoord] == 0)
    {
        FilledOut[id.xy] = ColorIn[min(id.xy, uint2(fullWidth, fullHeight) - 1)];
        return;
    }

    uint coarseWidth;
    uint coarseHeight;
    CoarseFilled.GetDimensions(coarseWidth, coarseHeight);
    float4 best = CoarseFilled[min(id.xy, uint2(coarseWidth, coarseHeight) - 1)];
    uint2 seed = id.xy;
    [loop]
    for (uint iteration = 0; iteration < 4; ++iteration)
    {
        static const int radii[4] = {8, 4, 2, 1};
        int radius = radii[iteration];
        static const int2 offsets[4] = {int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1)};
        [unroll]
        for (uint index = 0; index < 4; ++index)
        {
            const int2 candidate = int2(id.xy) + offsets[index];
            if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
            const uint2 candidateFull = min(uint2(candidate) * 2u, uint2(fullWidth, fullHeight) - 1u);
            if (OcclusionMask[candidateFull] == 0)
            {
                best = ColorIn[min(uint2(candidate), uint2(fullWidth, fullHeight) - 1)];
                radius = max(1, radius / 2);
                break;
            }
        }
        [loop]
        for (int search = 0; search < 4; ++search)
        {
            const int2 randomValue = int2(((seed.x * 1664525u + 1013904223u) / 65536u) & 31u,
                                          ((seed.y * 22695477u + 1u) / 65536u) & 31u);
            seed += randomValue + search;
            const int2 offset = (randomValue % (2 * radius + 1)) - radius;
            const int2 candidate = clamp(int2(id.xy) + offset, int2(0, 0), int2(width, height) - 1);
            const uint2 candidateFull = min(uint2(candidate) * 2u, uint2(fullWidth, fullHeight) - 1u);
            if (OcclusionMask[candidateFull] == 0)
            {
                best = ColorIn[min(uint2(candidate), uint2(fullWidth, fullHeight) - 1)];
                break;
            }
        }
    }
    FilledOut[id.xy] = best;
}

#elif DOF_PASS_PATCH0

Texture2D<float4> ColorIn : register(t0);
Texture2D<float4> CoarseFilled : register(t1);
Texture2D<uint> OcclusionMask : register(t2);
RWTexture2D<float4> FilledOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    FilledOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    if (OcclusionMask[id.xy] == 0)
    {
        FilledOut[id.xy] = ColorIn[id.xy];
        return;
    }

    uint coarseWidth;
    uint coarseHeight;
    CoarseFilled.GetDimensions(coarseWidth, coarseHeight);
    float4 best = CoarseFilled[min(id.xy, uint2(coarseWidth, coarseHeight) - 1)];
    uint2 seed = id.xy;
    [loop]
    for (uint iteration = 0; iteration < 4; ++iteration)
    {
        static const int radii[4] = {8, 4, 2, 1};
        int radius = radii[iteration];
        static const int2 offsets[4] = {int2(-1, 0), int2(1, 0), int2(0, -1), int2(0, 1)};
        [unroll]
        for (uint index = 0; index < 4; ++index)
        {
            const int2 candidate = int2(id.xy) + offsets[index];
            if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
            if (OcclusionMask[candidate] == 0)
            {
                best = ColorIn[candidate];
                radius = max(1, radius / 2);
                break;
            }
        }
        [loop]
        for (int search = 0; search < 4; ++search)
        {
            const int2 randomValue = int2(((seed.x * 1664525u + 1013904223u) / 65536u) & 31u,
                                          ((seed.y * 22695477u + 1u) / 65536u) & 31u);
            seed += randomValue + search;
            const int2 offset = (randomValue % (2 * radius + 1)) - radius;
            const int2 candidate = clamp(int2(id.xy) + offset, int2(0, 0), int2(width, height) - 1);
            if (OcclusionMask[candidate] == 0)
            {
                best = ColorIn[candidate];
                break;
            }
        }
    }
    FilledOut[id.xy] = best;
}

#elif DOF_PASS_WMAX

cbuffer DofWMaxParams : register(b0)
{
    float maxCoC;
    float dofTaps;
};

Texture2D<uint> OcclusionMask : register(t0);
Texture2D<float> CoC : register(t1);
RWTexture2D<float> WMaxOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    WMaxOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    const float radius = min(CoC[id.xy], maxCoC);
    if (radius < 0.5f)
    {
        WMaxOut[id.xy] = 0;
        return;
    }

    const uint hash = (id.x * 1664525u + 1013904223u) ^ (id.y * 22695477u);
    const float angle = float(hash & 0xffffu) * (6.2831853f / 65536.0f);
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    const uint taps = clamp((uint)dofTaps, 4u, 64u);
    uint blocked = 0;
    uint count = 0;
    [loop]
    for (uint tap = 0; tap < taps; ++tap)
    {
        const int2 candidate = int2(id.xy) + int2(round(sunflowerSample(tap, taps, float2(cosine, sine)) * radius));
        if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
        blocked += OcclusionMask[candidate] > 0.5f ? 1u : 0u;
        ++count;
    }
    WMaxOut[id.xy] = count > 0 ? float(blocked) / float(count) : 0;
}

#elif DOF_PASS_WFILTER

cbuffer DofWFilterParams : register(b0)
{
    float filterRadius;
};

Texture2D<float> WMaxIn : register(t0);
RWTexture2D<float> WFilteredOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    WFilteredOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;

    const float center = WMaxIn[id.xy];
    const int radius = max(1, (int)filterRadius);
    const float spatialSigma = max(filterRadius * 0.5f, 1.0f);
    const float invSpatial = 1.0f / (2.0f * spatialSigma * spatialSigma);
    const float invRange = 1.0f / (2.0f * 0.15f * 0.15f);
    float weighted = 0;
    float totalWeight = 0;
    [loop]
    for (int y = -radius; y <= radius; ++y)
    {
        [loop]
        for (int x = -radius; x <= radius; ++x)
        {
            const int2 candidate = int2(id.xy) + int2(x, y);
            if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
            const float sampleWeight = WMaxIn[candidate];
            const float difference = sampleWeight - center;
            const float weight = exp(-float(x * x + y * y) * invSpatial - difference * difference * invRange);
            weighted += sampleWeight * weight;
            totalWeight += weight;
        }
    }
    WFilteredOut[id.xy] = weighted / max(totalWeight, 1e-4f);
}

#elif DOF_PASS_GATHER

cbuffer DofGatherParams : register(b0)
{
    float maxCoC;
    float dofTaps;
};

Texture2D<float4> ColorIn : register(t0);
Texture2D<float4> ColorFilled : register(t1);
Texture2D<float> WFiltered : register(t2);
Texture2D<float> CoC : register(t3);
RWTexture2D<float4> DofOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    DofOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    if (isStatusMarker(id.xy, uint2(width, height)))
    {
        DofOut[id.xy] = float4(0, 1, 0, 1);
        return;
    }

    const float radius = min(CoC[id.xy], maxCoC);
    if (radius < 0.5f)
    {
        DofOut[id.xy] = ColorIn[id.xy];
        return;
    }

    const float occlusionWeight = saturate(WFiltered[id.xy]);
    const uint hash = (id.x * 1664525u + 1013904223u) ^ (id.y * 22695477u);
    const float angle = float(hash & 0xffffu) * (6.2831853f / 65536.0f);
    float sine;
    float cosine;
    sincos(angle, sine, cosine);
    const uint taps = clamp((uint)dofTaps, 4u, 64u);
    float4 accumulated = 0;
    uint count = 0;
    [loop]
    for (uint tap = 0; tap < taps; ++tap)
    {
        const int2 candidate = int2(id.xy) + int2(round(sunflowerSample(tap, taps, float2(cosine, sine)) * radius));
        if (candidate.x < 0 || candidate.y < 0 || candidate.x >= int(width) || candidate.y >= int(height)) continue;
        accumulated += lerp(ColorIn[candidate], ColorFilled[candidate], occlusionWeight);
        ++count;
    }
    // Keep the chain linear. The final pixel pass performs the single sRGB conversion.
    DofOut[id.xy] = count > 0 ? accumulated / float(count) : ColorIn[id.xy];
}

#elif DOF_PASS_COPY

Texture2D<float4> ColorIn : register(t0);
RWTexture2D<float4> DofOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    DofOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    DofOut[id.xy] = isStatusMarker(id.xy, uint2(width, height)) ? float4(1, 0, 0, 1) : ColorIn[id.xy];
}

#elif DOF_PASS_DEBUG

cbuffer DofDebugParams : register(b0)
{
    float focusDistance;
    float focusRange;
    float farZ;
    float autoFocus;
    float maxCoC;
};

Texture2D<float> LinearDepth : register(t0);
Texture2D<float> CoC : register(t1);
Texture2D<float> WFiltered : register(t2);
RWTexture2D<float4> DofOut : register(u0);

[numthreads(THREADS_X, THREADS_Y, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    uint width;
    uint height;
    DofOut.GetDimensions(width, height);
    if (id.x >= width || id.y >= height) return;
    if (isStatusMarker(id.xy, uint2(width, height)))
    {
        DofOut[id.xy] = float4(1, 1, 0, 1);
        return;
    }

    const float focus = activeFocusDistance(LinearDepth, focusDistance, farZ, autoFocus);
    const float linearZ = LinearDepth[id.xy] * farZ;
    const float blur = saturate(CoC[id.xy] / max(maxCoC, 1e-4f));
    const float nearBlur = blur * (linearZ < focus);
    const float farBlur = blur * (linearZ > focus);
    const float focused = 1.0f - blur;
    const float blocked = WFiltered[id.xy];
    DofOut[id.xy] = float4(saturate(nearBlur + blocked), focused, saturate(farBlur + blocked * 0.5f), 1);
}

#endif
