//*********************************************************
//
// Copyright (c) Microsoft. All rights reserved.
// This code is licensed under the MIT License (MIT).
// THIS CODE IS PROVIDED *AS IS* WITHOUT WARRANTY OF
// ANY KIND, EITHER EXPRESS OR IMPLIED, INCLUDING ANY
// IMPLIED WARRANTIES OF FITNESS FOR A PARTICULAR
// PURPOSE, MERCHANTABILITY, OR NON-INFRINGEMENT.
//
//*********************************************************

#ifndef __D3DX12_H__
#define __D3DX12_H__

#include "d3d12.h"

#if defined( __cplusplus )

struct CD3DX12_DEFAULT
{
};

extern const DECLSPEC_SELECTANY CD3DX12_DEFAULT D3D12_DEFAULT;

//------------------------------------------------------------------------------------------------
inline bool operator==(const D3D12_VIEWPORT& l, const D3D12_VIEWPORT& r) noexcept
{
    return l.TopLeftX == r.TopLeftX && l.TopLeftY == r.TopLeftY && l.Width == r.Width &&
        l.Height == r.Height && l.MinDepth == r.MinDepth && l.MaxDepth == r.MaxDepth;
}

//------------------------------------------------------------------------------------------------
inline bool operator!=(const D3D12_VIEWPORT& l, const D3D12_VIEWPORT& r) noexcept
{
    return !(l == r);
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_RECT : D3D12_RECT
{
    CD3DX12_RECT() = default;

    explicit CD3DX12_RECT(const D3D12_RECT& o) noexcept :
        D3D12_RECT(o)
    {
    }

    explicit CD3DX12_RECT(
        const LONG Left,
        const LONG Top,
        const LONG Right,
        const LONG Bottom) noexcept
    {
        left = Left;
        top = Top;
        right = Right;
        bottom = Bottom;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_VIEWPORT : D3D12_VIEWPORT
{
    CD3DX12_VIEWPORT() = default;

    explicit CD3DX12_VIEWPORT(const D3D12_VIEWPORT& o) noexcept :
        D3D12_VIEWPORT(o)
    {
    }

    explicit CD3DX12_VIEWPORT(
        const FLOAT topLeftX,
        const FLOAT topLeftY,
        const FLOAT width,
        const FLOAT height,
        const FLOAT minDepth = D3D12_MIN_DEPTH,
        const FLOAT maxDepth = D3D12_MAX_DEPTH) noexcept
    {
        TopLeftX = topLeftX;
        TopLeftY = topLeftY;
        Width = width;
        Height = height;
        MinDepth = minDepth;
        MaxDepth = maxDepth;
    }

    explicit CD3DX12_VIEWPORT(
        _In_ ID3D12Resource* pResource,
        const UINT mipSlice = 0,
        const FLOAT topLeftX = 0.0f,
        const FLOAT topLeftY = 0.0f,
        const FLOAT minDepth = D3D12_MIN_DEPTH,
        const FLOAT maxDepth = D3D12_MAX_DEPTH) noexcept
    {
        auto Desc = pResource->GetDesc();
        const UINT64 SubresourceWidth = Desc.Width >> mipSlice;
        const UINT64 SubresourceHeight = Desc.Height >> mipSlice;
        switch (Desc.Dimension)
        {
        case D3D12_RESOURCE_DIMENSION_BUFFER:
            TopLeftX = topLeftX;
            TopLeftY = 0.0f;
            Width = static_cast<float>(Desc.Width) - topLeftX;
            Height = 1.0f;
            break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE1D:
            TopLeftX = topLeftX;
            TopLeftY = 0.0f;
            Width = (SubresourceWidth ? static_cast<float>(SubresourceWidth) : 1.0f) - topLeftX;
            Height = 1.0f;
            break;
        case D3D12_RESOURCE_DIMENSION_TEXTURE2D:
        case D3D12_RESOURCE_DIMENSION_TEXTURE3D:
            TopLeftX = topLeftX;
            TopLeftY = topLeftY;
            Width = (SubresourceWidth ? static_cast<float>(SubresourceWidth) : 1.0f) - topLeftX;
            Height = (SubresourceHeight ? static_cast<float>(SubresourceHeight) : 1.0f) - topLeftY;
            break;
        default: break;
        }

        MinDepth = minDepth;
        MaxDepth = maxDepth;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_BOX : D3D12_BOX
{
    CD3DX12_BOX() = default;

    explicit CD3DX12_BOX(const D3D12_BOX& o) noexcept :
        D3D12_BOX(o)
    {
    }

    explicit CD3DX12_BOX(
        const LONG Left,
        const LONG Right) noexcept
    {
        left = static_cast<UINT>(Left);
        top = 0;
        front = 0;
        right = static_cast<UINT>(Right);
        bottom = 1;
        back = 1;
    }

    explicit CD3DX12_BOX(
        const LONG Left,
        const LONG Top,
        const LONG Right,
        const LONG Bottom) noexcept
    {
        left = static_cast<UINT>(Left);
        top = static_cast<UINT>(Top);
        front = 0;
        right = static_cast<UINT>(Right);
        bottom = static_cast<UINT>(Bottom);
        back = 1;
    }

    explicit CD3DX12_BOX(
        const LONG Left,
        const LONG Top,
        const LONG Front,
        const LONG Right,
        const LONG Bottom,
        const LONG Back) noexcept
    {
        left = static_cast<UINT>(Left);
        top = static_cast<UINT>(Top);
        front = static_cast<UINT>(Front);
        right = static_cast<UINT>(Right);
        bottom = static_cast<UINT>(Bottom);
        back = static_cast<UINT>(Back);
    }
};

inline bool operator==(const D3D12_BOX& l, const D3D12_BOX& r) noexcept
{
    return l.left == r.left && l.top == r.top && l.front == r.front &&
        l.right == r.right && l.bottom == r.bottom && l.back == r.back;
}

inline bool operator!=(const D3D12_BOX& l, const D3D12_BOX& r) noexcept
{
    return !(l == r);
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_DEPTH_STENCIL_DESC : D3D12_DEPTH_STENCIL_DESC
{
    CD3DX12_DEPTH_STENCIL_DESC() = default;

    explicit CD3DX12_DEPTH_STENCIL_DESC(const D3D12_DEPTH_STENCIL_DESC& o) noexcept :
        D3D12_DEPTH_STENCIL_DESC(o)
    {
    }

    explicit CD3DX12_DEPTH_STENCIL_DESC(CD3DX12_DEFAULT) noexcept
    {
        DepthEnable = TRUE;
        DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        StencilEnable = FALSE;
        StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        constexpr D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
            {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
        FrontFace = defaultStencilOp;
        BackFace = defaultStencilOp;
    }

    explicit CD3DX12_DEPTH_STENCIL_DESC(
        const BOOL depthEnable,
        const D3D12_DEPTH_WRITE_MASK depthWriteMask,
        const D3D12_COMPARISON_FUNC depthFunc,
        const BOOL stencilEnable,
        const UINT8 stencilReadMask,
        const UINT8 stencilWriteMask,
        const D3D12_STENCIL_OP frontStencilFailOp,
        const D3D12_STENCIL_OP frontStencilDepthFailOp,
        const D3D12_STENCIL_OP frontStencilPassOp,
        const D3D12_COMPARISON_FUNC frontStencilFunc,
        const D3D12_STENCIL_OP backStencilFailOp,
        const D3D12_STENCIL_OP backStencilDepthFailOp,
        const D3D12_STENCIL_OP backStencilPassOp,
        const D3D12_COMPARISON_FUNC backStencilFunc) noexcept
    {
        DepthEnable = depthEnable;
        DepthWriteMask = depthWriteMask;
        DepthFunc = depthFunc;
        StencilEnable = stencilEnable;
        StencilReadMask = stencilReadMask;
        StencilWriteMask = stencilWriteMask;
        FrontFace.StencilFailOp = frontStencilFailOp;
        FrontFace.StencilDepthFailOp = frontStencilDepthFailOp;
        FrontFace.StencilPassOp = frontStencilPassOp;
        FrontFace.StencilFunc = frontStencilFunc;
        BackFace.StencilFailOp = backStencilFailOp;
        BackFace.StencilDepthFailOp = backStencilDepthFailOp;
        BackFace.StencilPassOp = backStencilPassOp;
        BackFace.StencilFunc = backStencilFunc;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_DEPTH_STENCIL_DESC1 : D3D12_DEPTH_STENCIL_DESC1
{
    CD3DX12_DEPTH_STENCIL_DESC1() = default;

    explicit CD3DX12_DEPTH_STENCIL_DESC1(const D3D12_DEPTH_STENCIL_DESC1& o) noexcept :
        D3D12_DEPTH_STENCIL_DESC1(o)
    {
    }

    explicit CD3DX12_DEPTH_STENCIL_DESC1(const D3D12_DEPTH_STENCIL_DESC& o) noexcept
    {
        DepthEnable = o.DepthEnable;
        DepthWriteMask = o.DepthWriteMask;
        DepthFunc = o.DepthFunc;
        StencilEnable = o.StencilEnable;
        StencilReadMask = o.StencilReadMask;
        StencilWriteMask = o.StencilWriteMask;
        FrontFace.StencilFailOp = o.FrontFace.StencilFailOp;
        FrontFace.StencilDepthFailOp = o.FrontFace.StencilDepthFailOp;
        FrontFace.StencilPassOp = o.FrontFace.StencilPassOp;
        FrontFace.StencilFunc = o.FrontFace.StencilFunc;
        BackFace.StencilFailOp = o.BackFace.StencilFailOp;
        BackFace.StencilDepthFailOp = o.BackFace.StencilDepthFailOp;
        BackFace.StencilPassOp = o.BackFace.StencilPassOp;
        BackFace.StencilFunc = o.BackFace.StencilFunc;
        DepthBoundsTestEnable = FALSE;
    }

    explicit CD3DX12_DEPTH_STENCIL_DESC1(CD3DX12_DEFAULT) noexcept
    {
        DepthEnable = TRUE;
        DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        DepthFunc = D3D12_COMPARISON_FUNC_LESS;
        StencilEnable = FALSE;
        StencilReadMask = D3D12_DEFAULT_STENCIL_READ_MASK;
        StencilWriteMask = D3D12_DEFAULT_STENCIL_WRITE_MASK;
        constexpr D3D12_DEPTH_STENCILOP_DESC defaultStencilOp =
            {D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_STENCIL_OP_KEEP, D3D12_COMPARISON_FUNC_ALWAYS};
        FrontFace = defaultStencilOp;
        BackFace = defaultStencilOp;
        DepthBoundsTestEnable = FALSE;
    }

    explicit CD3DX12_DEPTH_STENCIL_DESC1(
        const BOOL depthEnable,
        const D3D12_DEPTH_WRITE_MASK depthWriteMask,
        const D3D12_COMPARISON_FUNC depthFunc,
        const BOOL stencilEnable,
        const UINT8 stencilReadMask,
        const UINT8 stencilWriteMask,
        const D3D12_STENCIL_OP frontStencilFailOp,
        const D3D12_STENCIL_OP frontStencilDepthFailOp,
        const D3D12_STENCIL_OP frontStencilPassOp,
        const D3D12_COMPARISON_FUNC frontStencilFunc,
        const D3D12_STENCIL_OP backStencilFailOp,
        const D3D12_STENCIL_OP backStencilDepthFailOp,
        const D3D12_STENCIL_OP backStencilPassOp,
        const D3D12_COMPARISON_FUNC backStencilFunc,
        const BOOL depthBoundsTestEnable) noexcept
    {
        DepthEnable = depthEnable;
        DepthWriteMask = depthWriteMask;
        DepthFunc = depthFunc;
        StencilEnable = stencilEnable;
        StencilReadMask = stencilReadMask;
        StencilWriteMask = stencilWriteMask;
        FrontFace.StencilFailOp = frontStencilFailOp;
        FrontFace.StencilDepthFailOp = frontStencilDepthFailOp;
        FrontFace.StencilPassOp = frontStencilPassOp;
        FrontFace.StencilFunc = frontStencilFunc;
        BackFace.StencilFailOp = backStencilFailOp;
        BackFace.StencilDepthFailOp = backStencilDepthFailOp;
        BackFace.StencilPassOp = backStencilPassOp;
        BackFace.StencilFunc = backStencilFunc;
        DepthBoundsTestEnable = depthBoundsTestEnable;
    }

    operator D3D12_DEPTH_STENCIL_DESC() const noexcept
    {
        D3D12_DEPTH_STENCIL_DESC D;
        D.DepthEnable = DepthEnable;
        D.DepthWriteMask = DepthWriteMask;
        D.DepthFunc = DepthFunc;
        D.StencilEnable = StencilEnable;
        D.StencilReadMask = StencilReadMask;
        D.StencilWriteMask = StencilWriteMask;
        D.FrontFace.StencilFailOp = FrontFace.StencilFailOp;
        D.FrontFace.StencilDepthFailOp = FrontFace.StencilDepthFailOp;
        D.FrontFace.StencilPassOp = FrontFace.StencilPassOp;
        D.FrontFace.StencilFunc = FrontFace.StencilFunc;
        D.BackFace.StencilFailOp = BackFace.StencilFailOp;
        D.BackFace.StencilDepthFailOp = BackFace.StencilDepthFailOp;
        D.BackFace.StencilPassOp = BackFace.StencilPassOp;
        D.BackFace.StencilFunc = BackFace.StencilFunc;
        return D;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_BLEND_DESC : D3D12_BLEND_DESC
{
    CD3DX12_BLEND_DESC() = default;

    explicit CD3DX12_BLEND_DESC(const D3D12_BLEND_DESC& o) noexcept :
        D3D12_BLEND_DESC(o)
    {
    }

    explicit CD3DX12_BLEND_DESC(CD3DX12_DEFAULT) noexcept
    {
        AlphaToCoverageEnable = FALSE;
        IndependentBlendEnable = FALSE;
        constexpr D3D12_RENDER_TARGET_BLEND_DESC defaultRenderTargetBlendDesc =
        {
            FALSE,FALSE,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
            D3D12_LOGIC_OP_NOOP,
            D3D12_COLOR_WRITE_ENABLE_ALL,
        };
        for (UINT i = 0; i < D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT; ++i)
            RenderTarget[i] = defaultRenderTargetBlendDesc;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_RASTERIZER_DESC : D3D12_RASTERIZER_DESC
{
    CD3DX12_RASTERIZER_DESC() = default;

    explicit CD3DX12_RASTERIZER_DESC(const D3D12_RASTERIZER_DESC& o) noexcept :
        D3D12_RASTERIZER_DESC(o)
    {
    }

    explicit CD3DX12_RASTERIZER_DESC(CD3DX12_DEFAULT) noexcept
    {
        FillMode = D3D12_FILL_MODE_SOLID;
        CullMode = D3D12_CULL_MODE_BACK;
        FrontCounterClockwise = FALSE;
        DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
        DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
        SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
        DepthClipEnable = TRUE;
        MultisampleEnable = FALSE;
        AntialiasedLineEnable = FALSE;
        ForcedSampleCount = 0;
        ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;
    }

    explicit CD3DX12_RASTERIZER_DESC(
        const D3D12_FILL_MODE fillMode,
        const D3D12_CULL_MODE cullMode,
        const BOOL frontCounterClockwise,
        const INT depthBias,
        const FLOAT depthBiasClamp,
        const FLOAT slopeScaledDepthBias,
        const BOOL depthClipEnable,
        const BOOL multisampleEnable,
        const BOOL antialiasedLineEnable,
        const UINT forcedSampleCount,
        const D3D12_CONSERVATIVE_RASTERIZATION_MODE conservativeRaster) noexcept
    {
        FillMode = fillMode;
        CullMode = cullMode;
        FrontCounterClockwise = frontCounterClockwise;
        DepthBias = depthBias;
        DepthBiasClamp = depthBiasClamp;
        SlopeScaledDepthBias = slopeScaledDepthBias;
        DepthClipEnable = depthClipEnable;
        MultisampleEnable = multisampleEnable;
        AntialiasedLineEnable = antialiasedLineEnable;
        ForcedSampleCount = forcedSampleCount;
        ConservativeRaster = conservativeRaster;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_RESOURCE_ALLOCATION_INFO : D3D12_RESOURCE_ALLOCATION_INFO
{
    CD3DX12_RESOURCE_ALLOCATION_INFO() = default;

    explicit CD3DX12_RESOURCE_ALLOCATION_INFO(const D3D12_RESOURCE_ALLOCATION_INFO& o) noexcept :
        D3D12_RESOURCE_ALLOCATION_INFO(o)
    {
    }

    CD3DX12_RESOURCE_ALLOCATION_INFO(
        const UINT64 size,
        const UINT64 alignment) noexcept
    {
        SizeInBytes = size;
        Alignment = alignment;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_HEAP_PROPERTIES : D3D12_HEAP_PROPERTIES
{
    CD3DX12_HEAP_PROPERTIES() = default;

    explicit CD3DX12_HEAP_PROPERTIES(const D3D12_HEAP_PROPERTIES& o) noexcept :
        D3D12_HEAP_PROPERTIES(o)
    {
    }

    CD3DX12_HEAP_PROPERTIES(
        const D3D12_CPU_PAGE_PROPERTY cpuPageProperty,
        const D3D12_MEMORY_POOL memoryPoolPreference,
        const UINT creationNodeMask = 1,
        const UINT nodeMask = 1) noexcept
    {
        Type = D3D12_HEAP_TYPE_CUSTOM;
        CPUPageProperty = cpuPageProperty;
        MemoryPoolPreference = memoryPoolPreference;
        CreationNodeMask = creationNodeMask;
        VisibleNodeMask = nodeMask;
    }

    explicit CD3DX12_HEAP_PROPERTIES(
        const D3D12_HEAP_TYPE type,
        const UINT creationNodeMask = 1,
        const UINT nodeMask = 1) noexcept
    {
        Type = type;
        CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        CreationNodeMask = creationNodeMask;
        VisibleNodeMask = nodeMask;
    }

    bool IsCPUAccessible() const noexcept
    {
        return Type == D3D12_HEAP_TYPE_UPLOAD || Type == D3D12_HEAP_TYPE_READBACK || (Type == D3D12_HEAP_TYPE_CUSTOM &&
            (CPUPageProperty == D3D12_CPU_PAGE_PROPERTY_WRITE_COMBINE || CPUPageProperty ==
                D3D12_CPU_PAGE_PROPERTY_WRITE_BACK));
    }
};

inline bool operator==(const D3D12_HEAP_PROPERTIES& l, const D3D12_HEAP_PROPERTIES& r) noexcept
{
    return l.Type == r.Type && l.CPUPageProperty == r.CPUPageProperty &&
        l.MemoryPoolPreference == r.MemoryPoolPreference &&
        l.CreationNodeMask == r.CreationNodeMask &&
        l.VisibleNodeMask == r.VisibleNodeMask;
}

inline bool operator!=(const D3D12_HEAP_PROPERTIES& l, const D3D12_HEAP_PROPERTIES& r) noexcept
{
    return !(l == r);
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_HEAP_DESC : D3D12_HEAP_DESC
{
    CD3DX12_HEAP_DESC() = default;

    explicit CD3DX12_HEAP_DESC(const D3D12_HEAP_DESC& o) noexcept :
        D3D12_HEAP_DESC(o)
    {
    }

    CD3DX12_HEAP_DESC(
        const UINT64 size,
        const D3D12_HEAP_PROPERTIES& properties,
        const UINT64 alignment = 0,
        const D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) noexcept
    {
        SizeInBytes = size;
        Properties = properties;
        Alignment = alignment;
        Flags = flags;
    }

    CD3DX12_HEAP_DESC(
        const UINT64 size,
        const D3D12_HEAP_TYPE type,
        const UINT64 alignment = 0,
        const D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) noexcept
    {
        SizeInBytes = size;
        Properties = CD3DX12_HEAP_PROPERTIES(type);
        Alignment = alignment;
        Flags = flags;
    }

    CD3DX12_HEAP_DESC(
        const UINT64 size,
        const D3D12_CPU_PAGE_PROPERTY cpuPageProperty,
        const D3D12_MEMORY_POOL memoryPoolPreference,
        const UINT64 alignment = 0,
        const D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) noexcept
    {
        SizeInBytes = size;
        Properties = CD3DX12_HEAP_PROPERTIES(cpuPageProperty, memoryPoolPreference);
        Alignment = alignment;
        Flags = flags;
    }

    CD3DX12_HEAP_DESC(
        const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
        const D3D12_HEAP_PROPERTIES& properties,
        const D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) noexcept
    {
        SizeInBytes = resAllocInfo.SizeInBytes;
        Properties = properties;
        Alignment = resAllocInfo.Alignment;
        Flags = flags;
    }

    CD3DX12_HEAP_DESC(
        const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
        const D3D12_HEAP_TYPE type,
        const D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) noexcept
    {
        SizeInBytes = resAllocInfo.SizeInBytes;
        Properties = CD3DX12_HEAP_PROPERTIES(type);
        Alignment = resAllocInfo.Alignment;
        Flags = flags;
    }

    CD3DX12_HEAP_DESC(
        const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
        const D3D12_CPU_PAGE_PROPERTY cpuPageProperty,
        const D3D12_MEMORY_POOL memoryPoolPreference,
        const D3D12_HEAP_FLAGS flags = D3D12_HEAP_FLAG_NONE) noexcept
    {
        SizeInBytes = resAllocInfo.SizeInBytes;
        Properties = CD3DX12_HEAP_PROPERTIES(cpuPageProperty, memoryPoolPreference);
        Alignment = resAllocInfo.Alignment;
        Flags = flags;
    }

    bool IsCPUAccessible() const noexcept
    {
        return static_cast<const CD3DX12_HEAP_PROPERTIES*>(&Properties)->IsCPUAccessible();
    }
};

inline bool operator==(const D3D12_HEAP_DESC& l, const D3D12_HEAP_DESC& r) noexcept
{
    return l.SizeInBytes == r.SizeInBytes &&
        l.Properties == r.Properties &&
        l.Alignment == r.Alignment &&
        l.Flags == r.Flags;
}

inline bool operator!=(const D3D12_HEAP_DESC& l, const D3D12_HEAP_DESC& r) noexcept
{
    return !(l == r);
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_CLEAR_VALUE : D3D12_CLEAR_VALUE
{
    CD3DX12_CLEAR_VALUE() = default;

    explicit CD3DX12_CLEAR_VALUE(const D3D12_CLEAR_VALUE& o) noexcept :
        D3D12_CLEAR_VALUE(o)
    {
    }

    CD3DX12_CLEAR_VALUE(
        const DXGI_FORMAT format,
        const FLOAT color[4]) noexcept
    {
        Format = format;
        memcpy(Color, color, sizeof(Color));
    }

    CD3DX12_CLEAR_VALUE(
        const DXGI_FORMAT format,
        const FLOAT depth,
        const UINT8 stencil) noexcept
    {
        Format = format;
        memset(&Color, 0, sizeof(Color));
        /* Use memcpy to preserve NAN values */
        memcpy(&DepthStencil.Depth, &depth, sizeof(depth));
        DepthStencil.Stencil = stencil;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_RANGE : D3D12_RANGE
{
    CD3DX12_RANGE() = default;

    explicit CD3DX12_RANGE(const D3D12_RANGE& o) noexcept :
        D3D12_RANGE(o)
    {
    }

    CD3DX12_RANGE(
        const SIZE_T begin,
        const SIZE_T end) noexcept
    {
        Begin = begin;
        End = end;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_RANGE_UINT64 : D3D12_RANGE_UINT64
{
    CD3DX12_RANGE_UINT64() = default;

    explicit CD3DX12_RANGE_UINT64(const D3D12_RANGE_UINT64& o) noexcept :
        D3D12_RANGE_UINT64(o)
    {
    }

    CD3DX12_RANGE_UINT64(
        const UINT64 begin,
        const UINT64 end) noexcept
    {
        Begin = begin;
        End = end;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_SUBRESOURCE_RANGE_UINT64 : D3D12_SUBRESOURCE_RANGE_UINT64
{
    CD3DX12_SUBRESOURCE_RANGE_UINT64() = default;

    explicit CD3DX12_SUBRESOURCE_RANGE_UINT64(const D3D12_SUBRESOURCE_RANGE_UINT64& o) noexcept :
        D3D12_SUBRESOURCE_RANGE_UINT64(o)
    {
    }

    CD3DX12_SUBRESOURCE_RANGE_UINT64(
        const UINT subresource,
        const D3D12_RANGE_UINT64& range) noexcept
    {
        Subresource = subresource;
        Range = range;
    }

    CD3DX12_SUBRESOURCE_RANGE_UINT64(
        const UINT subresource,
        const UINT64 begin,
        const UINT64 end) noexcept
    {
        Subresource = subresource;
        Range.Begin = begin;
        Range.End = end;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_SHADER_BYTECODE : D3D12_SHADER_BYTECODE
{
    CD3DX12_SHADER_BYTECODE() = default;

    explicit CD3DX12_SHADER_BYTECODE(const D3D12_SHADER_BYTECODE& o) noexcept :
        D3D12_SHADER_BYTECODE(o)
    {
    }

    CD3DX12_SHADER_BYTECODE(
        _In_ ID3DBlob* pShaderBlob) noexcept
    {
        pShaderBytecode = pShaderBlob->GetBufferPointer();
        BytecodeLength = pShaderBlob->GetBufferSize();
    }

    CD3DX12_SHADER_BYTECODE(
        const void* _pShaderBytecode,
        const SIZE_T bytecodeLength) noexcept
    {
        pShaderBytecode = _pShaderBytecode;
        BytecodeLength = bytecodeLength;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_TILED_RESOURCE_COORDINATE : D3D12_TILED_RESOURCE_COORDINATE
{
    CD3DX12_TILED_RESOURCE_COORDINATE() = default;

    explicit CD3DX12_TILED_RESOURCE_COORDINATE(const D3D12_TILED_RESOURCE_COORDINATE& o) noexcept :
        D3D12_TILED_RESOURCE_COORDINATE(o)
    {
    }

    CD3DX12_TILED_RESOURCE_COORDINATE(
        const UINT x,
        const UINT y,
        const UINT z,
        const UINT subresource) noexcept
    {
        X = x;
        Y = y;
        Z = z;
        Subresource = subresource;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_TILE_REGION_SIZE : D3D12_TILE_REGION_SIZE
{
    CD3DX12_TILE_REGION_SIZE() = default;

    explicit CD3DX12_TILE_REGION_SIZE(const D3D12_TILE_REGION_SIZE& o) noexcept :
        D3D12_TILE_REGION_SIZE(o)
    {
    }

    CD3DX12_TILE_REGION_SIZE(
        const UINT numTiles,
        const BOOL useBox,
        const UINT width,
        const UINT16 height,
        const UINT16 depth) noexcept
    {
        NumTiles = numTiles;
        UseBox = useBox;
        Width = width;
        Height = height;
        Depth = depth;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_SUBRESOURCE_TILING : D3D12_SUBRESOURCE_TILING
{
    CD3DX12_SUBRESOURCE_TILING() = default;

    explicit CD3DX12_SUBRESOURCE_TILING(const D3D12_SUBRESOURCE_TILING& o) noexcept :
        D3D12_SUBRESOURCE_TILING(o)
    {
    }

    CD3DX12_SUBRESOURCE_TILING(
        const UINT widthInTiles,
        const UINT16 heightInTiles,
        const UINT16 depthInTiles,
        const UINT startTileIndexInOverallResource) noexcept
    {
        WidthInTiles = widthInTiles;
        HeightInTiles = heightInTiles;
        DepthInTiles = depthInTiles;
        StartTileIndexInOverallResource = startTileIndexInOverallResource;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_TILE_SHAPE : D3D12_TILE_SHAPE
{
    CD3DX12_TILE_SHAPE() = default;

    explicit CD3DX12_TILE_SHAPE(const D3D12_TILE_SHAPE& o) noexcept :
        D3D12_TILE_SHAPE(o)
    {
    }

    CD3DX12_TILE_SHAPE(
        const UINT widthInTexels,
        const UINT heightInTexels,
        const UINT depthInTexels) noexcept
    {
        WidthInTexels = widthInTexels;
        HeightInTexels = heightInTexels;
        DepthInTexels = depthInTexels;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_RESOURCE_BARRIER : D3D12_RESOURCE_BARRIER
{
    CD3DX12_RESOURCE_BARRIER() = default;

    explicit CD3DX12_RESOURCE_BARRIER(const D3D12_RESOURCE_BARRIER& o) noexcept :
        D3D12_RESOURCE_BARRIER(o)
    {
    }

    static CD3DX12_RESOURCE_BARRIER Transition(
        _In_ ID3D12Resource* pResource,
        const D3D12_RESOURCE_STATES stateBefore,
        const D3D12_RESOURCE_STATES stateAfter,
        const UINT subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,
        const D3D12_RESOURCE_BARRIER_FLAGS flags = D3D12_RESOURCE_BARRIER_FLAG_NONE) noexcept
    {
        CD3DX12_RESOURCE_BARRIER result = {};
        D3D12_RESOURCE_BARRIER& barrier = result;
        result.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        result.Flags = flags;
        barrier.Transition.pResource = pResource;
        barrier.Transition.StateBefore = stateBefore;
        barrier.Transition.StateAfter = stateAfter;
        barrier.Transition.Subresource = subresource;
        return result;
    }

    static CD3DX12_RESOURCE_BARRIER Aliasing(
        _In_ ID3D12Resource* pResourceBefore,
        _In_ ID3D12Resource* pResourceAfter) noexcept
    {
        CD3DX12_RESOURCE_BARRIER result = {};
        D3D12_RESOURCE_BARRIER& barrier = result;
        result.Type = D3D12_RESOURCE_BARRIER_TYPE_ALIASING;
        barrier.Aliasing.pResourceBefore = pResourceBefore;
        barrier.Aliasing.pResourceAfter = pResourceAfter;
        return result;
    }

    static CD3DX12_RESOURCE_BARRIER UAV(
        _In_ ID3D12Resource* pResource) noexcept
    {
        CD3DX12_RESOURCE_BARRIER result = {};
        D3D12_RESOURCE_BARRIER& barrier = result;
        result.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
        barrier.UAV.pResource = pResource;
        return result;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_PACKED_MIP_INFO : D3D12_PACKED_MIP_INFO
{
    CD3DX12_PACKED_MIP_INFO() = default;

    explicit CD3DX12_PACKED_MIP_INFO(const D3D12_PACKED_MIP_INFO& o) noexcept :
        D3D12_PACKED_MIP_INFO(o)
    {
    }

    CD3DX12_PACKED_MIP_INFO(
        const UINT8 numStandardMips,
        const UINT8 numPackedMips,
        const UINT numTilesForPackedMips,
        const UINT startTileIndexInOverallResource) noexcept
    {
        NumStandardMips = numStandardMips;
        NumPackedMips = numPackedMips;
        NumTilesForPackedMips = numTilesForPackedMips;
        StartTileIndexInOverallResource = startTileIndexInOverallResource;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_SUBRESOURCE_FOOTPRINT : D3D12_SUBRESOURCE_FOOTPRINT
{
    CD3DX12_SUBRESOURCE_FOOTPRINT() = default;

    explicit CD3DX12_SUBRESOURCE_FOOTPRINT(const D3D12_SUBRESOURCE_FOOTPRINT& o) noexcept :
        D3D12_SUBRESOURCE_FOOTPRINT(o)
    {
    }

    CD3DX12_SUBRESOURCE_FOOTPRINT(
        const DXGI_FORMAT format,
        const UINT width,
        const UINT height,
        const UINT depth,
        const UINT rowPitch) noexcept
    {
        Format = format;
        Width = width;
        Height = height;
        Depth = depth;
        RowPitch = rowPitch;
    }

    explicit CD3DX12_SUBRESOURCE_FOOTPRINT(
        const D3D12_RESOURCE_DESC& resDesc,
        const UINT rowPitch) noexcept
    {
        Format = resDesc.Format;
        Width = static_cast<UINT>(resDesc.Width);
        Height = resDesc.Height;
        Depth = (resDesc.Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? resDesc.DepthOrArraySize : 1);
        RowPitch = rowPitch;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_TEXTURE_COPY_LOCATION : D3D12_TEXTURE_COPY_LOCATION
{
    CD3DX12_TEXTURE_COPY_LOCATION() = default;

    explicit CD3DX12_TEXTURE_COPY_LOCATION(const D3D12_TEXTURE_COPY_LOCATION& o) noexcept :
        D3D12_TEXTURE_COPY_LOCATION(o)
    {
    }

    CD3DX12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes) noexcept
    {
        pResource = pRes;
        Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        PlacedFootprint = {};
    }

    CD3DX12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes,
                                  const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& Footprint) noexcept
    {
        pResource = pRes;
        Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        PlacedFootprint = Footprint;
    }

    CD3DX12_TEXTURE_COPY_LOCATION(_In_ ID3D12Resource* pRes, const UINT Sub) noexcept
    {
        pResource = pRes;
        Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        PlacedFootprint = {};
        SubresourceIndex = Sub;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_DESCRIPTOR_RANGE : D3D12_DESCRIPTOR_RANGE
{
    CD3DX12_DESCRIPTOR_RANGE() = default;

    explicit CD3DX12_DESCRIPTOR_RANGE(const D3D12_DESCRIPTOR_RANGE& o) noexcept :
        D3D12_DESCRIPTOR_RANGE(o)
    {
    }

    CD3DX12_DESCRIPTOR_RANGE(
        const D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        const UINT numDescriptors,
        const UINT baseShaderRegister,
        const UINT registerSpace = 0,
        const UINT offsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) noexcept
    {
        Init(rangeType, numDescriptors, baseShaderRegister, registerSpace, offsetInDescriptorsFromTableStart);
    }

    void Init(
        const D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        const UINT numDescriptors,
        const UINT baseShaderRegister,
        const UINT registerSpace = 0,
        const UINT offsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) noexcept
    {
        Init(*this, rangeType, numDescriptors, baseShaderRegister, registerSpace, offsetInDescriptorsFromTableStart);
    }

    static void Init(
        _Out_ D3D12_DESCRIPTOR_RANGE& range,
        const D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        const UINT numDescriptors,
        const UINT baseShaderRegister,
        const UINT registerSpace = 0,
        const UINT offsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) noexcept
    {
        range.RangeType = rangeType;
        range.NumDescriptors = numDescriptors;
        range.BaseShaderRegister = baseShaderRegister;
        range.RegisterSpace = registerSpace;
        range.OffsetInDescriptorsFromTableStart = offsetInDescriptorsFromTableStart;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_DESCRIPTOR_TABLE : D3D12_ROOT_DESCRIPTOR_TABLE
{
    CD3DX12_ROOT_DESCRIPTOR_TABLE() = default;

    explicit CD3DX12_ROOT_DESCRIPTOR_TABLE(const D3D12_ROOT_DESCRIPTOR_TABLE& o) noexcept :
        D3D12_ROOT_DESCRIPTOR_TABLE(o)
    {
    }

    CD3DX12_ROOT_DESCRIPTOR_TABLE(
        const UINT numDescriptorRanges,
        _In_reads_opt_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE* _pDescriptorRanges) noexcept
    {
        Init(numDescriptorRanges, _pDescriptorRanges);
    }

    void Init(
        const UINT numDescriptorRanges,
        _In_reads_opt_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE* _pDescriptorRanges) noexcept
    {
        Init(*this, numDescriptorRanges, _pDescriptorRanges);
    }

    static void Init(
        _Out_ D3D12_ROOT_DESCRIPTOR_TABLE& rootDescriptorTable,
        const UINT numDescriptorRanges,
        _In_reads_opt_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE* _pDescriptorRanges) noexcept
    {
        rootDescriptorTable.NumDescriptorRanges = numDescriptorRanges;
        rootDescriptorTable.pDescriptorRanges = _pDescriptorRanges;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_CONSTANTS : D3D12_ROOT_CONSTANTS
{
    CD3DX12_ROOT_CONSTANTS() = default;

    explicit CD3DX12_ROOT_CONSTANTS(const D3D12_ROOT_CONSTANTS& o) noexcept :
        D3D12_ROOT_CONSTANTS(o)
    {
    }

    CD3DX12_ROOT_CONSTANTS(
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0) noexcept
    {
        Init(num32BitValues, shaderRegister, registerSpace);
    }

    void Init(
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0) noexcept
    {
        Init(*this, num32BitValues, shaderRegister, registerSpace);
    }

    static void Init(
        _Out_ D3D12_ROOT_CONSTANTS& rootConstants,
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0) noexcept
    {
        rootConstants.Num32BitValues = num32BitValues;
        rootConstants.ShaderRegister = shaderRegister;
        rootConstants.RegisterSpace = registerSpace;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_DESCRIPTOR : D3D12_ROOT_DESCRIPTOR
{
    CD3DX12_ROOT_DESCRIPTOR() = default;

    explicit CD3DX12_ROOT_DESCRIPTOR(const D3D12_ROOT_DESCRIPTOR& o) noexcept :
        D3D12_ROOT_DESCRIPTOR(o)
    {
    }

    CD3DX12_ROOT_DESCRIPTOR(
        const UINT shaderRegister,
        const UINT registerSpace = 0) noexcept
    {
        Init(shaderRegister, registerSpace);
    }

    void Init(
        const UINT shaderRegister,
        const UINT registerSpace = 0) noexcept
    {
        Init(*this, shaderRegister, registerSpace);
    }

    static void Init(_Out_ D3D12_ROOT_DESCRIPTOR& table, const UINT shaderRegister, const UINT registerSpace = 0) noexcept
    {
        table.ShaderRegister = shaderRegister;
        table.RegisterSpace = registerSpace;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_PARAMETER : D3D12_ROOT_PARAMETER
{
    CD3DX12_ROOT_PARAMETER() = default;

    explicit CD3DX12_ROOT_PARAMETER(const D3D12_ROOT_PARAMETER& o) noexcept :
        D3D12_ROOT_PARAMETER(o)
    {
    }

    static void InitAsDescriptorTable(
        _Out_ D3D12_ROOT_PARAMETER& rootParam,
        const UINT numDescriptorRanges,
        _In_reads_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE* pDescriptorRanges,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR_TABLE::Init(rootParam.DescriptorTable, numDescriptorRanges, pDescriptorRanges);
    }

    static void InitAsConstants(
        _Out_ D3D12_ROOT_PARAMETER& rootParam,
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_CONSTANTS::Init(rootParam.Constants, num32BitValues, shaderRegister, registerSpace);
    }

    static void InitAsConstantBufferView(
        _Out_ D3D12_ROOT_PARAMETER& rootParam,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR::Init(rootParam.Descriptor, shaderRegister, registerSpace);
    }

    static void InitAsShaderResourceView(
        _Out_ D3D12_ROOT_PARAMETER& rootParam,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR::Init(rootParam.Descriptor, shaderRegister, registerSpace);
    }

    static void InitAsUnorderedAccessView(
        _Out_ D3D12_ROOT_PARAMETER& rootParam,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR::Init(rootParam.Descriptor, shaderRegister, registerSpace);
    }

    void InitAsDescriptorTable(
        const UINT numDescriptorRanges,
        _In_reads_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE* pDescriptorRanges,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsDescriptorTable(*this, numDescriptorRanges, pDescriptorRanges, visibility);
    }

    void InitAsConstants(
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsConstants(*this, num32BitValues, shaderRegister, registerSpace, visibility);
    }

    void InitAsConstantBufferView(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsConstantBufferView(*this, shaderRegister, registerSpace, visibility);
    }

    void InitAsShaderResourceView(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsShaderResourceView(*this, shaderRegister, registerSpace, visibility);
    }

    void InitAsUnorderedAccessView(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsUnorderedAccessView(*this, shaderRegister, registerSpace, visibility);
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_STATIC_SAMPLER_DESC : D3D12_STATIC_SAMPLER_DESC
{
    CD3DX12_STATIC_SAMPLER_DESC() = default;

    explicit CD3DX12_STATIC_SAMPLER_DESC(const D3D12_STATIC_SAMPLER_DESC& o) noexcept :
        D3D12_STATIC_SAMPLER_DESC(o)
    {
    }

    CD3DX12_STATIC_SAMPLER_DESC(
        const UINT shaderRegister,
        const D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
        const D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const FLOAT mipLODBias = 0,
        const UINT maxAnisotropy = 16,
        const D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        const D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        const FLOAT minLOD = 0.f,
        const FLOAT maxLOD = D3D12_FLOAT32_MAX,
        const D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        const UINT registerSpace = 0) noexcept
    {
        Init(
            shaderRegister,
            filter,
            addressU,
            addressV,
            addressW,
            mipLODBias,
            maxAnisotropy,
            comparisonFunc,
            borderColor,
            minLOD,
            maxLOD,
            shaderVisibility,
            registerSpace);
    }

    static void Init(
        _Out_ D3D12_STATIC_SAMPLER_DESC& samplerDesc,
        const UINT shaderRegister,
        const D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
        const D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const FLOAT mipLODBias = 0,
        const UINT maxAnisotropy = 16,
        const D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        const D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        const FLOAT minLOD = 0.f,
        const FLOAT maxLOD = D3D12_FLOAT32_MAX,
        const D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        const UINT registerSpace = 0) noexcept
    {
        samplerDesc.ShaderRegister = shaderRegister;
        samplerDesc.Filter = filter;
        samplerDesc.AddressU = addressU;
        samplerDesc.AddressV = addressV;
        samplerDesc.AddressW = addressW;
        samplerDesc.MipLODBias = mipLODBias;
        samplerDesc.MaxAnisotropy = maxAnisotropy;
        samplerDesc.ComparisonFunc = comparisonFunc;
        samplerDesc.BorderColor = borderColor;
        samplerDesc.MinLOD = minLOD;
        samplerDesc.MaxLOD = maxLOD;
        samplerDesc.ShaderVisibility = shaderVisibility;
        samplerDesc.RegisterSpace = registerSpace;
    }

    void Init(
        const UINT shaderRegister,
        const D3D12_FILTER filter = D3D12_FILTER_ANISOTROPIC,
        const D3D12_TEXTURE_ADDRESS_MODE addressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const D3D12_TEXTURE_ADDRESS_MODE addressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const D3D12_TEXTURE_ADDRESS_MODE addressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP,
        const FLOAT mipLODBias = 0,
        const UINT maxAnisotropy = 16,
        const D3D12_COMPARISON_FUNC comparisonFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL,
        const D3D12_STATIC_BORDER_COLOR borderColor = D3D12_STATIC_BORDER_COLOR_OPAQUE_WHITE,
        const FLOAT minLOD = 0.f,
        const FLOAT maxLOD = D3D12_FLOAT32_MAX,
        const D3D12_SHADER_VISIBILITY shaderVisibility = D3D12_SHADER_VISIBILITY_ALL,
        const UINT registerSpace = 0) noexcept
    {
        Init(
            *this,
            shaderRegister,
            filter,
            addressU,
            addressV,
            addressW,
            mipLODBias,
            maxAnisotropy,
            comparisonFunc,
            borderColor,
            minLOD,
            maxLOD,
            shaderVisibility,
            registerSpace);
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_SIGNATURE_DESC : D3D12_ROOT_SIGNATURE_DESC
{
    CD3DX12_ROOT_SIGNATURE_DESC() = default;

    explicit CD3DX12_ROOT_SIGNATURE_DESC(const D3D12_ROOT_SIGNATURE_DESC& o) noexcept :
        D3D12_ROOT_SIGNATURE_DESC(o)
    {
    }

    CD3DX12_ROOT_SIGNATURE_DESC(
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init(numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
    }

    CD3DX12_ROOT_SIGNATURE_DESC(CD3DX12_DEFAULT) noexcept
    {
        Init(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    }

    void Init(
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init(*this, numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
    }

    static void Init(
        _Out_ D3D12_ROOT_SIGNATURE_DESC& desc,
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        desc.NumParameters = numParameters;
        desc.pParameters = _pParameters;
        desc.NumStaticSamplers = numStaticSamplers;
        desc.pStaticSamplers = _pStaticSamplers;
        desc.Flags = flags;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_DESCRIPTOR_RANGE1 : D3D12_DESCRIPTOR_RANGE1
{
    CD3DX12_DESCRIPTOR_RANGE1() = default;

    explicit CD3DX12_DESCRIPTOR_RANGE1(const D3D12_DESCRIPTOR_RANGE1& o) noexcept :
        D3D12_DESCRIPTOR_RANGE1(o)
    {
    }

    CD3DX12_DESCRIPTOR_RANGE1(
        const D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        const UINT numDescriptors,
        const UINT baseShaderRegister,
        const UINT registerSpace = 0,
        const D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        const UINT offsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) noexcept
    {
        Init(rangeType, numDescriptors, baseShaderRegister, registerSpace, flags, offsetInDescriptorsFromTableStart);
    }

    void Init(
        const D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        const UINT numDescriptors,
        const UINT baseShaderRegister,
        const UINT registerSpace = 0,
        const D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        const UINT offsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) noexcept
    {
        Init(*this, rangeType, numDescriptors, baseShaderRegister, registerSpace, flags,
             offsetInDescriptorsFromTableStart);
    }

    static void Init(
        _Out_ D3D12_DESCRIPTOR_RANGE1& range,
        const D3D12_DESCRIPTOR_RANGE_TYPE rangeType,
        const UINT numDescriptors,
        const UINT baseShaderRegister,
        const UINT registerSpace = 0,
        const D3D12_DESCRIPTOR_RANGE_FLAGS flags = D3D12_DESCRIPTOR_RANGE_FLAG_NONE,
        const UINT offsetInDescriptorsFromTableStart =
            D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND) noexcept
    {
        range.RangeType = rangeType;
        range.NumDescriptors = numDescriptors;
        range.BaseShaderRegister = baseShaderRegister;
        range.RegisterSpace = registerSpace;
        range.Flags = flags;
        range.OffsetInDescriptorsFromTableStart = offsetInDescriptorsFromTableStart;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_DESCRIPTOR_TABLE1 : D3D12_ROOT_DESCRIPTOR_TABLE1
{
    CD3DX12_ROOT_DESCRIPTOR_TABLE1() = default;

    explicit CD3DX12_ROOT_DESCRIPTOR_TABLE1(const D3D12_ROOT_DESCRIPTOR_TABLE1& o) noexcept :
        D3D12_ROOT_DESCRIPTOR_TABLE1(o)
    {
    }

    CD3DX12_ROOT_DESCRIPTOR_TABLE1(
        const UINT numDescriptorRanges,
        _In_reads_opt_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE1* _pDescriptorRanges) noexcept
    {
        Init(numDescriptorRanges, _pDescriptorRanges);
    }

    void Init(
        const UINT numDescriptorRanges,
        _In_reads_opt_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE1* _pDescriptorRanges) noexcept
    {
        Init(*this, numDescriptorRanges, _pDescriptorRanges);
    }

    static void Init(
        _Out_ D3D12_ROOT_DESCRIPTOR_TABLE1& rootDescriptorTable,
        const UINT numDescriptorRanges,
        _In_reads_opt_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE1* _pDescriptorRanges) noexcept
    {
        rootDescriptorTable.NumDescriptorRanges = numDescriptorRanges;
        rootDescriptorTable.pDescriptorRanges = _pDescriptorRanges;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_DESCRIPTOR1 : D3D12_ROOT_DESCRIPTOR1
{
    CD3DX12_ROOT_DESCRIPTOR1() = default;

    explicit CD3DX12_ROOT_DESCRIPTOR1(const D3D12_ROOT_DESCRIPTOR1& o) noexcept :
        D3D12_ROOT_DESCRIPTOR1(o)
    {
    }

    CD3DX12_ROOT_DESCRIPTOR1(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE) noexcept
    {
        Init(shaderRegister, registerSpace, flags);
    }

    void Init(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE) noexcept
    {
        Init(*this, shaderRegister, registerSpace, flags);
    }

    static void Init(
        _Out_ D3D12_ROOT_DESCRIPTOR1& table,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE) noexcept
    {
        table.ShaderRegister = shaderRegister;
        table.RegisterSpace = registerSpace;
        table.Flags = flags;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_ROOT_PARAMETER1 : D3D12_ROOT_PARAMETER1
{
    CD3DX12_ROOT_PARAMETER1() = default;

    explicit CD3DX12_ROOT_PARAMETER1(const D3D12_ROOT_PARAMETER1& o) noexcept :
        D3D12_ROOT_PARAMETER1(o)
    {
    }

    static void InitAsDescriptorTable(
        _Out_ D3D12_ROOT_PARAMETER1& rootParam,
        const UINT numDescriptorRanges,
        _In_reads_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR_TABLE1::Init(rootParam.DescriptorTable, numDescriptorRanges, pDescriptorRanges);
    }

    static void InitAsConstants(
        _Out_ D3D12_ROOT_PARAMETER1& rootParam,
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_CONSTANTS::Init(rootParam.Constants, num32BitValues, shaderRegister, registerSpace);
    }

    static void InitAsConstantBufferView(
        _Out_ D3D12_ROOT_PARAMETER1& rootParam,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR1::Init(rootParam.Descriptor, shaderRegister, registerSpace, flags);
    }

    static void InitAsShaderResourceView(
        _Out_ D3D12_ROOT_PARAMETER1& rootParam,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_SRV;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR1::Init(rootParam.Descriptor, shaderRegister, registerSpace, flags);
    }

    static void InitAsUnorderedAccessView(
        _Out_ D3D12_ROOT_PARAMETER1& rootParam,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_UAV;
        rootParam.ShaderVisibility = visibility;
        CD3DX12_ROOT_DESCRIPTOR1::Init(rootParam.Descriptor, shaderRegister, registerSpace, flags);
    }

    void InitAsDescriptorTable(
        const UINT numDescriptorRanges,
        _In_reads_(numDescriptorRanges) const D3D12_DESCRIPTOR_RANGE1* pDescriptorRanges,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsDescriptorTable(*this, numDescriptorRanges, pDescriptorRanges, visibility);
    }

    void InitAsConstants(
        const UINT num32BitValues,
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsConstants(*this, num32BitValues, shaderRegister, registerSpace, visibility);
    }

    void InitAsConstantBufferView(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsConstantBufferView(*this, shaderRegister, registerSpace, flags, visibility);
    }

    void InitAsShaderResourceView(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsShaderResourceView(*this, shaderRegister, registerSpace, flags, visibility);
    }

    void InitAsUnorderedAccessView(
        const UINT shaderRegister,
        const UINT registerSpace = 0,
        const D3D12_ROOT_DESCRIPTOR_FLAGS flags = D3D12_ROOT_DESCRIPTOR_FLAG_NONE,
        const D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL) noexcept
    {
        InitAsUnorderedAccessView(*this, shaderRegister, registerSpace, flags, visibility);
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC : D3D12_VERSIONED_ROOT_SIGNATURE_DESC
{
    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC() = default;

    explicit CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(const D3D12_VERSIONED_ROOT_SIGNATURE_DESC& o) noexcept :
        D3D12_VERSIONED_ROOT_SIGNATURE_DESC(o)
    {
    }

    explicit CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(const D3D12_ROOT_SIGNATURE_DESC& o) noexcept
    {
        Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
        Desc_1_0 = o;
    }

    explicit CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(const D3D12_ROOT_SIGNATURE_DESC1& o) noexcept
    {
        Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        Desc_1_1 = o;
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init_1_0(numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER1* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init_1_1(numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
    }

    CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC(CD3DX12_DEFAULT) noexcept
    {
        Init_1_1(0, nullptr, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_NONE);
    }

    void Init_1_0(
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init_1_0(*this, numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
    }

    static void Init_1_0(
        _Out_ D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc,
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_0;
        desc.Desc_1_0.NumParameters = numParameters;
        desc.Desc_1_0.pParameters = _pParameters;
        desc.Desc_1_0.NumStaticSamplers = numStaticSamplers;
        desc.Desc_1_0.pStaticSamplers = _pStaticSamplers;
        desc.Desc_1_0.Flags = flags;
    }

    void Init_1_1(
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER1* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        Init_1_1(*this, numParameters, _pParameters, numStaticSamplers, _pStaticSamplers, flags);
    }

    static void Init_1_1(
        _Out_ D3D12_VERSIONED_ROOT_SIGNATURE_DESC& desc,
        const UINT numParameters,
        _In_reads_opt_(numParameters) const D3D12_ROOT_PARAMETER1* _pParameters,
        const UINT numStaticSamplers = 0,
        _In_reads_opt_(numStaticSamplers) const D3D12_STATIC_SAMPLER_DESC* _pStaticSamplers = nullptr,
        const D3D12_ROOT_SIGNATURE_FLAGS flags = D3D12_ROOT_SIGNATURE_FLAG_NONE) noexcept
    {
        desc.Version = D3D_ROOT_SIGNATURE_VERSION_1_1;
        desc.Desc_1_1.NumParameters = numParameters;
        desc.Desc_1_1.pParameters = _pParameters;
        desc.Desc_1_1.NumStaticSamplers = numStaticSamplers;
        desc.Desc_1_1.pStaticSamplers = _pStaticSamplers;
        desc.Desc_1_1.Flags = flags;
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_CPU_DESCRIPTOR_HANDLE : D3D12_CPU_DESCRIPTOR_HANDLE
{
    CD3DX12_CPU_DESCRIPTOR_HANDLE() = default;

    explicit CD3DX12_CPU_DESCRIPTOR_HANDLE(const D3D12_CPU_DESCRIPTOR_HANDLE& o) noexcept :
        D3D12_CPU_DESCRIPTOR_HANDLE(o)
    {
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE(CD3DX12_DEFAULT) noexcept { ptr = 0; }

    CD3DX12_CPU_DESCRIPTOR_HANDLE(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& other,
                                  const INT offsetScaledByIncrementSize) noexcept
    {
        InitOffsetted(other, offsetScaledByIncrementSize);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& other, const INT offsetInDescriptors,
                                  const UINT descriptorIncrementSize) noexcept
    {
        InitOffsetted(other, offsetInDescriptors, descriptorIncrementSize);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE& Offset(const INT offsetInDescriptors, const UINT descriptorIncrementSize) noexcept
    {
        ptr = static_cast<SIZE_T>(static_cast<INT64>(ptr) + static_cast<INT64>(offsetInDescriptors) * static_cast<INT64>(descriptorIncrementSize));
        return *this;
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE& Offset(const INT offsetScaledByIncrementSize) noexcept
    {
        ptr = static_cast<SIZE_T>(static_cast<INT64>(ptr) + static_cast<INT64>(offsetScaledByIncrementSize));
        return *this;
    }

    bool operator==(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& other) const noexcept
    {
        return (ptr == other.ptr);
    }

    bool operator!=(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& other) const noexcept
    {
        return (ptr != other.ptr);
    }

    CD3DX12_CPU_DESCRIPTOR_HANDLE& operator=(const D3D12_CPU_DESCRIPTOR_HANDLE& other) noexcept
    {
        ptr = other.ptr;
        return *this;
    }

    void InitOffsetted(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& base, const INT offsetScaledByIncrementSize) noexcept
    {
        InitOffsetted(*this, base, offsetScaledByIncrementSize);
    }

    void InitOffsetted(_In_ const D3D12_CPU_DESCRIPTOR_HANDLE& base, const INT offsetInDescriptors,
                       const UINT descriptorIncrementSize) noexcept
    {
        InitOffsetted(*this, base, offsetInDescriptors, descriptorIncrementSize);
    }

    static void InitOffsetted(_Out_ D3D12_CPU_DESCRIPTOR_HANDLE& handle, _In_ const D3D12_CPU_DESCRIPTOR_HANDLE& base,
                              const INT offsetScaledByIncrementSize) noexcept
    {
        handle.ptr = static_cast<SIZE_T>(static_cast<INT64>(base.ptr) + static_cast<INT64>(offsetScaledByIncrementSize));
    }

    static void InitOffsetted(_Out_ D3D12_CPU_DESCRIPTOR_HANDLE& handle, _In_ const D3D12_CPU_DESCRIPTOR_HANDLE& base,
                              const INT offsetInDescriptors, const UINT descriptorIncrementSize) noexcept
    {
        handle.ptr = static_cast<SIZE_T>(static_cast<INT64>(base.ptr) + static_cast<INT64>(offsetInDescriptors) * static_cast<INT64>(descriptorIncrementSize));
    }
};

//------------------------------------------------------------------------------------------------
struct CD3DX12_GPU_DESCRIPTOR_HANDLE : D3D12_GPU_DESCRIPTOR_HANDLE
{
    CD3DX12_GPU_DESCRIPTOR_HANDLE() = default;

    explicit CD3DX12_GPU_DESCRIPTOR_HANDLE(const D3D12_GPU_DESCRIPTOR_HANDLE& o) noexcept :
        D3D12_GPU_DESCRIPTOR_HANDLE(o)
    {
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE(CD3DX12_DEFAULT) noexcept { ptr = 0; }

    CD3DX12_GPU_DESCRIPTOR_HANDLE(_In_ const D3D12_GPU_DESCRIPTOR_HANDLE& other,
                                  const INT offsetScaledByIncrementSize) noexcept
    {
        InitOffsetted(other, offsetScaledByIncrementSize);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE(_In_ const D3D12_GPU_DESCRIPTOR_HANDLE& other, const INT offsetInDescriptors,
                                  const UINT descriptorIncrementSize) noexcept
    {
        InitOffsetted(other, offsetInDescriptors, descriptorIncrementSize);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE& Offset(const INT offsetInDescriptors, const UINT descriptorIncrementSize) noexcept
    {
        ptr = static_cast<UINT64>(static_cast<INT64>(ptr) + static_cast<INT64>(offsetInDescriptors) * static_cast<INT64>(descriptorIncrementSize));
        return *this;
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE& Offset(const INT offsetScaledByIncrementSize) noexcept
    {
        ptr = static_cast<UINT64>(static_cast<INT64>(ptr) + static_cast<INT64>(offsetScaledByIncrementSize));
        return *this;
    }

    bool operator==(_In_ const D3D12_GPU_DESCRIPTOR_HANDLE& other) const noexcept
    {
        return (ptr == other.ptr);
    }

    bool operator!=(_In_ const D3D12_GPU_DESCRIPTOR_HANDLE& other) const noexcept
    {
        return (ptr != other.ptr);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE& operator=(const D3D12_GPU_DESCRIPTOR_HANDLE& other) noexcept
    {
        ptr = other.ptr;
        return *this;
    }

    void InitOffsetted(_In_ const D3D12_GPU_DESCRIPTOR_HANDLE& base, const INT offsetScaledByIncrementSize) noexcept
    {
        InitOffsetted(*this, base, offsetScaledByIncrementSize);
    }

    void InitOffsetted(_In_ const D3D12_GPU_DESCRIPTOR_HANDLE& base, const INT offsetInDescriptors,
                       const UINT descriptorIncrementSize) noexcept
    {
        InitOffsetted(*this, base, offsetInDescriptors, descriptorIncrementSize);
    }

    static void InitOffsetted(_Out_ D3D12_GPU_DESCRIPTOR_HANDLE& handle, _In_ const D3D12_GPU_DESCRIPTOR_HANDLE& base,
                              const INT offsetScaledByIncrementSize) noexcept
    {
        handle.ptr = static_cast<UINT64>(static_cast<INT64>(base.ptr) + static_cast<INT64>(offsetScaledByIncrementSize));
    }

    static void InitOffsetted(_Out_ D3D12_GPU_DESCRIPTOR_HANDLE& handle, _In_ const D3D12_GPU_DESCRIPTOR_HANDLE& base,
                              const INT offsetInDescriptors, const UINT descriptorIncrementSize) noexcept
    {
        handle.ptr = static_cast<UINT64>(static_cast<INT64>(base.ptr) + static_cast<INT64>(offsetInDescriptors) * static_cast<INT64>(descriptorIncrementSize));
    }
};

//------------------------------------------------------------------------------------------------
constexpr UINT D3D12CalcSubresource(const UINT MipSlice, const UINT ArraySlice, const UINT PlaneSlice, const UINT MipLevels,
                                    const UINT ArraySize) noexcept
{
    return MipSlice + ArraySlice * MipLevels + PlaneSlice * MipLevels * ArraySize;
}

//------------------------------------------------------------------------------------------------
template <typename T, typename U, typename V>
void D3D12DecomposeSubresource(const UINT Subresource, const UINT MipLevels, const UINT ArraySize, _Out_ T& MipSlice, _Out_ U& ArraySlice,
                               _Out_ V& PlaneSlice) noexcept
{
    MipSlice = static_cast<T>(Subresource % MipLevels);
    ArraySlice = static_cast<U>((Subresource / MipLevels) % ArraySize);
    PlaneSlice = static_cast<V>(Subresource / (MipLevels * ArraySize));
}

//------------------------------------------------------------------------------------------------
inline UINT8 D3D12GetFormatPlaneCount(
    _In_ ID3D12Device* pDevice,
    const DXGI_FORMAT Format
) noexcept
{
    D3D12_FEATURE_DATA_FORMAT_INFO formatInfo = {Format, 0};
    if (FAILED(pDevice->CheckFeatureSupport(D3D12_FEATURE_FORMAT_INFO, &formatInfo, sizeof(formatInfo))))
    {
        return 0;
    }
    return formatInfo.PlaneCount;
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_RESOURCE_DESC : D3D12_RESOURCE_DESC
{
    CD3DX12_RESOURCE_DESC() = default;

    explicit CD3DX12_RESOURCE_DESC(const D3D12_RESOURCE_DESC& o) noexcept :
        D3D12_RESOURCE_DESC(o)
    {
    }

    CD3DX12_RESOURCE_DESC(
        const D3D12_RESOURCE_DIMENSION dimension,
        const UINT64 alignment,
        const UINT64 width,
        const UINT height,
        const UINT16 depthOrArraySize,
        const UINT16 mipLevels,
        const DXGI_FORMAT format,
        const UINT sampleCount,
        const UINT sampleQuality,
        const D3D12_TEXTURE_LAYOUT layout,
        const D3D12_RESOURCE_FLAGS flags) noexcept
    {
        Dimension = dimension;
        Alignment = alignment;
        Width = width;
        Height = height;
        DepthOrArraySize = depthOrArraySize;
        MipLevels = mipLevels;
        Format = format;
        SampleDesc.Count = sampleCount;
        SampleDesc.Quality = sampleQuality;
        Layout = layout;
        Flags = flags;
    }

    static CD3DX12_RESOURCE_DESC Buffer(
        const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) noexcept
    {
        return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, resAllocInfo.Alignment, resAllocInfo.SizeInBytes,
                                     1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
    }

    static CD3DX12_RESOURCE_DESC Buffer(
        const UINT64 width,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_BUFFER, alignment, width, 1, 1, 1,
                                     DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags);
    }

    static CD3DX12_RESOURCE_DESC Tex1D(
        const DXGI_FORMAT format,
        const UINT64 width,
        const UINT16 arraySize = 1,
        const UINT16 mipLevels = 0,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE1D, alignment, width, 1, arraySize,
                                     mipLevels, format, 1, 0, layout, flags);
    }

    static CD3DX12_RESOURCE_DESC Tex2D(
        const DXGI_FORMAT format,
        const UINT64 width,
        const UINT height,
        const UINT16 arraySize = 1,
        const UINT16 mipLevels = 0,
        const UINT sampleCount = 1,
        const UINT sampleQuality = 0,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE2D, alignment, width, height, arraySize,
                                     mipLevels, format, sampleCount, sampleQuality, layout, flags);
    }

    static CD3DX12_RESOURCE_DESC Tex3D(
        const DXGI_FORMAT format,
        const UINT64 width,
        const UINT height,
        const UINT16 depth,
        const UINT16 mipLevels = 0,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC(D3D12_RESOURCE_DIMENSION_TEXTURE3D, alignment, width, height, depth,
                                     mipLevels, format, 1, 0, layout, flags);
    }

    UINT16 Depth() const noexcept
    {
        return (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1);
    }

    UINT16 ArraySize() const noexcept
    {
        return (Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1);
    }

    UINT8 PlaneCount(_In_ ID3D12Device* pDevice) const noexcept
    {
        return D3D12GetFormatPlaneCount(pDevice, Format);
    }

    UINT Subresources(_In_ ID3D12Device* pDevice) const noexcept
    {
        return MipLevels * ArraySize() * PlaneCount(pDevice);
    }

    UINT CalcSubresource(const UINT MipSlice, const UINT ArraySlice, const UINT PlaneSlice) noexcept
    {
        return D3D12CalcSubresource(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize());
    }
};

inline bool operator==(const D3D12_RESOURCE_DESC& l, const D3D12_RESOURCE_DESC& r) noexcept
{
    return l.Dimension == r.Dimension &&
        l.Alignment == r.Alignment &&
        l.Width == r.Width &&
        l.Height == r.Height &&
        l.DepthOrArraySize == r.DepthOrArraySize &&
        l.MipLevels == r.MipLevels &&
        l.Format == r.Format &&
        l.SampleDesc.Count == r.SampleDesc.Count &&
        l.SampleDesc.Quality == r.SampleDesc.Quality &&
        l.Layout == r.Layout &&
        l.Flags == r.Flags;
}

inline bool operator!=(const D3D12_RESOURCE_DESC& l, const D3D12_RESOURCE_DESC& r) noexcept
{
    return !(l == r);
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_RESOURCE_DESC1 : D3D12_RESOURCE_DESC1
{
    CD3DX12_RESOURCE_DESC1() = default;

    explicit CD3DX12_RESOURCE_DESC1(const D3D12_RESOURCE_DESC1& o) noexcept :
        D3D12_RESOURCE_DESC1(o)
    {
    }

    CD3DX12_RESOURCE_DESC1(
        const D3D12_RESOURCE_DIMENSION dimension,
        const UINT64 alignment,
        const UINT64 width,
        const UINT height,
        const UINT16 depthOrArraySize,
        const UINT16 mipLevels,
        const DXGI_FORMAT format,
        const UINT sampleCount,
        const UINT sampleQuality,
        const D3D12_TEXTURE_LAYOUT layout,
        const D3D12_RESOURCE_FLAGS flags,
        const UINT samplerFeedbackMipRegionWidth = 0,
        const UINT samplerFeedbackMipRegionHeight = 0,
        const UINT samplerFeedbackMipRegionDepth = 0) noexcept
    {
        Dimension = dimension;
        Alignment = alignment;
        Width = width;
        Height = height;
        DepthOrArraySize = depthOrArraySize;
        MipLevels = mipLevels;
        Format = format;
        SampleDesc.Count = sampleCount;
        SampleDesc.Quality = sampleQuality;
        Layout = layout;
        Flags = flags;
        SamplerFeedbackMipRegion.Width = samplerFeedbackMipRegionWidth;
        SamplerFeedbackMipRegion.Height = samplerFeedbackMipRegionHeight;
        SamplerFeedbackMipRegion.Depth = samplerFeedbackMipRegionDepth;
    }

    static CD3DX12_RESOURCE_DESC1 Buffer(
        const D3D12_RESOURCE_ALLOCATION_INFO& resAllocInfo,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE) noexcept
    {
        return CD3DX12_RESOURCE_DESC1(D3D12_RESOURCE_DIMENSION_BUFFER, resAllocInfo.Alignment, resAllocInfo.SizeInBytes,
                                      1, 1, 1, DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags, 0, 0,
                                      0);
    }

    static CD3DX12_RESOURCE_DESC1 Buffer(
        const UINT64 width,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC1(D3D12_RESOURCE_DIMENSION_BUFFER, alignment, width, 1, 1, 1,
                                      DXGI_FORMAT_UNKNOWN, 1, 0, D3D12_TEXTURE_LAYOUT_ROW_MAJOR, flags, 0, 0, 0);
    }

    static CD3DX12_RESOURCE_DESC1 Tex1D(
        const DXGI_FORMAT format,
        const UINT64 width,
        const UINT16 arraySize = 1,
        const UINT16 mipLevels = 0,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC1(D3D12_RESOURCE_DIMENSION_TEXTURE1D, alignment, width, 1, arraySize,
                                      mipLevels, format, 1, 0, layout, flags, 0, 0, 0);
    }

    static CD3DX12_RESOURCE_DESC1 Tex2D(
        const DXGI_FORMAT format,
        const UINT64 width,
        const UINT height,
        const UINT16 arraySize = 1,
        const UINT16 mipLevels = 0,
        const UINT sampleCount = 1,
        const UINT sampleQuality = 0,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        const UINT64 alignment = 0,
        const UINT samplerFeedbackMipRegionWidth = 0,
        const UINT samplerFeedbackMipRegionHeight = 0,
        const UINT samplerFeedbackMipRegionDepth = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC1(D3D12_RESOURCE_DIMENSION_TEXTURE2D, alignment, width, height, arraySize,
                                      mipLevels, format, sampleCount, sampleQuality, layout, flags,
                                      samplerFeedbackMipRegionWidth,
                                      samplerFeedbackMipRegionHeight, samplerFeedbackMipRegionDepth);
    }

    static CD3DX12_RESOURCE_DESC1 Tex3D(
        const DXGI_FORMAT format,
        const UINT64 width,
        const UINT height,
        const UINT16 depth,
        const UINT16 mipLevels = 0,
        const D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
        const D3D12_TEXTURE_LAYOUT layout = D3D12_TEXTURE_LAYOUT_UNKNOWN,
        const UINT64 alignment = 0) noexcept
    {
        return CD3DX12_RESOURCE_DESC1(D3D12_RESOURCE_DIMENSION_TEXTURE3D, alignment, width, height, depth,
                                      mipLevels, format, 1, 0, layout, flags, 0, 0, 0);
    }

    UINT16 Depth() const noexcept
    {
        return (Dimension == D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1);
    }

    UINT16 ArraySize() const noexcept
    {
        return (Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE3D ? DepthOrArraySize : 1);
    }

    UINT8 PlaneCount(_In_ ID3D12Device* pDevice) const noexcept
    {
        return D3D12GetFormatPlaneCount(pDevice, Format);
    }

    UINT Subresources(_In_ ID3D12Device* pDevice) const noexcept
    {
        return MipLevels * ArraySize() * PlaneCount(pDevice);
    }

    UINT CalcSubresource(const UINT MipSlice, const UINT ArraySlice, const UINT PlaneSlice) noexcept
    {
        return D3D12CalcSubresource(MipSlice, ArraySlice, PlaneSlice, MipLevels, ArraySize());
    }
};

inline bool operator==(const D3D12_RESOURCE_DESC1& l, const D3D12_RESOURCE_DESC1& r) noexcept
{
    return l.Dimension == r.Dimension &&
        l.Alignment == r.Alignment &&
        l.Width == r.Width &&
        l.Height == r.Height &&
        l.DepthOrArraySize == r.DepthOrArraySize &&
        l.MipLevels == r.MipLevels &&
        l.Format == r.Format &&
        l.SampleDesc.Count == r.SampleDesc.Count &&
        l.SampleDesc.Quality == r.SampleDesc.Quality &&
        l.Layout == r.Layout &&
        l.Flags == r.Flags &&
        l.SamplerFeedbackMipRegion.Width == r.SamplerFeedbackMipRegion.Width &&
        l.SamplerFeedbackMipRegion.Height == r.SamplerFeedbackMipRegion.Height &&
        l.SamplerFeedbackMipRegion.Depth == r.SamplerFeedbackMipRegion.Depth;
}

inline bool operator!=(const D3D12_RESOURCE_DESC1& l, const D3D12_RESOURCE_DESC1& r) noexcept
{
    return !(l == r);
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_VIEW_INSTANCING_DESC : D3D12_VIEW_INSTANCING_DESC
{
    CD3DX12_VIEW_INSTANCING_DESC() = default;

    explicit CD3DX12_VIEW_INSTANCING_DESC(const D3D12_VIEW_INSTANCING_DESC& o) noexcept :
        D3D12_VIEW_INSTANCING_DESC(o)
    {
    }

    explicit CD3DX12_VIEW_INSTANCING_DESC(CD3DX12_DEFAULT) noexcept
    {
        ViewInstanceCount = 0;
        pViewInstanceLocations = nullptr;
        Flags = D3D12_VIEW_INSTANCING_FLAG_NONE;
    }

    explicit CD3DX12_VIEW_INSTANCING_DESC(
        const UINT InViewInstanceCount,
        const D3D12_VIEW_INSTANCE_LOCATION* InViewInstanceLocations,
        const D3D12_VIEW_INSTANCING_FLAGS InFlags) noexcept
    {
        ViewInstanceCount = InViewInstanceCount;
        pViewInstanceLocations = InViewInstanceLocations;
        Flags = InFlags;
    }
};

//------------------------------------------------------------------------------------------------
// Row-by-row memcpy
inline void MemcpySubresource(
    _In_ const D3D12_MEMCPY_DEST* pDest,
    _In_ const D3D12_SUBRESOURCE_DATA* pSrc,
    const SIZE_T RowSizeInBytes,
    const UINT NumRows,
    const UINT NumSlices) noexcept
{
    for (UINT z = 0; z < NumSlices; ++z)
    {
        auto pDestSlice = static_cast<BYTE*>(pDest->pData) + pDest->SlicePitch * z;
        auto pSrcSlice = static_cast<const BYTE*>(pSrc->pData) + pSrc->SlicePitch * static_cast<LONG_PTR>(z);
        for (UINT y = 0; y < NumRows; ++y)
        {
            memcpy(pDestSlice + pDest->RowPitch * y,
                   pSrcSlice + pSrc->RowPitch * static_cast<LONG_PTR>(y),
                   RowSizeInBytes);
        }
    }
}

//------------------------------------------------------------------------------------------------
// Returns required size of a buffer to be used for data upload
inline UINT64 GetRequiredIntermediateSize(
    _In_ ID3D12Resource* pDestinationResource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) const UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) const UINT NumSubresources) noexcept
{
    auto Desc = pDestinationResource->GetDesc();
    UINT64 RequiredSize = 0;

    ID3D12Device* pDevice = nullptr;
    pDestinationResource->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, 0, nullptr, nullptr, nullptr,
                                   &RequiredSize);
    pDevice->Release();

    return RequiredSize;
}

//------------------------------------------------------------------------------------------------
// All arrays must be populated (e.g. by calling GetCopyableFootprints)
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) const UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) const UINT NumSubresources,
    const UINT64 RequiredSize,
    _In_reads_(NumSubresources) const D3D12_PLACED_SUBRESOURCE_FOOTPRINT* pLayouts,
    _In_reads_(NumSubresources) const UINT* pNumRows,
    _In_reads_(NumSubresources) const UINT64* pRowSizesInBytes,
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
{
    // Minor validation
    auto IntermediateDesc = pIntermediate->GetDesc();
    auto DestinationDesc = pDestinationResource->GetDesc();
    if (IntermediateDesc.Dimension != D3D12_RESOURCE_DIMENSION_BUFFER ||
        IntermediateDesc.Width < RequiredSize + pLayouts[0].Offset ||
        RequiredSize > static_cast<SIZE_T>(-1) ||
        (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER &&
            (FirstSubresource != 0 || NumSubresources != 1)))
    {
        return 0;
    }

    BYTE* pData;
    HRESULT hr = pIntermediate->Map(0, nullptr, reinterpret_cast<void**>(&pData));
    if (FAILED(hr))
    {
        return 0;
    }

    for (UINT i = 0; i < NumSubresources; ++i)
    {
        if (pRowSizesInBytes[i] > static_cast<SIZE_T>(-1)) return 0;
        D3D12_MEMCPY_DEST DestData = {
            pData + pLayouts[i].Offset, pLayouts[i].Footprint.RowPitch,
            static_cast<SIZE_T>(pLayouts[i].Footprint.RowPitch) * static_cast<SIZE_T>(pNumRows[i])
        };
        MemcpySubresource(&DestData, &pSrcData[i], pRowSizesInBytes[i], pNumRows[i], pLayouts[i].Footprint.Depth);
    }
    pIntermediate->Unmap(0, nullptr);

    if (DestinationDesc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER)
    {
        pCmdList->CopyBufferRegion(
            pDestinationResource, 0, pIntermediate, pLayouts[0].Offset, pLayouts[0].Footprint.Width);
    }
    else
    {
        for (UINT i = 0; i < NumSubresources; ++i)
        {
            CD3DX12_TEXTURE_COPY_LOCATION Dst(pDestinationResource, i + FirstSubresource);
            CD3DX12_TEXTURE_COPY_LOCATION Src(pIntermediate, pLayouts[i]);
            pCmdList->CopyTextureRegion(&Dst, 0, 0, 0, &Src, nullptr);
        }
    }
    return RequiredSize;
}

//------------------------------------------------------------------------------------------------
// Heap-allocating UpdateSubresources implementation
inline UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    const UINT64 IntermediateOffset,
    _In_range_(0, D3D12_REQ_SUBRESOURCES) const UINT FirstSubresource,
    _In_range_(0, D3D12_REQ_SUBRESOURCES - FirstSubresource) const UINT NumSubresources,
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
{
    UINT64 RequiredSize = 0;
    UINT64 MemToAlloc = (sizeof(D3D12_PLACED_SUBRESOURCE_FOOTPRINT) + sizeof(UINT) + sizeof(UINT64)) * NumSubresources;
    if (MemToAlloc > SIZE_MAX)
    {
        return 0;
    }
    void* pMem = HeapAlloc(GetProcessHeap(), 0, MemToAlloc);
    if (pMem == nullptr)
    {
        return 0;
    }
    auto pLayouts = static_cast<D3D12_PLACED_SUBRESOURCE_FOOTPRINT*>(pMem);
    auto pRowSizesInBytes = reinterpret_cast<UINT64*>(pLayouts + NumSubresources);
    auto pNumRows = reinterpret_cast<UINT*>(pRowSizesInBytes + NumSubresources);

    auto Desc = pDestinationResource->GetDesc();
    ID3D12Device* pDevice = nullptr;
    pDestinationResource->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, pLayouts, pNumRows,
                                   pRowSizesInBytes, &RequiredSize);
    pDevice->Release();

    UINT64 Result = UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources,
                                       RequiredSize, pLayouts, pNumRows, pRowSizesInBytes, pSrcData);
    HeapFree(GetProcessHeap(), 0, pMem);
    return Result;
}

//------------------------------------------------------------------------------------------------
// Stack-allocating UpdateSubresources implementation
template <UINT MaxSubresources>
UINT64 UpdateSubresources(
    _In_ ID3D12GraphicsCommandList* pCmdList,
    _In_ ID3D12Resource* pDestinationResource,
    _In_ ID3D12Resource* pIntermediate,
    const UINT64 IntermediateOffset,
    _In_range_(0, MaxSubresources) const UINT FirstSubresource,
    _In_range_(1, MaxSubresources - FirstSubresource) const UINT NumSubresources,
    _In_reads_(NumSubresources) const D3D12_SUBRESOURCE_DATA* pSrcData) noexcept
{
    UINT64 RequiredSize = 0;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT Layouts[MaxSubresources];
    UINT NumRows[MaxSubresources];
    UINT64 RowSizesInBytes[MaxSubresources];

    auto Desc = pDestinationResource->GetDesc();
    ID3D12Device* pDevice = nullptr;
    pDestinationResource->GetDevice(IID_ID3D12Device, reinterpret_cast<void**>(&pDevice));
    pDevice->GetCopyableFootprints(&Desc, FirstSubresource, NumSubresources, IntermediateOffset, Layouts, NumRows,
                                   RowSizesInBytes, &RequiredSize);
    pDevice->Release();

    return UpdateSubresources(pCmdList, pDestinationResource, pIntermediate, FirstSubresource, NumSubresources,
                              RequiredSize, Layouts, NumRows, RowSizesInBytes, pSrcData);
}

//------------------------------------------------------------------------------------------------
constexpr bool D3D12IsLayoutOpaque(const D3D12_TEXTURE_LAYOUT Layout) noexcept
{
    return Layout == D3D12_TEXTURE_LAYOUT_UNKNOWN || Layout == D3D12_TEXTURE_LAYOUT_64KB_UNDEFINED_SWIZZLE;
}

//------------------------------------------------------------------------------------------------
template <typename t_CommandListType>
ID3D12CommandList* const* CommandListCast(t_CommandListType* const* pp) noexcept
{
    // This cast is useful for passing strongly typed command list pointers into
    // ExecuteCommandLists.
    // This cast is valid as long as the const-ness is respected. D3D12 APIs do
    // respect the const-ness of their arguments.
    return reinterpret_cast<ID3D12CommandList* const*>(pp);
}

//------------------------------------------------------------------------------------------------
// D3D12 exports a new method for serializing root signatures in the Windows 10 Anniversary Update.
// To help enable root signature 1.1 features when they are available and not require maintaining
// two code paths for building root signatures, this helper method reconstructs a 1.0 signature when
// 1.1 is not supported.
inline HRESULT D3DX12SerializeVersionedRootSignature(
    _In_ const D3D12_VERSIONED_ROOT_SIGNATURE_DESC* pRootSignatureDesc,
    const D3D_ROOT_SIGNATURE_VERSION MaxVersion,
    _Outptr_ ID3DBlob** ppBlob,
    _Always_(_Outptr_opt_result_maybenull_) ID3DBlob** ppErrorBlob) noexcept
{
    if (ppErrorBlob != nullptr)
    {
        *ppErrorBlob = nullptr;
    }

    switch (MaxVersion)
    {
    case D3D_ROOT_SIGNATURE_VERSION_1_0:
        switch (pRootSignatureDesc->Version)
        {
        case D3D_ROOT_SIGNATURE_VERSION_1_0:
            return D3D12SerializeRootSignature(&pRootSignatureDesc->Desc_1_0, D3D_ROOT_SIGNATURE_VERSION_1, ppBlob,
                                               ppErrorBlob);

        case D3D_ROOT_SIGNATURE_VERSION_1_1:
            {
                HRESULT hr = S_OK;
                const D3D12_ROOT_SIGNATURE_DESC1& desc_1_1 = pRootSignatureDesc->Desc_1_1;

                const SIZE_T ParametersSize = sizeof(D3D12_ROOT_PARAMETER) * desc_1_1.NumParameters;
                void* pParameters = (ParametersSize > 0) ? HeapAlloc(GetProcessHeap(), 0, ParametersSize) : nullptr;
                if (ParametersSize > 0 && pParameters == nullptr)
                {
                    hr = E_OUTOFMEMORY;
                }
                auto pParameters_1_0 = static_cast<D3D12_ROOT_PARAMETER*>(pParameters);

                if (SUCCEEDED(hr))
                {
                    for (UINT n = 0; n < desc_1_1.NumParameters; n++)
                    {
                        __analysis_assume(ParametersSize == sizeof(D3D12_ROOT_PARAMETER) * desc_1_1.NumParameters);
                        pParameters_1_0[n].ParameterType = desc_1_1.pParameters[n].ParameterType;
                        pParameters_1_0[n].ShaderVisibility = desc_1_1.pParameters[n].ShaderVisibility;

                        switch (desc_1_1.pParameters[n].ParameterType)
                        {
                        case D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS:
                            pParameters_1_0[n].Constants.Num32BitValues = desc_1_1.pParameters[n].Constants.
                                                                                                  Num32BitValues;
                            pParameters_1_0[n].Constants.RegisterSpace = desc_1_1.pParameters[n].Constants.
                                                                                                 RegisterSpace;
                            pParameters_1_0[n].Constants.ShaderRegister = desc_1_1.pParameters[n].Constants.
                                                                                                  ShaderRegister;
                            break;

                        case D3D12_ROOT_PARAMETER_TYPE_CBV:
                        case D3D12_ROOT_PARAMETER_TYPE_SRV:
                        case D3D12_ROOT_PARAMETER_TYPE_UAV:
                            pParameters_1_0[n].Descriptor.RegisterSpace = desc_1_1.pParameters[n].Descriptor.
                                                                                                  RegisterSpace;
                            pParameters_1_0[n].Descriptor.ShaderRegister = desc_1_1.pParameters[n].Descriptor.
                                                                                                   ShaderRegister;
                            break;

                        case D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE:
                            const D3D12_ROOT_DESCRIPTOR_TABLE1& table_1_1 = desc_1_1.pParameters[n].DescriptorTable;

                            const SIZE_T DescriptorRangesSize = sizeof(D3D12_DESCRIPTOR_RANGE) * table_1_1.
                                NumDescriptorRanges;
                            void* pDescriptorRanges = (DescriptorRangesSize > 0 && SUCCEEDED(hr))
                                                          ? HeapAlloc(GetProcessHeap(), 0, DescriptorRangesSize)
                                                          : nullptr;
                            if (DescriptorRangesSize > 0 && pDescriptorRanges == nullptr)
                            {
                                hr = E_OUTOFMEMORY;
                            }
                            auto pDescriptorRanges_1_0 = static_cast<D3D12_DESCRIPTOR_RANGE*>(pDescriptorRanges);

                            if (SUCCEEDED(hr))
                            {
                                for (UINT x = 0; x < table_1_1.NumDescriptorRanges; x++)
                                {
                                    __analysis_assume(
                                        DescriptorRangesSize == sizeof(D3D12_DESCRIPTOR_RANGE) * table_1_1.
                                        NumDescriptorRanges);
                                    pDescriptorRanges_1_0[x].BaseShaderRegister = table_1_1.pDescriptorRanges[x].
                                        BaseShaderRegister;
                                    pDescriptorRanges_1_0[x].NumDescriptors = table_1_1.pDescriptorRanges[x].
                                        NumDescriptors;
                                    pDescriptorRanges_1_0[x].OffsetInDescriptorsFromTableStart = table_1_1.
                                        pDescriptorRanges[x].OffsetInDescriptorsFromTableStart;
                                    pDescriptorRanges_1_0[x].RangeType = table_1_1.pDescriptorRanges[x].RangeType;
                                    pDescriptorRanges_1_0[x].RegisterSpace = table_1_1.pDescriptorRanges[x].
                                        RegisterSpace;
                                }
                            }

                            D3D12_ROOT_DESCRIPTOR_TABLE& table_1_0 = pParameters_1_0[n].DescriptorTable;
                            table_1_0.NumDescriptorRanges = table_1_1.NumDescriptorRanges;
                            table_1_0.pDescriptorRanges = pDescriptorRanges_1_0;
                        }
                    }
                }

                if (SUCCEEDED(hr))
                {
                    CD3DX12_ROOT_SIGNATURE_DESC desc_1_0(desc_1_1.NumParameters, pParameters_1_0,
                                                         desc_1_1.NumStaticSamplers, desc_1_1.pStaticSamplers,
                                                         desc_1_1.Flags);
                    hr = D3D12SerializeRootSignature(&desc_1_0, D3D_ROOT_SIGNATURE_VERSION_1, ppBlob, ppErrorBlob);
                }

                if (pParameters)
                {
                    for (UINT n = 0; n < desc_1_1.NumParameters; n++)
                    {
                        if (desc_1_1.pParameters[n].ParameterType == D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE)
                        {
                            HeapFree(GetProcessHeap(), 0,
                                     const_cast<D3D12_DESCRIPTOR_RANGE*>(pParameters_1_0[n].DescriptorTable.
                                                                                            pDescriptorRanges));
                        }
                    }
                    HeapFree(GetProcessHeap(), 0, pParameters);
                }
                return hr;
            }
        }
        break;

    case D3D_ROOT_SIGNATURE_VERSION_1_1:
        return D3D12SerializeVersionedRootSignature(pRootSignatureDesc, ppBlob, ppErrorBlob);
    }

    return E_INVALIDARG;
}

//------------------------------------------------------------------------------------------------
struct CD3DX12_RT_FORMAT_ARRAY : D3D12_RT_FORMAT_ARRAY
{
    CD3DX12_RT_FORMAT_ARRAY() = default;

    explicit CD3DX12_RT_FORMAT_ARRAY(const D3D12_RT_FORMAT_ARRAY& o) noexcept
        : D3D12_RT_FORMAT_ARRAY(o)
    {
    }

    explicit CD3DX12_RT_FORMAT_ARRAY(_In_reads_(NumFormats) const DXGI_FORMAT* pFormats, const UINT NumFormats) noexcept
    {
        NumRenderTargets = NumFormats;
        memcpy(RTFormats, pFormats, sizeof(RTFormats));
        // assumes ARRAY_SIZE(pFormats) == ARRAY_SIZE(RTFormats)
    }
};

//------------------------------------------------------------------------------------------------
// Pipeline State Stream Helpers
//------------------------------------------------------------------------------------------------

//------------------------------------------------------------------------------------------------
// Stream Subobjects, i.e. elements of a stream

struct DefaultSampleMask
{
    operator UINT() noexcept { return UINT_MAX; }
};

struct DefaultSampleDesc
{
    operator DXGI_SAMPLE_DESC() noexcept { return DXGI_SAMPLE_DESC{1, 0}; }
};

#pragma warning(push)
#pragma warning(disable : 4324)
template <typename InnerStructType, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE Type, typename DefaultArg = InnerStructType>
class alignas(void*) CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT
{
    D3D12_PIPELINE_STATE_SUBOBJECT_TYPE _Type;
    InnerStructType _Inner;

public:
    CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT() noexcept : _Type(Type), _Inner(DefaultArg())
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT(const InnerStructType& i) noexcept : _Type(Type), _Inner(i)
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT& operator=(const InnerStructType& i) noexcept
    {
        _Type = Type;
        _Inner = i;
        return *this;
    }

    operator const InnerStructType&() const noexcept { return _Inner; }
    operator InnerStructType&() noexcept { return _Inner; }
    InnerStructType* operator&() noexcept { return &_Inner; }
    const InnerStructType* operator&() const noexcept { return &_Inner; }
};
#pragma warning(pop)
using CD3DX12_PIPELINE_STATE_STREAM_FLAGS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_PIPELINE_STATE_FLAGS, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS>;
using CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    UINT, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK>;
using CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    ID3D12RootSignature*, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE>;
using CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_INPUT_LAYOUT_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT>;
using CD3DX12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_INDEX_BUFFER_STRIP_CUT_VALUE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE>;
using CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_PRIMITIVE_TOPOLOGY_TYPE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY>;
using CD3DX12_PIPELINE_STATE_STREAM_VS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS>;
using CD3DX12_PIPELINE_STATE_STREAM_GS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS>;
using CD3DX12_PIPELINE_STATE_STREAM_STREAM_OUTPUT = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_STREAM_OUTPUT_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT>;
using CD3DX12_PIPELINE_STATE_STREAM_HS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS>;
using CD3DX12_PIPELINE_STATE_STREAM_DS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS>;
using CD3DX12_PIPELINE_STATE_STREAM_PS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS>;
using CD3DX12_PIPELINE_STATE_STREAM_AS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS>;
using CD3DX12_PIPELINE_STATE_STREAM_MS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS>;
using CD3DX12_PIPELINE_STATE_STREAM_CS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_SHADER_BYTECODE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS>;
using CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    CD3DX12_BLEND_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND, CD3DX12_DEFAULT>;
using CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    CD3DX12_DEPTH_STENCIL_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL, CD3DX12_DEFAULT>;
using CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    CD3DX12_DEPTH_STENCIL_DESC1, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1, CD3DX12_DEFAULT>;
using CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    DXGI_FORMAT, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT>;
using CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    CD3DX12_RASTERIZER_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER, CD3DX12_DEFAULT>;
using CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_RT_FORMAT_ARRAY, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS>;
using CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    DXGI_SAMPLE_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC, DefaultSampleDesc>;
using CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    UINT, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK, DefaultSampleMask>;
using CD3DX12_PIPELINE_STATE_STREAM_CACHED_PSO = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    D3D12_CACHED_PIPELINE_STATE, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO>;
using CD3DX12_PIPELINE_STATE_STREAM_VIEW_INSTANCING = CD3DX12_PIPELINE_STATE_STREAM_SUBOBJECT<
    CD3DX12_VIEW_INSTANCING_DESC, D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING, CD3DX12_DEFAULT>;

//------------------------------------------------------------------------------------------------
// Stream Parser Helpers

struct ID3DX12PipelineParserCallbacks
{
    // Subobject Callbacks
    virtual void FlagsCb(D3D12_PIPELINE_STATE_FLAGS)
    {
    }

    virtual void NodeMaskCb(UINT)
    {
    }

    virtual void RootSignatureCb(ID3D12RootSignature*)
    {
    }

    virtual void InputLayoutCb(const D3D12_INPUT_LAYOUT_DESC&)
    {
    }

    virtual void IBStripCutValueCb(D3D12_INDEX_BUFFER_STRIP_CUT_VALUE)
    {
    }

    virtual void PrimitiveTopologyTypeCb(D3D12_PRIMITIVE_TOPOLOGY_TYPE)
    {
    }

    virtual void VSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void GSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void StreamOutputCb(const D3D12_STREAM_OUTPUT_DESC&)
    {
    }

    virtual void HSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void DSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void PSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void CSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void ASCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void MSCb(const D3D12_SHADER_BYTECODE&)
    {
    }

    virtual void BlendStateCb(const D3D12_BLEND_DESC&)
    {
    }

    virtual void DepthStencilStateCb(const D3D12_DEPTH_STENCIL_DESC&)
    {
    }

    virtual void DepthStencilState1Cb(const D3D12_DEPTH_STENCIL_DESC1&)
    {
    }

    virtual void DSVFormatCb(DXGI_FORMAT)
    {
    }

    virtual void RasterizerStateCb(const D3D12_RASTERIZER_DESC&)
    {
    }

    virtual void RTVFormatsCb(const D3D12_RT_FORMAT_ARRAY&)
    {
    }

    virtual void SampleDescCb(const DXGI_SAMPLE_DESC&)
    {
    }

    virtual void SampleMaskCb(UINT)
    {
    }

    virtual void ViewInstancingCb(const D3D12_VIEW_INSTANCING_DESC&)
    {
    }

    virtual void CachedPSOCb(const D3D12_CACHED_PIPELINE_STATE&)
    {
    }

    // Error Callbacks
    virtual void ErrorBadInputParameter(UINT /*ParameterIndex*/)
    {
    }

    virtual void ErrorDuplicateSubobject(D3D12_PIPELINE_STATE_SUBOBJECT_TYPE /*DuplicateType*/)
    {
    }

    virtual void ErrorUnknownSubobject(UINT /*UnknownTypeValue*/)
    {
    }

    virtual ~ID3DX12PipelineParserCallbacks() = default;
};

struct D3DX12_MESH_SHADER_PIPELINE_STATE_DESC
{
    ID3D12RootSignature* pRootSignature;
    D3D12_SHADER_BYTECODE AS;
    D3D12_SHADER_BYTECODE MS;
    D3D12_SHADER_BYTECODE PS;
    D3D12_BLEND_DESC BlendState;
    UINT SampleMask;
    D3D12_RASTERIZER_DESC RasterizerState;
    D3D12_DEPTH_STENCIL_DESC DepthStencilState;
    D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType;
    UINT NumRenderTargets;
    DXGI_FORMAT RTVFormats[D3D12_SIMULTANEOUS_RENDER_TARGET_COUNT];
    DXGI_FORMAT DSVFormat;
    DXGI_SAMPLE_DESC SampleDesc;
    UINT NodeMask;
    D3D12_CACHED_PIPELINE_STATE CachedPSO;
    D3D12_PIPELINE_STATE_FLAGS Flags;
};

// CD3DX12_PIPELINE_STATE_STREAM2 Works on OS Build 19041+ (where there is a new mesh shader pipeline).
// Use CD3DX12_PIPELINE_STATE_STREAM1 for OS Build 16299+ (where there is a new view instancing subobject).
// Use CD3DX12_PIPELINE_STATE_STREAM for OS Build 15063+ support.
struct CD3DX12_PIPELINE_STATE_STREAM2
{
    CD3DX12_PIPELINE_STATE_STREAM2() = default;
    // Mesh and amplification shaders must be set manually, since they do not have representation in D3D12_GRAPHICS_PIPELINE_STATE_DESC
    CD3DX12_PIPELINE_STATE_STREAM2(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , InputLayout(Desc.InputLayout)
          , IBStripCutValue(Desc.IBStripCutValue)
          , PrimitiveTopologyType(Desc.PrimitiveTopologyType)
          , VS(Desc.VS)
          , GS(Desc.GS)
          , StreamOutput(Desc.StreamOutput)
          , HS(Desc.HS)
          , DS(Desc.DS)
          , PS(Desc.PS)
          , BlendState(CD3DX12_BLEND_DESC(Desc.BlendState))
          , DepthStencilState(CD3DX12_DEPTH_STENCIL_DESC1(Desc.DepthStencilState))
          , DSVFormat(Desc.DSVFormat)
          , RasterizerState(CD3DX12_RASTERIZER_DESC(Desc.RasterizerState))
          , RTVFormats(CD3DX12_RT_FORMAT_ARRAY(Desc.RTVFormats, Desc.NumRenderTargets))
          , SampleDesc(Desc.SampleDesc)
          , SampleMask(Desc.SampleMask)
          , CachedPSO(Desc.CachedPSO)
          , ViewInstancingDesc(CD3DX12_VIEW_INSTANCING_DESC(CD3DX12_DEFAULT()))
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM2(const D3DX12_MESH_SHADER_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , PrimitiveTopologyType(Desc.PrimitiveTopologyType)
          , PS(Desc.PS)
          , AS(Desc.AS)
          , MS(Desc.MS)
          , BlendState(CD3DX12_BLEND_DESC(Desc.BlendState))
          , DepthStencilState(CD3DX12_DEPTH_STENCIL_DESC1(Desc.DepthStencilState))
          , DSVFormat(Desc.DSVFormat)
          , RasterizerState(CD3DX12_RASTERIZER_DESC(Desc.RasterizerState))
          , RTVFormats(CD3DX12_RT_FORMAT_ARRAY(Desc.RTVFormats, Desc.NumRenderTargets))
          , SampleDesc(Desc.SampleDesc)
          , SampleMask(Desc.SampleMask)
          , CachedPSO(Desc.CachedPSO)
          , ViewInstancingDesc(CD3DX12_VIEW_INSTANCING_DESC(CD3DX12_DEFAULT()))
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM2(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , CS(CD3DX12_SHADER_BYTECODE(Desc.CS))
          , CachedPSO(Desc.CachedPSO)
    {
        static_cast<D3D12_DEPTH_STENCIL_DESC1&>(DepthStencilState).DepthEnable = false;
    }

    CD3DX12_PIPELINE_STATE_STREAM_FLAGS Flags;
    CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK NodeMask;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
    CD3DX12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE IBStripCutValue;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
    CD3DX12_PIPELINE_STATE_STREAM_VS VS;
    CD3DX12_PIPELINE_STATE_STREAM_GS GS;
    CD3DX12_PIPELINE_STATE_STREAM_STREAM_OUTPUT StreamOutput;
    CD3DX12_PIPELINE_STATE_STREAM_HS HS;
    CD3DX12_PIPELINE_STATE_STREAM_DS DS;
    CD3DX12_PIPELINE_STATE_STREAM_PS PS;
    CD3DX12_PIPELINE_STATE_STREAM_AS AS;
    CD3DX12_PIPELINE_STATE_STREAM_MS MS;
    CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
    CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask;
    CD3DX12_PIPELINE_STATE_STREAM_CACHED_PSO CachedPSO;
    CD3DX12_PIPELINE_STATE_STREAM_VIEW_INSTANCING ViewInstancingDesc;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDescV0() const noexcept
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.InputLayout = this->InputLayout;
        D.IBStripCutValue = this->IBStripCutValue;
        D.PrimitiveTopologyType = this->PrimitiveTopologyType;
        D.VS = this->VS;
        D.GS = this->GS;
        D.StreamOutput = this->StreamOutput;
        D.HS = this->HS;
        D.DS = this->DS;
        D.PS = this->PS;
        D.BlendState = this->BlendState;
        D.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEPTH_STENCIL_DESC1(this->DepthStencilState));
        D.DSVFormat = this->DSVFormat;
        D.RasterizerState = this->RasterizerState;
        D.NumRenderTargets = D3D12_RT_FORMAT_ARRAY(this->RTVFormats).NumRenderTargets;
        memcpy(D.RTVFormats, D3D12_RT_FORMAT_ARRAY(this->RTVFormats).RTFormats, sizeof(D.RTVFormats));
        D.SampleDesc = this->SampleDesc;
        D.SampleMask = this->SampleMask;
        D.CachedPSO = this->CachedPSO;
        return D;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDescV0() const noexcept
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.CS = this->CS;
        D.CachedPSO = this->CachedPSO;
        return D;
    }
};

// CD3DX12_PIPELINE_STATE_STREAM1 Works on OS Build 16299+ (where there is a new view instancing subobject).
// Use CD3DX12_PIPELINE_STATE_STREAM for OS Build 15063+ support.
struct CD3DX12_PIPELINE_STATE_STREAM1
{
    CD3DX12_PIPELINE_STATE_STREAM1() = default;
    // Mesh and amplification shaders must be set manually, since they do not have representation in D3D12_GRAPHICS_PIPELINE_STATE_DESC
    CD3DX12_PIPELINE_STATE_STREAM1(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , InputLayout(Desc.InputLayout)
          , IBStripCutValue(Desc.IBStripCutValue)
          , PrimitiveTopologyType(Desc.PrimitiveTopologyType)
          , VS(Desc.VS)
          , GS(Desc.GS)
          , StreamOutput(Desc.StreamOutput)
          , HS(Desc.HS)
          , DS(Desc.DS)
          , PS(Desc.PS)
          , BlendState(CD3DX12_BLEND_DESC(Desc.BlendState))
          , DepthStencilState(CD3DX12_DEPTH_STENCIL_DESC1(Desc.DepthStencilState))
          , DSVFormat(Desc.DSVFormat)
          , RasterizerState(CD3DX12_RASTERIZER_DESC(Desc.RasterizerState))
          , RTVFormats(CD3DX12_RT_FORMAT_ARRAY(Desc.RTVFormats, Desc.NumRenderTargets))
          , SampleDesc(Desc.SampleDesc)
          , SampleMask(Desc.SampleMask)
          , CachedPSO(Desc.CachedPSO)
          , ViewInstancingDesc(CD3DX12_VIEW_INSTANCING_DESC(CD3DX12_DEFAULT()))
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM1(const D3DX12_MESH_SHADER_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , PrimitiveTopologyType(Desc.PrimitiveTopologyType)
          , PS(Desc.PS)
          , BlendState(CD3DX12_BLEND_DESC(Desc.BlendState))
          , DepthStencilState(CD3DX12_DEPTH_STENCIL_DESC1(Desc.DepthStencilState))
          , DSVFormat(Desc.DSVFormat)
          , RasterizerState(CD3DX12_RASTERIZER_DESC(Desc.RasterizerState))
          , RTVFormats(CD3DX12_RT_FORMAT_ARRAY(Desc.RTVFormats, Desc.NumRenderTargets))
          , SampleDesc(Desc.SampleDesc)
          , SampleMask(Desc.SampleMask)
          , CachedPSO(Desc.CachedPSO)
          , ViewInstancingDesc(CD3DX12_VIEW_INSTANCING_DESC(CD3DX12_DEFAULT()))
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM1(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , CS(CD3DX12_SHADER_BYTECODE(Desc.CS))
          , CachedPSO(Desc.CachedPSO)
    {
        static_cast<D3D12_DEPTH_STENCIL_DESC1&>(DepthStencilState).DepthEnable = false;
    }

    CD3DX12_PIPELINE_STATE_STREAM_FLAGS Flags;
    CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK NodeMask;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
    CD3DX12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE IBStripCutValue;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
    CD3DX12_PIPELINE_STATE_STREAM_VS VS;
    CD3DX12_PIPELINE_STATE_STREAM_GS GS;
    CD3DX12_PIPELINE_STATE_STREAM_STREAM_OUTPUT StreamOutput;
    CD3DX12_PIPELINE_STATE_STREAM_HS HS;
    CD3DX12_PIPELINE_STATE_STREAM_DS DS;
    CD3DX12_PIPELINE_STATE_STREAM_PS PS;
    CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
    CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask;
    CD3DX12_PIPELINE_STATE_STREAM_CACHED_PSO CachedPSO;
    CD3DX12_PIPELINE_STATE_STREAM_VIEW_INSTANCING ViewInstancingDesc;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDescV0() const noexcept
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.InputLayout = this->InputLayout;
        D.IBStripCutValue = this->IBStripCutValue;
        D.PrimitiveTopologyType = this->PrimitiveTopologyType;
        D.VS = this->VS;
        D.GS = this->GS;
        D.StreamOutput = this->StreamOutput;
        D.HS = this->HS;
        D.DS = this->DS;
        D.PS = this->PS;
        D.BlendState = this->BlendState;
        D.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEPTH_STENCIL_DESC1(this->DepthStencilState));
        D.DSVFormat = this->DSVFormat;
        D.RasterizerState = this->RasterizerState;
        D.NumRenderTargets = D3D12_RT_FORMAT_ARRAY(this->RTVFormats).NumRenderTargets;
        memcpy(D.RTVFormats, D3D12_RT_FORMAT_ARRAY(this->RTVFormats).RTFormats, sizeof(D.RTVFormats));
        D.SampleDesc = this->SampleDesc;
        D.SampleMask = this->SampleMask;
        D.CachedPSO = this->CachedPSO;
        return D;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDescV0() const noexcept
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.CS = this->CS;
        D.CachedPSO = this->CachedPSO;
        return D;
    }
};


struct CD3DX12_PIPELINE_MESH_STATE_STREAM
{
    CD3DX12_PIPELINE_MESH_STATE_STREAM() = default;

    CD3DX12_PIPELINE_MESH_STATE_STREAM(const D3DX12_MESH_SHADER_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , PS(Desc.PS)
          , AS(Desc.AS)
          , MS(Desc.MS)
          , BlendState(CD3DX12_BLEND_DESC(Desc.BlendState))
          , DepthStencilState(CD3DX12_DEPTH_STENCIL_DESC1(Desc.DepthStencilState))
          , DSVFormat(Desc.DSVFormat)
          , RasterizerState(CD3DX12_RASTERIZER_DESC(Desc.RasterizerState))
          , RTVFormats(CD3DX12_RT_FORMAT_ARRAY(Desc.RTVFormats, Desc.NumRenderTargets))
          , SampleDesc(Desc.SampleDesc)
          , SampleMask(Desc.SampleMask)
          , CachedPSO(Desc.CachedPSO)
          , ViewInstancingDesc(CD3DX12_VIEW_INSTANCING_DESC(CD3DX12_DEFAULT()))
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM_FLAGS Flags;
    CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK NodeMask;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_PS PS;
    CD3DX12_PIPELINE_STATE_STREAM_AS AS;
    CD3DX12_PIPELINE_STATE_STREAM_MS MS;
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
    CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask;
    CD3DX12_PIPELINE_STATE_STREAM_CACHED_PSO CachedPSO;
    CD3DX12_PIPELINE_STATE_STREAM_VIEW_INSTANCING ViewInstancingDesc;

    D3DX12_MESH_SHADER_PIPELINE_STATE_DESC MeshShaderDescV0() const noexcept
    {
        D3DX12_MESH_SHADER_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.PS = this->PS;
        D.AS = this->AS;
        D.MS = this->MS;
        D.BlendState = this->BlendState;
        D.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEPTH_STENCIL_DESC1(this->DepthStencilState));
        D.DSVFormat = this->DSVFormat;
        D.RasterizerState = this->RasterizerState;
        D.NumRenderTargets = D3D12_RT_FORMAT_ARRAY(this->RTVFormats).NumRenderTargets;
        memcpy(D.RTVFormats, D3D12_RT_FORMAT_ARRAY(this->RTVFormats).RTFormats, sizeof(D.RTVFormats));
        D.SampleDesc = this->SampleDesc;
        D.SampleMask = this->SampleMask;
        D.CachedPSO = this->CachedPSO;
        return D;
    }
};

// CD3DX12_PIPELINE_STATE_STREAM works on OS Build 15063+ but does not support new subobject(s) added in OS Build 16299+.
// See CD3DX12_PIPELINE_STATE_STREAM1 for instance.
struct CD3DX12_PIPELINE_STATE_STREAM
{
    CD3DX12_PIPELINE_STATE_STREAM() = default;

    CD3DX12_PIPELINE_STATE_STREAM(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , InputLayout(Desc.InputLayout)
          , IBStripCutValue(Desc.IBStripCutValue)
          , PrimitiveTopologyType(Desc.PrimitiveTopologyType)
          , VS(Desc.VS)
          , GS(Desc.GS)
          , StreamOutput(Desc.StreamOutput)
          , HS(Desc.HS)
          , DS(Desc.DS)
          , PS(Desc.PS)
          , BlendState(CD3DX12_BLEND_DESC(Desc.BlendState))
          , DepthStencilState(CD3DX12_DEPTH_STENCIL_DESC1(Desc.DepthStencilState))
          , DSVFormat(Desc.DSVFormat)
          , RasterizerState(CD3DX12_RASTERIZER_DESC(Desc.RasterizerState))
          , RTVFormats(CD3DX12_RT_FORMAT_ARRAY(Desc.RTVFormats, Desc.NumRenderTargets))
          , SampleDesc(Desc.SampleDesc)
          , SampleMask(Desc.SampleMask)
          , CachedPSO(Desc.CachedPSO)
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM(const D3D12_COMPUTE_PIPELINE_STATE_DESC& Desc) noexcept
        : Flags(Desc.Flags)
          , NodeMask(Desc.NodeMask)
          , pRootSignature(Desc.pRootSignature)
          , CS(CD3DX12_SHADER_BYTECODE(Desc.CS))
          , CachedPSO(Desc.CachedPSO)
    {
    }

    CD3DX12_PIPELINE_STATE_STREAM_FLAGS Flags;
    CD3DX12_PIPELINE_STATE_STREAM_NODE_MASK NodeMask;
    CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
    CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
    CD3DX12_PIPELINE_STATE_STREAM_IB_STRIP_CUT_VALUE IBStripCutValue;
    CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
    CD3DX12_PIPELINE_STATE_STREAM_VS VS;
    CD3DX12_PIPELINE_STATE_STREAM_GS GS;
    CD3DX12_PIPELINE_STATE_STREAM_STREAM_OUTPUT StreamOutput;
    CD3DX12_PIPELINE_STATE_STREAM_HS HS;
    CD3DX12_PIPELINE_STATE_STREAM_DS DS;
    CD3DX12_PIPELINE_STATE_STREAM_PS PS;
    CD3DX12_PIPELINE_STATE_STREAM_CS CS;
    CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC BlendState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL1 DepthStencilState;
    CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
    CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER RasterizerState;
    CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
    CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask;
    CD3DX12_PIPELINE_STATE_STREAM_CACHED_PSO CachedPSO;

    D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicsDescV0() const noexcept
    {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.InputLayout = this->InputLayout;
        D.IBStripCutValue = this->IBStripCutValue;
        D.PrimitiveTopologyType = this->PrimitiveTopologyType;
        D.VS = this->VS;
        D.GS = this->GS;
        D.StreamOutput = this->StreamOutput;
        D.HS = this->HS;
        D.DS = this->DS;
        D.PS = this->PS;
        D.BlendState = this->BlendState;
        D.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(D3D12_DEPTH_STENCIL_DESC1(this->DepthStencilState));
        D.DSVFormat = this->DSVFormat;
        D.RasterizerState = this->RasterizerState;
        D.NumRenderTargets = D3D12_RT_FORMAT_ARRAY(this->RTVFormats).NumRenderTargets;
        memcpy(D.RTVFormats, D3D12_RT_FORMAT_ARRAY(this->RTVFormats).RTFormats, sizeof(D.RTVFormats));
        D.SampleDesc = this->SampleDesc;
        D.SampleMask = this->SampleMask;
        D.CachedPSO = this->CachedPSO;
        return D;
    }

    D3D12_COMPUTE_PIPELINE_STATE_DESC ComputeDescV0() const noexcept
    {
        D3D12_COMPUTE_PIPELINE_STATE_DESC D;
        D.Flags = this->Flags;
        D.NodeMask = this->NodeMask;
        D.pRootSignature = this->pRootSignature;
        D.CS = this->CS;
        D.CachedPSO = this->CachedPSO;
        return D;
    }
};

struct CD3DX12_PIPELINE_STATE_STREAM2_PARSE_HELPER : ID3DX12PipelineParserCallbacks
{
    CD3DX12_PIPELINE_STATE_STREAM2 PipelineStream;

    CD3DX12_PIPELINE_STATE_STREAM2_PARSE_HELPER() noexcept
        : SeenDSS(false)
    {
        // Adjust defaults to account for absent members.
        PipelineStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Depth disabled if no DSV format specified.
        static_cast<D3D12_DEPTH_STENCIL_DESC1&>(PipelineStream.DepthStencilState).DepthEnable = false;
    }

    // ID3DX12PipelineParserCallbacks
    void FlagsCb(const D3D12_PIPELINE_STATE_FLAGS Flags) override { PipelineStream.Flags = Flags; }
    void NodeMaskCb(const UINT NodeMask) override { PipelineStream.NodeMask = NodeMask; }

    void RootSignatureCb(ID3D12RootSignature* pRootSignature) override
    {
        PipelineStream.pRootSignature = pRootSignature;
    }

    void InputLayoutCb(const D3D12_INPUT_LAYOUT_DESC& InputLayout) override
    {
        PipelineStream.InputLayout = InputLayout;
    }

    void IBStripCutValueCb(const D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue) override
    {
        PipelineStream.IBStripCutValue = IBStripCutValue;
    }

    void PrimitiveTopologyTypeCb(const D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType) override
    {
        PipelineStream.PrimitiveTopologyType = PrimitiveTopologyType;
    }

    void VSCb(const D3D12_SHADER_BYTECODE& VS) override { PipelineStream.VS = VS; }
    void GSCb(const D3D12_SHADER_BYTECODE& GS) override { PipelineStream.GS = GS; }

    void StreamOutputCb(const D3D12_STREAM_OUTPUT_DESC& StreamOutput) override
    {
        PipelineStream.StreamOutput = StreamOutput;
    }

    void HSCb(const D3D12_SHADER_BYTECODE& HS) override { PipelineStream.HS = HS; }
    void DSCb(const D3D12_SHADER_BYTECODE& DS) override { PipelineStream.DS = DS; }
    void PSCb(const D3D12_SHADER_BYTECODE& PS) override { PipelineStream.PS = PS; }
    void CSCb(const D3D12_SHADER_BYTECODE& CS) override { PipelineStream.CS = CS; }
    void ASCb(const D3D12_SHADER_BYTECODE& AS) override { PipelineStream.AS = AS; }
    void MSCb(const D3D12_SHADER_BYTECODE& MS) override { PipelineStream.MS = MS; }

    void BlendStateCb(const D3D12_BLEND_DESC& BlendState) override
    {
        PipelineStream.BlendState = CD3DX12_BLEND_DESC(BlendState);
    }

    void DepthStencilStateCb(const D3D12_DEPTH_STENCIL_DESC& DepthStencilState) override
    {
        PipelineStream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(DepthStencilState);
        SeenDSS = true;
    }

    void DepthStencilState1Cb(const D3D12_DEPTH_STENCIL_DESC1& DepthStencilState) override
    {
        PipelineStream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(DepthStencilState);
        SeenDSS = true;
    }

    void DSVFormatCb(const DXGI_FORMAT DSVFormat) override
    {
        PipelineStream.DSVFormat = DSVFormat;
        if (!SeenDSS && DSVFormat != DXGI_FORMAT_UNKNOWN)
        {
            // Re-enable depth for the default state.
            static_cast<D3D12_DEPTH_STENCIL_DESC1&>(PipelineStream.DepthStencilState).DepthEnable = true;
        }
    }

    void RasterizerStateCb(const D3D12_RASTERIZER_DESC& RasterizerState) override
    {
        PipelineStream.RasterizerState = CD3DX12_RASTERIZER_DESC(RasterizerState);
    }

    void RTVFormatsCb(const D3D12_RT_FORMAT_ARRAY& RTVFormats) override { PipelineStream.RTVFormats = RTVFormats; }
    void SampleDescCb(const DXGI_SAMPLE_DESC& SampleDesc) override { PipelineStream.SampleDesc = SampleDesc; }
    void SampleMaskCb(const UINT SampleMask) override { PipelineStream.SampleMask = SampleMask; }

    void ViewInstancingCb(const D3D12_VIEW_INSTANCING_DESC& ViewInstancingDesc) override
    {
        PipelineStream.ViewInstancingDesc = CD3DX12_VIEW_INSTANCING_DESC(ViewInstancingDesc);
    }

    void CachedPSOCb(const D3D12_CACHED_PIPELINE_STATE& CachedPSO) override { PipelineStream.CachedPSO = CachedPSO; }

private:
    bool SeenDSS;
};


struct CD3DX12_PIPELINE_STATE_STREAM_PARSE_HELPER : ID3DX12PipelineParserCallbacks
{
    CD3DX12_PIPELINE_STATE_STREAM1 PipelineStream;

    CD3DX12_PIPELINE_STATE_STREAM_PARSE_HELPER() noexcept
        : SeenDSS(false)
    {
        // Adjust defaults to account for absent members.
        PipelineStream.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

        // Depth disabled if no DSV format specified.
        static_cast<D3D12_DEPTH_STENCIL_DESC1&>(PipelineStream.DepthStencilState).DepthEnable = false;
    }

    // ID3DX12PipelineParserCallbacks
    void FlagsCb(const D3D12_PIPELINE_STATE_FLAGS Flags) override { PipelineStream.Flags = Flags; }
    void NodeMaskCb(const UINT NodeMask) override { PipelineStream.NodeMask = NodeMask; }

    void RootSignatureCb(ID3D12RootSignature* pRootSignature) override
    {
        PipelineStream.pRootSignature = pRootSignature;
    }

    void InputLayoutCb(const D3D12_INPUT_LAYOUT_DESC& InputLayout) override
    {
        PipelineStream.InputLayout = InputLayout;
    }

    void IBStripCutValueCb(const D3D12_INDEX_BUFFER_STRIP_CUT_VALUE IBStripCutValue) override
    {
        PipelineStream.IBStripCutValue = IBStripCutValue;
    }

    void PrimitiveTopologyTypeCb(const D3D12_PRIMITIVE_TOPOLOGY_TYPE PrimitiveTopologyType) override
    {
        PipelineStream.PrimitiveTopologyType = PrimitiveTopologyType;
    }

    void VSCb(const D3D12_SHADER_BYTECODE& VS) override { PipelineStream.VS = VS; }
    void GSCb(const D3D12_SHADER_BYTECODE& GS) override { PipelineStream.GS = GS; }

    void StreamOutputCb(const D3D12_STREAM_OUTPUT_DESC& StreamOutput) override
    {
        PipelineStream.StreamOutput = StreamOutput;
    }

    void HSCb(const D3D12_SHADER_BYTECODE& HS) override { PipelineStream.HS = HS; }
    void DSCb(const D3D12_SHADER_BYTECODE& DS) override { PipelineStream.DS = DS; }
    void PSCb(const D3D12_SHADER_BYTECODE& PS) override { PipelineStream.PS = PS; }
    void CSCb(const D3D12_SHADER_BYTECODE& CS) override { PipelineStream.CS = CS; }

    void BlendStateCb(const D3D12_BLEND_DESC& BlendState) override
    {
        PipelineStream.BlendState = CD3DX12_BLEND_DESC(BlendState);
    }

    void DepthStencilStateCb(const D3D12_DEPTH_STENCIL_DESC& DepthStencilState) override
    {
        PipelineStream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(DepthStencilState);
        SeenDSS = true;
    }

    void DepthStencilState1Cb(const D3D12_DEPTH_STENCIL_DESC1& DepthStencilState) override
    {
        PipelineStream.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC1(DepthStencilState);
        SeenDSS = true;
    }

    void DSVFormatCb(const DXGI_FORMAT DSVFormat) override
    {
        PipelineStream.DSVFormat = DSVFormat;
        if (!SeenDSS && DSVFormat != DXGI_FORMAT_UNKNOWN)
        {
            // Re-enable depth for the default state.
            static_cast<D3D12_DEPTH_STENCIL_DESC1&>(PipelineStream.DepthStencilState).DepthEnable = true;
        }
    }

    void RasterizerStateCb(const D3D12_RASTERIZER_DESC& RasterizerState) override
    {
        PipelineStream.RasterizerState = CD3DX12_RASTERIZER_DESC(RasterizerState);
    }

    void RTVFormatsCb(const D3D12_RT_FORMAT_ARRAY& RTVFormats) override { PipelineStream.RTVFormats = RTVFormats; }
    void SampleDescCb(const DXGI_SAMPLE_DESC& SampleDesc) override { PipelineStream.SampleDesc = SampleDesc; }
    void SampleMaskCb(const UINT SampleMask) override { PipelineStream.SampleMask = SampleMask; }

    void ViewInstancingCb(const D3D12_VIEW_INSTANCING_DESC& ViewInstancingDesc) override
    {
        PipelineStream.ViewInstancingDesc = CD3DX12_VIEW_INSTANCING_DESC(ViewInstancingDesc);
    }

    void CachedPSOCb(const D3D12_CACHED_PIPELINE_STATE& CachedPSO) override { PipelineStream.CachedPSO = CachedPSO; }

private:
    bool SeenDSS;
};

inline D3D12_PIPELINE_STATE_SUBOBJECT_TYPE D3DX12GetBaseSubobjectType(
    const D3D12_PIPELINE_STATE_SUBOBJECT_TYPE SubobjectType) noexcept
{
    switch (SubobjectType)
    {
    case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
        return D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL;
    default:
        return SubobjectType;
    }
}

inline HRESULT D3DX12ParsePipelineStream(const D3D12_PIPELINE_STATE_STREAM_DESC& Desc,
                                         ID3DX12PipelineParserCallbacks* pCallbacks)
{
    if (pCallbacks == nullptr)
    {
        return E_INVALIDARG;
    }

    if (Desc.SizeInBytes == 0 || Desc.pPipelineStateSubobjectStream == nullptr)
    {
        pCallbacks->ErrorBadInputParameter(1); // first parameter issue
        return E_INVALIDARG;
    }

    bool SubobjectSeen[D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MAX_VALID] = {};
    for (SIZE_T CurOffset = 0, SizeOfSubobject = 0; CurOffset < Desc.SizeInBytes; CurOffset += SizeOfSubobject)
    {
        BYTE* pStream = static_cast<BYTE*>(Desc.pPipelineStateSubobjectStream) + CurOffset;
        auto SubobjectType = *reinterpret_cast<D3D12_PIPELINE_STATE_SUBOBJECT_TYPE*>(pStream);
        if (SubobjectType < 0 || SubobjectType >= D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MAX_VALID)
        {
            pCallbacks->ErrorUnknownSubobject(SubobjectType);
            return E_INVALIDARG;
        }
        if (SubobjectSeen[D3DX12GetBaseSubobjectType(SubobjectType)])
        {
            pCallbacks->ErrorDuplicateSubobject(SubobjectType);
            return E_INVALIDARG; // disallow subobject duplicates in a stream
        }
        SubobjectSeen[SubobjectType] = true;
        switch (SubobjectType)
        {
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_ROOT_SIGNATURE:
            pCallbacks->RootSignatureCb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::pRootSignature)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::pRootSignature);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VS:
            pCallbacks->VSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::VS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::VS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PS:
            pCallbacks->PSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::PS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::PS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DS:
            pCallbacks->DSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::DS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::DS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_HS:
            pCallbacks->HSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::HS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::HS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_GS:
            pCallbacks->GSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::GS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::GS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CS:
            pCallbacks->CSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::CS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::CS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_AS:
            pCallbacks->ASCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM2::AS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM2::AS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_MS:
            pCallbacks->MSCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM2::MS)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM2::MS);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_STREAM_OUTPUT:
            pCallbacks->StreamOutputCb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::StreamOutput)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::StreamOutput);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_BLEND:
            pCallbacks->BlendStateCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::BlendState)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::BlendState);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_MASK:
            pCallbacks->SampleMaskCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::SampleMask)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::SampleMask);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RASTERIZER:
            pCallbacks->RasterizerStateCb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::RasterizerState)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::RasterizerState);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL:
            pCallbacks->DepthStencilStateCb(*reinterpret_cast<CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL1:
            pCallbacks->DepthStencilState1Cb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::DepthStencilState)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::DepthStencilState);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_INPUT_LAYOUT:
            pCallbacks->
                InputLayoutCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::InputLayout)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::InputLayout);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_IB_STRIP_CUT_VALUE:
            pCallbacks->IBStripCutValueCb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::IBStripCutValue)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::IBStripCutValue);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_PRIMITIVE_TOPOLOGY:
            pCallbacks->PrimitiveTopologyTypeCb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::PrimitiveTopologyType)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::PrimitiveTopologyType);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_RENDER_TARGET_FORMATS:
            pCallbacks->RTVFormatsCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::RTVFormats)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::RTVFormats);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_DEPTH_STENCIL_FORMAT:
            pCallbacks->DSVFormatCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::DSVFormat)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::DSVFormat);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_SAMPLE_DESC:
            pCallbacks->SampleDescCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::SampleDesc)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::SampleDesc);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_NODE_MASK:
            pCallbacks->NodeMaskCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::NodeMask)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::NodeMask);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_CACHED_PSO:
            pCallbacks->CachedPSOCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::CachedPSO)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::CachedPSO);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_FLAGS:
            pCallbacks->FlagsCb(*reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM::Flags)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM::Flags);
            break;
        case D3D12_PIPELINE_STATE_SUBOBJECT_TYPE_VIEW_INSTANCING:
            pCallbacks->ViewInstancingCb(
                *reinterpret_cast<decltype(CD3DX12_PIPELINE_STATE_STREAM1::ViewInstancingDesc)*>(pStream));
            SizeOfSubobject = sizeof(CD3DX12_PIPELINE_STATE_STREAM1::ViewInstancingDesc);
            break;
        default:
            pCallbacks->ErrorUnknownSubobject(SubobjectType);
            return E_INVALIDARG;
        }
    }

    return S_OK;
}

//------------------------------------------------------------------------------------------------
inline bool operator==(const D3D12_CLEAR_VALUE& a, const D3D12_CLEAR_VALUE& b) noexcept
{
    if (a.Format != b.Format) return false;
    if (a.Format == DXGI_FORMAT_D24_UNORM_S8_UINT
        || a.Format == DXGI_FORMAT_D16_UNORM
        || a.Format == DXGI_FORMAT_D32_FLOAT
        || a.Format == DXGI_FORMAT_D32_FLOAT_S8X24_UINT)
    {
        return (a.DepthStencil.Depth == b.DepthStencil.Depth) &&
            (a.DepthStencil.Stencil == b.DepthStencil.Stencil);
    }
    return (a.Color[0] == b.Color[0]) &&
        (a.Color[1] == b.Color[1]) &&
        (a.Color[2] == b.Color[2]) &&
        (a.Color[3] == b.Color[3]);
}

inline bool operator==(const D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS& a,
                       const D3D12_RENDER_PASS_BEGINNING_ACCESS_CLEAR_PARAMETERS& b) noexcept
{
    return a.ClearValue == b.ClearValue;
}

inline bool operator==(const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS& a,
                       const D3D12_RENDER_PASS_ENDING_ACCESS_RESOLVE_PARAMETERS& b) noexcept
{
    if (a.pSrcResource != b.pSrcResource) return false;
    if (a.pDstResource != b.pDstResource) return false;
    if (a.SubresourceCount != b.SubresourceCount) return false;
    if (a.Format != b.Format) return false;
    if (a.ResolveMode != b.ResolveMode) return false;
    if (a.PreserveResolveSource != b.PreserveResolveSource) return false;
    return true;
}

inline bool operator==(const D3D12_RENDER_PASS_BEGINNING_ACCESS& a,
                       const D3D12_RENDER_PASS_BEGINNING_ACCESS& b) noexcept
{
    if (a.Type != b.Type) return false;
    if (a.Type == D3D12_RENDER_PASS_BEGINNING_ACCESS_TYPE_CLEAR && !(a.Clear == b.Clear)) return false;
    return true;
}

inline bool operator==(const D3D12_RENDER_PASS_ENDING_ACCESS& a, const D3D12_RENDER_PASS_ENDING_ACCESS& b) noexcept
{
    if (a.Type != b.Type) return false;
    if (a.Type == D3D12_RENDER_PASS_ENDING_ACCESS_TYPE_RESOLVE && !(a.Resolve == b.Resolve)) return false;
    return true;
}

inline bool operator==(const D3D12_RENDER_PASS_RENDER_TARGET_DESC& a,
                       const D3D12_RENDER_PASS_RENDER_TARGET_DESC& b) noexcept
{
    if (a.cpuDescriptor.ptr != b.cpuDescriptor.ptr) return false;
    if (!(a.BeginningAccess == b.BeginningAccess)) return false;
    if (!(a.EndingAccess == b.EndingAccess)) return false;
    return true;
}

inline bool operator==(const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC& a,
                       const D3D12_RENDER_PASS_DEPTH_STENCIL_DESC& b) noexcept
{
    if (a.cpuDescriptor.ptr != b.cpuDescriptor.ptr) return false;
    if (!(a.DepthBeginningAccess == b.DepthBeginningAccess)) return false;
    if (!(a.StencilBeginningAccess == b.StencilBeginningAccess)) return false;
    if (!(a.DepthEndingAccess == b.DepthEndingAccess)) return false;
    if (!(a.StencilEndingAccess == b.StencilEndingAccess)) return false;
    return true;
}


#ifndef D3DX12_NO_STATE_OBJECT_HELPERS

//================================================================================================
// D3DX12 State Object Creation Helpers
//
// Helper classes for creating new style state objects out of an arbitrary set of subobjects.
// Uses STL
//
// Start by instantiating CD3DX12_STATE_OBJECT_DESC (see it's public methods).
// One of its methods is CreateSubobject(), which has a comment showing a couple of options for
// defining subobjects using the helper classes for each subobject (CD3DX12_DXIL_LIBRARY_SUBOBJECT
// etc.). The subobject helpers each have methods specific to the subobject for configuring it's
// contents.
//
//================================================================================================
#include <list>
#include <vector>
#include <string>
#include <memory>
#ifndef D3DX12_USE_ATL
#include <wrl/client.h>
#define D3DX12_COM_PTR Microsoft::WRL::ComPtr
#define D3DX12_COM_PTR_GET(x) x.Get()
#define D3DX12_COM_PTR_ADDRESSOF(x) x.GetAddressOf()
#else
#include <atlbase.h>
#define D3DX12_COM_PTR ATL::CComPtr
#define D3DX12_COM_PTR_GET(x) x.p
#define D3DX12_COM_PTR_ADDRESSOF(x) &x.p
#endif

//------------------------------------------------------------------------------------------------
class CD3DX12_STATE_OBJECT_DESC
{
public:
    CD3DX12_STATE_OBJECT_DESC() noexcept
    {
        Init(D3D12_STATE_OBJECT_TYPE_COLLECTION);
    }

    CD3DX12_STATE_OBJECT_DESC(const D3D12_STATE_OBJECT_TYPE Type) noexcept
    {
        Init(Type);
    }

    void SetStateObjectType(const D3D12_STATE_OBJECT_TYPE Type) noexcept { m_Desc.Type = Type; }

    operator const D3D12_STATE_OBJECT_DESC&()
    {
        // Do final preparation work
        m_RepointedAssociations.clear();
        m_SubobjectArray.clear();
        m_SubobjectArray.reserve(m_Desc.NumSubobjects);
        // Flatten subobjects into an array (each flattened subobject still has a
        // member that's a pointer to it's desc that's not flattened)
        for (auto Iter = m_SubobjectList.begin();
             Iter != m_SubobjectList.end(); ++Iter)
        {
            m_SubobjectArray.push_back(*Iter);
            // Store new location in array so we can redirect pointers contained in subobjects
            Iter->pSubobjectArrayLocation = &m_SubobjectArray.back();
        }
        // For subobjects with pointer fields, create a new copy of those subobject definitions
        // with fixed pointers
        for (UINT i = 0; i < m_Desc.NumSubobjects; i++)
        {
            if (m_SubobjectArray[i].Type == D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION)
            {
                auto pOriginalSubobjectAssociation =
                    static_cast<const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION*>(m_SubobjectArray[i].pDesc);
                D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION Repointed = *pOriginalSubobjectAssociation;
                auto pWrapper =
                    static_cast<const SUBOBJECT_WRAPPER*>(pOriginalSubobjectAssociation->pSubobjectToAssociate);
                Repointed.pSubobjectToAssociate = pWrapper->pSubobjectArrayLocation;
                m_RepointedAssociations.push_back(Repointed);
                m_SubobjectArray[i].pDesc = &m_RepointedAssociations.back();
            }
        }
        // Below: using ugly way to get pointer in case .data() is not defined
        m_Desc.pSubobjects = m_Desc.NumSubobjects ? &m_SubobjectArray[0] : nullptr;
        return m_Desc;
    }

    operator const D3D12_STATE_OBJECT_DESC*()
    {
        // Cast calls the above final preparation work
        return &static_cast<const D3D12_STATE_OBJECT_DESC&>(*this);
    }

    // CreateSubobject creates a sububject helper (e.g. CD3DX12_HIT_GROUP_SUBOBJECT)
    // whose lifetime is owned by this class.
    // e.g.
    //
    //    CD3DX12_STATE_OBJECT_DESC Collection1(D3D12_STATE_OBJECT_TYPE_COLLECTION);
    //    auto Lib0 = Collection1.CreateSubobject<CD3DX12_DXIL_LIBRARY_SUBOBJECT>();
    //    Lib0->SetDXILLibrary(&pMyAppDxilLibs[0]);
    //    Lib0->DefineExport(L"rayGenShader0"); // in practice these export listings might be
    //                                          // data/engine driven
    //    etc.
    //
    // Alternatively, users can instantiate sububject helpers explicitly, such as via local
    // variables instead, passing the state object desc that should point to it into the helper
    // constructor (or call mySubobjectHelper.AddToStateObject(Collection1)).
    // In this alternative scenario, the user must keep the subobject alive as long as the state
    // object it is associated with is alive, else it's pointer references will be stale.
    // e.g.
    //
    //    CD3DX12_STATE_OBJECT_DESC RaytracingState2(D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE);
    //    CD3DX12_DXIL_LIBRARY_SUBOBJECT LibA(RaytracingState2);
    //    LibA.SetDXILLibrary(&pMyAppDxilLibs[4]); // not manually specifying exports
    //                                             // - meaning all exports in the libraries
    //                                             // are exported
    //    etc.

    template <typename T>
    T* CreateSubobject()
    {
        T* pSubobject = new T(*this);
        m_OwnedSubobjectHelpers.emplace_back(pSubobject);
        return pSubobject;
    }

private:
    D3D12_STATE_SUBOBJECT* TrackSubobject(const D3D12_STATE_SUBOBJECT_TYPE Type, void* pDesc)
    {
        SUBOBJECT_WRAPPER Subobject;
        Subobject.pSubobjectArrayLocation = nullptr;
        Subobject.Type = Type;
        Subobject.pDesc = pDesc;
        m_SubobjectList.push_back(Subobject);
        m_Desc.NumSubobjects++;
        return &m_SubobjectList.back();
    }

    void Init(const D3D12_STATE_OBJECT_TYPE Type) noexcept
    {
        SetStateObjectType(Type);
        m_Desc.pSubobjects = nullptr;
        m_Desc.NumSubobjects = 0;
        m_SubobjectList.clear();
        m_SubobjectArray.clear();
        m_RepointedAssociations.clear();
    }

    using SUBOBJECT_WRAPPER = struct SUBOBJECT_WRAPPER : D3D12_STATE_SUBOBJECT
    {
        D3D12_STATE_SUBOBJECT* pSubobjectArrayLocation; // new location when flattened into array
        // for repointing pointers in subobjects
    };
    D3D12_STATE_OBJECT_DESC m_Desc;
    std::list<SUBOBJECT_WRAPPER> m_SubobjectList; // Pointers to list nodes handed out so
    // these can be edited live
    std::vector<D3D12_STATE_SUBOBJECT> m_SubobjectArray; // Built at the end, copying list contents

    std::list<D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION>
    m_RepointedAssociations; // subobject type that contains pointers to other subobjects,
    // repointed to flattened array

    class StringContainer
    {
    public:
        LPCWSTR LocalCopy(const LPCWSTR string, const bool bSingleString = false)
        {
            if (string)
            {
                if (bSingleString)
                {
                    m_Strings.clear();
                    m_Strings.push_back(string);
                }
                else
                {
                    m_Strings.push_back(string);
                }
                return m_Strings.back().c_str();
            }
            return nullptr;
        }

        void clear() noexcept { m_Strings.clear(); }

    private:
        std::list<std::wstring> m_Strings;
    };

    class SUBOBJECT_HELPER_BASE
    {
    public:
        SUBOBJECT_HELPER_BASE() noexcept { Init(); }
        virtual ~SUBOBJECT_HELPER_BASE() = default;
        virtual D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept = 0;

        void AddToStateObject(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
        {
            m_pSubobject = ContainingStateObject.TrackSubobject(Type(), Data());
        }

    protected:
        virtual void* Data() noexcept = 0;
        void Init() noexcept { m_pSubobject = nullptr; }
        D3D12_STATE_SUBOBJECT* m_pSubobject;
    };

#if(__cplusplus >= 201103L)
    std::list<std::unique_ptr<const SUBOBJECT_HELPER_BASE>> m_OwnedSubobjectHelpers;
#else
    class OWNED_HELPER
    {
    public:
        OWNED_HELPER(const SUBOBJECT_HELPER_BASE* pHelper) noexcept { m_pHelper = pHelper; }
        ~OWNED_HELPER() { delete m_pHelper; }
        const SUBOBJECT_HELPER_BASE* m_pHelper;
    };

    std::list<OWNED_HELPER> m_OwnedSubobjectHelpers;
#endif

    friend class CD3DX12_DXIL_LIBRARY_SUBOBJECT;
    friend class CD3DX12_EXISTING_COLLECTION_SUBOBJECT;
    friend class CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT;
    friend class CD3DX12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    friend class CD3DX12_HIT_GROUP_SUBOBJECT;
    friend class CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT;
    friend class CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT;
    friend class CD3DX12_RAYTRACING_PIPELINE_CONFIG1_SUBOBJECT;
    friend class CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT;
    friend class CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT;
    friend class CD3DX12_STATE_OBJECT_CONFIG_SUBOBJECT;
    friend class CD3DX12_NODE_MASK_SUBOBJECT;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_DXIL_LIBRARY_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_DXIL_LIBRARY_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_DXIL_LIBRARY_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetDXILLibrary(const D3D12_SHADER_BYTECODE* pCode) noexcept
    {
        static constexpr D3D12_SHADER_BYTECODE Default = {};
        m_Desc.DXILLibrary = pCode ? *pCode : Default;
    }

    void DefineExport(
        const LPCWSTR Name,
        const LPCWSTR ExportToRename = nullptr,
        const D3D12_EXPORT_FLAGS Flags = D3D12_EXPORT_FLAG_NONE)
    {
        D3D12_EXPORT_DESC Export;
        Export.Name = m_Strings.LocalCopy(Name);
        Export.ExportToRename = m_Strings.LocalCopy(ExportToRename);
        Export.Flags = Flags;
        m_Exports.push_back(Export);
        m_Desc.pExports = &m_Exports[0]; // using ugly way to get pointer in case .data() is not defined
        m_Desc.NumExports = static_cast<UINT>(m_Exports.size());
    }

    template <size_t N>
    void DefineExports(LPCWSTR (&Exports)[N])
    {
        for (UINT i = 0; i < N; i++)
        {
            DefineExport(Exports[i]);
        }
    }

    void DefineExports(const LPCWSTR* Exports, const UINT N)
    {
        for (UINT i = 0; i < N; i++)
        {
            DefineExport(Exports[i]);
        }
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_DXIL_LIBRARY_DESC&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
        m_Strings.clear();
        m_Exports.clear();
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_DXIL_LIBRARY_DESC m_Desc;
    CD3DX12_STATE_OBJECT_DESC::StringContainer m_Strings;
    std::vector<D3D12_EXPORT_DESC> m_Exports;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_EXISTING_COLLECTION_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_EXISTING_COLLECTION_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_EXISTING_COLLECTION_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetExistingCollection(ID3D12StateObject* pExistingCollection) noexcept
    {
        m_Desc.pExistingCollection = pExistingCollection;
        m_CollectionRef = pExistingCollection;
    }

    void DefineExport(
        const LPCWSTR Name,
        const LPCWSTR ExportToRename = nullptr,
        const D3D12_EXPORT_FLAGS Flags = D3D12_EXPORT_FLAG_NONE)
    {
        D3D12_EXPORT_DESC Export;
        Export.Name = m_Strings.LocalCopy(Name);
        Export.ExportToRename = m_Strings.LocalCopy(ExportToRename);
        Export.Flags = Flags;
        m_Exports.push_back(Export);
        m_Desc.pExports = &m_Exports[0]; // using ugly way to get pointer in case .data() is not defined
        m_Desc.NumExports = static_cast<UINT>(m_Exports.size());
    }

    template <size_t N>
    void DefineExports(LPCWSTR (&Exports)[N])
    {
        for (UINT i = 0; i < N; i++)
        {
            DefineExport(Exports[i]);
        }
    }

    void DefineExports(const LPCWSTR* Exports, const UINT N)
    {
        for (UINT i = 0; i < N; i++)
        {
            DefineExport(Exports[i]);
        }
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_EXISTING_COLLECTION;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_EXISTING_COLLECTION_DESC&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
        m_CollectionRef = nullptr;
        m_Strings.clear();
        m_Exports.clear();
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_EXISTING_COLLECTION_DESC m_Desc;
    D3DX12_COM_PTR<ID3D12StateObject> m_CollectionRef;
    CD3DX12_STATE_OBJECT_DESC::StringContainer m_Strings;
    std::vector<D3D12_EXPORT_DESC> m_Exports;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_SUBOBJECT_TO_EXPORTS_ASSOCIATION_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetSubobjectToAssociate(const D3D12_STATE_SUBOBJECT& SubobjectToAssociate) noexcept
    {
        m_Desc.pSubobjectToAssociate = &SubobjectToAssociate;
    }

    void AddExport(const LPCWSTR Export)
    {
        m_Desc.NumExports++;
        m_Exports.push_back(m_Strings.LocalCopy(Export));
        m_Desc.pExports = &m_Exports[0]; // using ugly way to get pointer in case .data() is not defined
    }

    template <size_t N>
    void AddExports(LPCWSTR (&Exports)[N])
    {
        for (UINT i = 0; i < N; i++)
        {
            AddExport(Exports[i]);
        }
    }

    void AddExports(const LPCWSTR* Exports, const UINT N)
    {
        for (UINT i = 0; i < N; i++)
        {
            AddExport(Exports[i]);
        }
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
        m_Strings.clear();
        m_Exports.clear();
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION m_Desc;
    CD3DX12_STATE_OBJECT_DESC::StringContainer m_Strings;
    std::vector<LPCWSTR> m_Exports;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION() noexcept
    {
        Init();
    }

    CD3DX12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetSubobjectNameToAssociate(const LPCWSTR SubobjectToAssociate)
    {
        m_Desc.SubobjectToAssociate = m_SubobjectName.LocalCopy(SubobjectToAssociate, true);
    }

    void AddExport(const LPCWSTR Export)
    {
        m_Desc.NumExports++;
        m_Exports.push_back(m_Strings.LocalCopy(Export));
        m_Desc.pExports = &m_Exports[0]; // using ugly way to get pointer in case .data() is not defined
    }

    template <size_t N>
    void AddExports(LPCWSTR (&Exports)[N])
    {
        for (UINT i = 0; i < N; i++)
        {
            AddExport(Exports[i]);
        }
    }

    void AddExports(const LPCWSTR* Exports, const UINT N)
    {
        for (UINT i = 0; i < N; i++)
        {
            AddExport(Exports[i]);
        }
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
        m_Strings.clear();
        m_SubobjectName.clear();
        m_Exports.clear();
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_DXIL_SUBOBJECT_TO_EXPORTS_ASSOCIATION m_Desc;
    CD3DX12_STATE_OBJECT_DESC::StringContainer m_Strings;
    CD3DX12_STATE_OBJECT_DESC::StringContainer m_SubobjectName;
    std::vector<LPCWSTR> m_Exports;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_HIT_GROUP_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_HIT_GROUP_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_HIT_GROUP_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetHitGroupExport(const LPCWSTR exportName)
    {
        m_Desc.HitGroupExport = m_Strings[0].LocalCopy(exportName, true);
    }

    void SetHitGroupType(const D3D12_HIT_GROUP_TYPE Type) noexcept { m_Desc.Type = Type; }

    void SetAnyHitShaderImport(const LPCWSTR importName)
    {
        m_Desc.AnyHitShaderImport = m_Strings[1].LocalCopy(importName, true);
    }

    void SetClosestHitShaderImport(const LPCWSTR importName)
    {
        m_Desc.ClosestHitShaderImport = m_Strings[2].LocalCopy(importName, true);
    }

    void SetIntersectionShaderImport(const LPCWSTR importName)
    {
        m_Desc.IntersectionShaderImport = m_Strings[3].LocalCopy(importName, true);
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_HIT_GROUP_DESC&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
        for (UINT i = 0; i < m_NumStrings; i++)
        {
            m_Strings[i].clear();
        }
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_HIT_GROUP_DESC m_Desc;
    static constexpr UINT m_NumStrings = 4;
    CD3DX12_STATE_OBJECT_DESC::StringContainer
    m_Strings[m_NumStrings]; // one string for every entrypoint name
};

//------------------------------------------------------------------------------------------------
class CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_RAYTRACING_SHADER_CONFIG_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void Config(const UINT MaxPayloadSizeInBytes, const UINT MaxAttributeSizeInBytes) noexcept
    {
        m_Desc.MaxPayloadSizeInBytes = MaxPayloadSizeInBytes;
        m_Desc.MaxAttributeSizeInBytes = MaxAttributeSizeInBytes;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_RAYTRACING_SHADER_CONFIG&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_RAYTRACING_SHADER_CONFIG m_Desc;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_RAYTRACING_PIPELINE_CONFIG_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void Config(const UINT MaxTraceRecursionDepth) noexcept
    {
        m_Desc.MaxTraceRecursionDepth = MaxTraceRecursionDepth;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_RAYTRACING_PIPELINE_CONFIG&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_RAYTRACING_PIPELINE_CONFIG m_Desc;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_RAYTRACING_PIPELINE_CONFIG1_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_RAYTRACING_PIPELINE_CONFIG1_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_RAYTRACING_PIPELINE_CONFIG1_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void Config(const UINT MaxTraceRecursionDepth, const D3D12_RAYTRACING_PIPELINE_FLAGS Flags) noexcept
    {
        m_Desc.MaxTraceRecursionDepth = MaxTraceRecursionDepth;
        m_Desc.Flags = Flags;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG1;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_RAYTRACING_PIPELINE_CONFIG1&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_RAYTRACING_PIPELINE_CONFIG1 m_Desc;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_GLOBAL_ROOT_SIGNATURE_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetRootSignature(ID3D12RootSignature* pRootSig) noexcept
    {
        m_pRootSig = pRootSig;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator ID3D12RootSignature*() const noexcept { return D3DX12_COM_PTR_GET(m_pRootSig); }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_pRootSig = nullptr;
    }

    void* Data() noexcept override { return D3DX12_COM_PTR_ADDRESSOF(m_pRootSig); }
    D3DX12_COM_PTR<ID3D12RootSignature> m_pRootSig;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_LOCAL_ROOT_SIGNATURE_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetRootSignature(ID3D12RootSignature* pRootSig) noexcept
    {
        m_pRootSig = pRootSig;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator ID3D12RootSignature*() const noexcept { return D3DX12_COM_PTR_GET(m_pRootSig); }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_pRootSig = nullptr;
    }

    void* Data() noexcept override { return D3DX12_COM_PTR_ADDRESSOF(m_pRootSig); }
    D3DX12_COM_PTR<ID3D12RootSignature> m_pRootSig;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_STATE_OBJECT_CONFIG_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_STATE_OBJECT_CONFIG_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_STATE_OBJECT_CONFIG_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetFlags(const D3D12_STATE_OBJECT_FLAGS Flags) noexcept
    {
        m_Desc.Flags = Flags;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_STATE_OBJECT_CONFIG;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_STATE_OBJECT_CONFIG&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_STATE_OBJECT_CONFIG m_Desc;
};

//------------------------------------------------------------------------------------------------
class CD3DX12_NODE_MASK_SUBOBJECT
    : public CD3DX12_STATE_OBJECT_DESC::SUBOBJECT_HELPER_BASE
{
public:
    CD3DX12_NODE_MASK_SUBOBJECT() noexcept
    {
        Init();
    }

    CD3DX12_NODE_MASK_SUBOBJECT(CD3DX12_STATE_OBJECT_DESC& ContainingStateObject)
    {
        Init();
        AddToStateObject(ContainingStateObject);
    }

    void SetNodeMask(const UINT NodeMask) noexcept
    {
        m_Desc.NodeMask = NodeMask;
    }

    D3D12_STATE_SUBOBJECT_TYPE Type() const noexcept override
    {
        return D3D12_STATE_SUBOBJECT_TYPE_NODE_MASK;
    }

    operator const D3D12_STATE_SUBOBJECT&() const noexcept { return *m_pSubobject; }
    operator const D3D12_NODE_MASK&() const noexcept { return m_Desc; }

private:
    void Init() noexcept
    {
        SUBOBJECT_HELPER_BASE::Init();
        m_Desc = {};
    }

    void* Data() noexcept override { return &m_Desc; }
    D3D12_NODE_MASK m_Desc;
};

#undef D3DX12_COM_PTR
#undef D3DX12_COM_PTR_GET
#undef D3DX12_COM_PTR_ADDRESSOF
#endif // #ifndef D3DX12_NO_STATE_OBJECT_HELPERS

#endif // defined( __cplusplus )

#endif //__D3DX12_H__
