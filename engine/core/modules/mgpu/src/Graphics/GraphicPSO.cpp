#include "nau/mgpu/Graphics/GraphicPSO.h"

#include <algorithm>

#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/mgpu/Graphics/GDevice.h"

namespace nau::PEPEngine::Graphics
{
    GraphicPSO::GraphicPSO(RenderMode type) : PSO(), type(type)
    {
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.pRootSignature = nullptr;
    }

    GraphicPSO::GraphicPSO(const GRootSignature& rs, RenderMode type) : PSO(rs), type(type)
    {
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.pRootSignature = rs.GetNativeSignature().Get();
    }

    void GraphicPSO::SetPsoDesc(const D3D12_GRAPHICS_PIPELINE_STATE_DESC& desc)
    {
        psoDesc = desc;
        if (desc.pRootSignature != nullptr)
        {
            rs = GRootSignature(desc.pRootSignature);
        }
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC GraphicPSO::GetPsoDescription() const
    {
        return psoDesc;
    }

    void GraphicPSO::SetInputLayout(const D3D12_INPUT_LAYOUT_DESC layoutDesc)
    {
        psoDesc.InputLayout = layoutDesc;
    }

    void GraphicPSO::SetRasterizationState(const D3D12_RASTERIZER_DESC& rastState)
    {
        psoDesc.RasterizerState = rastState;
    }

    void GraphicPSO::SetRenderTargetBlendState(const UINT index, const D3D12_RENDER_TARGET_BLEND_DESC& desc)
    {
        psoDesc.BlendState.RenderTarget[index] = desc;
    }

    void GraphicPSO::SetBlendState(const D3D12_BLEND_DESC& blendDesc)
    {
        psoDesc.BlendState = blendDesc;
    }

    void GraphicPSO::SetDepthStencilState(const D3D12_DEPTH_STENCIL_DESC& ddsDesc)
    {
        psoDesc.DepthStencilState = ddsDesc;
    }

    void GraphicPSO::SetDSVFormat(const DXGI_FORMAT format)
    {
        psoDesc.DSVFormat = format;
    }

    void GraphicPSO::SetSampleMask(const UINT sampleMask)
    {
        psoDesc.SampleMask = sampleMask;
    }

    void GraphicPSO::SetPrimitiveType(const D3D12_PRIMITIVE_TOPOLOGY_TYPE primitiveType)
    {
        psoDesc.PrimitiveTopologyType = primitiveType;
    }

    void GraphicPSO::SetRenderTargetsCount(const UINT count)
    {
        psoDesc.NumRenderTargets = count;
    }

    void GraphicPSO::SetRTVFormat(UINT index, const DXGI_FORMAT format)
    {
        index = std::clamp(index, 0U, 7U);
        psoDesc.RTVFormats[index] = format;
    }

    void GraphicPSO::SetSampleCount(const UINT count)
    {
        psoDesc.SampleDesc.Count = count;
    }

    void GraphicPSO::SetSampleQuality(const UINT quality)
    {
        psoDesc.SampleDesc.Quality = quality;
    }

    void GraphicPSO::SetShader(const GShader* shader)
    {
        switch (shader->GetType())
        {
        case VertexShader:
            {
                psoDesc.VS = shader->GetShaderResource();
                break;
            }
        case PixelShader:
            {
                psoDesc.PS = shader->GetShaderResource();
                break;
            }
        case DomainShader:
            {
                psoDesc.DS = shader->GetShaderResource();
                break;
            }
        case GeometryShader:
            {
                psoDesc.GS = shader->GetShaderResource();
                break;
            }
        case HullShader:
            {
                psoDesc.HS = shader->GetShaderResource();
                break;
            }
        case ComputeShader:
            {
                assert("Bad Shader!!");
            }
        }
    }

    RenderMode GraphicPSO::GetRenderMode() const
    {
        return type;
    }

    void GraphicPSO::Initialize(const std::shared_ptr<GDevice>& device)
    {
        if (IsInitialized()) return;

        const auto result = device->GetDXDevice()->CreateGraphicsPipelineState(&psoDesc,
                                                                               IID_PPV_ARGS(&nativePSO));
        ThrowIfFailed(result);
    }

    void GraphicPSO::SetRootSignature(const GRootSignature& rs)
    {
        PSO::SetRootSignature(rs);
        psoDesc.pRootSignature = rs.GetNativeSignature().Get();
    }
}
