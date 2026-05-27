// Copyright 2024 N-GINN LLC. All rights reserved.
// Copyright (C) 2024  Gaijin Games KFT.  All rights reserved

#ifndef HARDWARE_DEFINES_HEADER
#define HARDWARE_DEFINES_HEADER 1



#if _HARDWARE_METAL
#define BGRA_SWIZZLE(a) a.zyxw
#define BGR_SWIZZLE(a) a.zyx
#endif
#ifndef BGRA_SWIZZLE
#define BGRA_SWIZZLE(a) a
#endif
#ifndef BGR_SWIZZLE
#define BGR_SWIZZLE(a) a
#endif

#ifndef SHADER_COMPILER_HLSL2021
#define SHADER_COMPILER_HLSL2021 1
#endif

#define GPU_TARGET 1
#define PI 3.14159265f
#define PIh 3.14159265h

#define EPSILON 0.000001
#define UNIT_X float3(1.0, 0.0, 0.0)
#define UNIT_Y float3(0.0, 1.0, 0.0)
#define UNIT_Z float3(0.0, 0.0, 1.0)

#define HAS_REVERSEBITS 1
#define SEPARATE_SAMPLER_OBJECT 1
#define double doubles_are_not_supported_by_some_gpus
#define double2 doubles_are_not_supported_by_some_gpus
#define double3 doubles_are_not_supported_by_some_gpus
#define double4 doubles_are_not_supported_by_some_gpus

#define fixed half
#define fixed2 half2
#define fixed3 half3
#define fixed4 half4

float max3(float a, float b, float c)
{
    return max(a, max(b, c));
}
float min3(float a, float b, float c)
{
    return min(a, min(b, c));
}
#define INVARIANT(x) x

float max3(float3 a)
{
    return max3(a.x, a.y, a.z);
}
float max4(float a, float b, float c, float d)
{
    return max(max(a, d), max(b, c));
}
float min4(float a, float b, float c, float d)
{
    return min(min(a, d), min(b, c));
}

#define PRECISE precise
#define SQRT_SAT(x)  sqrt(saturate(x))
#define VS_OUT_POSITION(name) float4 name:SV_POSITION;
#define tex2Dgrad(a, uv, dx, dy) a.SampleGrad(a##_samplerstate, (uv).xy, dx, dy)
#define tex2Dlod(a, uv) a.SampleLevel(a##_samplerstate, (uv).xy, (uv).w)
#define tex3Dlod(a, uv) a.SampleLevel(a##_samplerstate, (uv).xyz, (uv).w)
#define texCUBElod(a, uv) a.SampleLevel(a##_samplerstate, (uv).xyz, uv.w)
#define texCUBEArraylod(a, uv, lod) a.SampleLevel(a##_samplerstate, (uv).xyzw, lod)
#define tex2Dproj(a, uv) a.Sample(a##_samplerstate, (uv).xy/(uv).w)
#define tex2D(a, uv) a.Sample(a##_samplerstate, uv)
#define tex3D(a, uv) a.Sample(a##_samplerstate, uv)
#define texCUBE(a, uv) a.Sample(a##_samplerstate, uv)
#define textureGather(a, tc) a.Gather(a##_samplerstate, tc)
#define texelFetchOffset(a, tc, lod, ofs) a.Load(int3(tc, lod), ofs)
#define textureOffset(a, tc, ofs) a.Sample(a##_samplerstate, tc, ofs)
#define textureLodOffset(a, tc, lod, ofs) a.SampleLevel(a##_samplerstate, tc, lod, ofs)

#define CLAMP_BORDER(a, name, val)
#define LOOP [loop]
#define UNROLL [unroll]
#define BRANCH [branch]
#define FLATTEN [flatten]

#define EMPTY_STRUCT(name) struct name {}
#define DECLARE_UNUSED_MEMBER
#define RETURN_EMPTY_STRUCT(name)
#define INIT_EMPTY_STRUCT(name)

#define HW_USE_SCREEN_POS
#define GET_SCREEN_POS(vs_pos) vs_pos
#ifndef TEXELFETCH_DEFINED
#define TEXELFETCH_DEFINED 1

#define CHECK_TEXTURE2D
#define CHECK_TEXTURE2D_EXPR(a, tc) 0
#define CHECK_TEXTURE2DARRAY
#define CHECK_TEXTURE3D
#define CHECK_BUFFER(file, ln, name)
#define CHECK_BUFFER_EXPR(a, tc) 0
#define CHECK_STRUCTURED_BUFFER(file, ln, name)
#define CHECK_STRUCTURED_BUFFER_EXPR(a, tc) 0
#define CHECK_STENCIL
float4 texelFetchBase(Texture2D<float4> a, int2 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2D;
    return a.Load(int3(tc, lod));
}
float3 texelFetchBase(Texture2D<float3> a, int2 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2D;
    return a.Load(int3(tc, lod));
}
float2 texelFetchBase(Texture2D<float2> a, int2 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2D;
    return a.Load(int3(tc, lod));
}
float texelFetchBase(Texture2D<float> a, int2 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2D;
    return a.Load(int3(tc, lod));
}
float4 texelFetchBase(Texture2DArray<float4> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2DARRAY;
    return a.Load(int4(tc, lod));
}
float3 texelFetchBase(Texture2DArray<float3> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2DARRAY;
    return a.Load(int4(tc, lod));
}
float2 texelFetchBase(Texture2DArray<float2> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2DARRAY;
    return a.Load(int4(tc, lod));
}
float texelFetchBase(Texture2DArray<float> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE2DARRAY;
    return a.Load(int4(tc, lod));
}
float4 texelFetchBase(Texture3D<float4> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE3D;
    return a.Load(int4(tc, lod));
}
float3 texelFetchBase(Texture3D<float3> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE3D;
    return a.Load(int4(tc, lod));
}
float2 texelFetchBase(Texture3D<float2> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE3D;
    return a.Load(int4(tc, lod));
}
float texelFetchBase(Texture3D<float> a, int3 tc, int lod, int file, int ln, int name)
{ CHECK_TEXTURE3D;
    return a.Load(int4(tc, lod));
}
#define texelFetch(a, tc, lod) texelFetchBase(a, tc, lod, _FILE_, __LINE__, -1)

float4 loadBufferBase(Buffer<float4> a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a[tc];
}
float3 loadBufferBase(Buffer<float3> a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a[tc];
}
float2 loadBufferBase(Buffer<float2> a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a[tc];
}
float loadBufferBase(Buffer<float> a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a[tc];
}
uint loadBufferBase(Buffer<uint> a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a[tc];
}
uint loadBufferBase(StructuredBuffer<uint> a, int tc, int file, int ln, int name)
{ CHECK_STRUCTURED_BUFFER(file, ln, name);
    return a[tc];
}
uint loadBufferBase(RWStructuredBuffer<uint> a, int tc, int file, int ln, int name)
{ CHECK_STRUCTURED_BUFFER(file, ln, name);
    return a[tc];
}
uint loadBufferBase(ByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load(tc);
}
uint2 loadBuffer2Base(ByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load2(tc);
}
uint3 loadBuffer3Base(ByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load3(tc);
}
uint4 loadBuffer4Base(ByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load4(tc);
}
uint loadBufferBase(RWByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load(tc);
}
uint2 loadBuffer2Base(RWByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load2(tc);
}
uint3 loadBuffer3Base(RWByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load3(tc);
}
uint4 loadBuffer4Base(RWByteAddressBuffer a, int tc, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    return a.Load4(tc);
}
#define loadBuffer(a, tc) loadBufferBase(a, tc, _FILE_, __LINE__, -1)
#define loadBuffer2(a, tc) loadBuffer2Base(a, tc, _FILE_, __LINE__, -1)
#define loadBuffer3(a, tc) loadBuffer3Base(a, tc, _FILE_, __LINE__, get_name_##a)
#define loadBuffer4(a, tc) loadBuffer4Base(a, tc, _FILE_, __LINE__, -1)
void storeBufferBase(RWByteAddressBuffer a, int tc, uint value, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    a.Store(tc, value);
}
void storeBuffer2Base(RWByteAddressBuffer a, int tc, uint2 value, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    a.Store2(tc, value);
}
void storeBuffer3Base(RWByteAddressBuffer a, int tc, uint3 value, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    a.Store3(tc, value);
}
void storeBuffer4Base(RWByteAddressBuffer a, int tc, uint4 value, int file, int ln, int name)
{ CHECK_BUFFER(file, ln, name);
    a.Store4(tc, value);
}
#define storeBuffer(a, tc, value) storeBufferBase(a, tc, value, _FILE_, __LINE__, -1)
#define storeBuffer2(a, tc, value) storeBuffer2Base(a, tc, value, _FILE_, __LINE__, get_name_##a)
#define storeBuffer3(a, tc, value) storeBuffer3Base(a, tc, value, _FILE_, __LINE__, -1)
#define storeBuffer4(a, tc, value) storeBuffer4Base(a, tc, value, _FILE_, __LINE__, -1)
#define structuredBufferAt(a, tc) a[uint((CHECK_STRUCTURED_BUFFER_EXPR(a, tc), tc))]
#define bufferAt(a, tc) a[uint((CHECK_BUFFER_EXPR(a, tc), tc))]
#define texture2DAt(a, tc) a[int2((CHECK_TEXTURE2D_EXPR(a, tc), tc))]
#endif

uint stencilFetchBase(Texture2D<uint2> a, int2 tc, int file, int ln, int name)
{ 
    CHECK_STENCIL;
    return a[tc].g;
}
#define stencilFetch(a, tc) stencilFetchBase(a, tc, _FILE_, __LINE__, get_name_##a)

#if !SHADER_COMPILER_HLSL2021
#define select(a, b, c) a ? b : c
#define or(a, b) a || b
#endif

half3 h3nanofilter(half3 val)
{
#if HALF_PRECISION
    return min(val, 65504.h);
#else
    return select(isfinite(dot(val, val)).xxx, val, half3(0, 0, 0));
#endif
}

#endif