#include "nau/mgpu/Graphics/ComputePSO.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GShader.h"
#include "nau/mgpu/Utils/d3dUtil.h"

#include <windows.h>
#include <stdio.h>

namespace nau::PEPEngine::Graphics
{
    HRESULT ComputePSO::TryInitialize(const std::shared_ptr<GDevice>& device)
    {
        const HRESULT hr = device->GetDXDevice()->CreateComputePipelineState(&psoDesc, IID_PPV_ARGS(&nativePSO));
        if (FAILED(hr))
        {
            wchar_t buf[320] = {};
            swprintf_s(buf, L"[ComputePSO] CreateComputePipelineState failed: HRESULT 0x%08X  CS bytecode=%zu bytes\n",
                       static_cast<unsigned>(hr), static_cast<size_t>(psoDesc.CS.BytecodeLength));
            OutputDebugStringW(buf);
        }
        return hr;
    }

    void ComputePSO::Initialize(const std::shared_ptr<GDevice>& device)
    {
        const HRESULT hr = TryInitialize(device);
        if (FAILED(hr))
        {
            throw DxException(hr, L"CreateComputePipelineState", AnsiToWString(__FILE__), __LINE__);
        }
    }

    void ComputePSO::SetRootSignature(const GRootSignature& rs)
    {
        PSO::SetRootSignature(rs);
        psoDesc.pRootSignature = rs.GetNativeSignature().Get();
    }

    ComputePSO::ComputePSO() : PSO()
    {
    }

    ComputePSO::ComputePSO(const GRootSignature& RS) : PSO(RS)
    {
        ZeroMemory(&psoDesc, sizeof(D3D12_COMPUTE_PIPELINE_STATE_DESC));
        psoDesc.pRootSignature = rs.GetNativeSignature().Get();
    }


    void ComputePSO::SetShader(const GShader* shader)
    {
        if (shader->GetType() == ComputeShader)
        {
            psoDesc.CS = shader->GetShaderResource();
            return;
        }

        assert("Bad Shader");
    }
}
