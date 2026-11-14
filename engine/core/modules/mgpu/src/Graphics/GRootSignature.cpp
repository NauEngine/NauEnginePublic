#include "nau/mgpu/Graphics/GRootSignature.h"


#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GDevice.h"

namespace nau::PEPEngine::Graphics
{
    static std::vector<CD3DX12_STATIC_SAMPLER_DESC> GetStaticSamplers()
    {
        const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
            0, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
            1, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
            2, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
            3, // shaderRegister
            D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
            4, // shaderRegister
            D3D12_FILTER_ANISOTROPIC, // filter
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, // addressW
            0.0f, // mipLODBias
            8); // maxAnisotropy

        const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
            5, // shaderRegister
            D3D12_FILTER_ANISOTROPIC, // filter
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_CLAMP, // addressW
            0.0f, // mipLODBias
            8); // maxAnisotropy

        const CD3DX12_STATIC_SAMPLER_DESC shadow(
            6, // shaderRegister
            D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT, // filter
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressU
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressV
            D3D12_TEXTURE_ADDRESS_MODE_BORDER, // addressW
            0.0f, // mipLODBias
            16, // maxAnisotropy
            D3D12_COMPARISON_FUNC_LESS_EQUAL,
            D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);


        return std::vector<CD3DX12_STATIC_SAMPLER_DESC>{
            pointWrap, pointClamp,
            linearWrap, linearClamp,
            anisotropicWrap, anisotropicClamp, shadow
        };
    }

    inline static std::vector<CD3DX12_STATIC_SAMPLER_DESC> staticSamplersVector = GetStaticSamplers();

    GRootSignature::GRootSignature(ID3D12RootSignature* rootSignature)
    {
        this->signature = rootSignature;
    }

    GRootSignature::~GRootSignature()
    {
        slotRootParameters.clear();
        staticSampler.clear();
    }

    const D3D12_ROOT_SIGNATURE_DESC& GRootSignature::GetRootSignatureDesc() const
    {
        return rootSigDesc;
    }

    void GRootSignature::AddParameter(const CD3DX12_ROOT_PARAMETER& parameter)
    {
        slotRootParameters.push_back(parameter);
    }

    void GRootSignature::AddDescriptorParameter(const CD3DX12_DESCRIPTOR_RANGE* rangeParameters, const UINT size,
                                                const D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER slotParameter;
        slotParameter.InitAsDescriptorTable(size, rangeParameters, visibility);
        AddParameter(slotParameter);
    }

    void GRootSignature::AddConstantBufferParameter(const UINT shaderRegister, const UINT registerSpace,
                                                    const D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER slotParameter;
        slotParameter.InitAsConstantBufferView(shaderRegister, registerSpace, visibility);
        AddParameter(slotParameter);
    }

    void GRootSignature::AddConstantParameter(const UINT valueCount, const UINT shaderRegister,
                                              const UINT registerSpace,
                                              const D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER slotParameter;
        slotParameter.InitAsConstants(valueCount, shaderRegister, registerSpace, visibility);
        AddParameter(slotParameter);
    }

    void GRootSignature::AddShaderResourceView(const UINT shaderRegister, const UINT registerSpace,
                                               const D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER slotParameter;
        slotParameter.InitAsShaderResourceView(shaderRegister, registerSpace, visibility);
        AddParameter(slotParameter);
    }

    void GRootSignature::AddUnorderedAccessView(const UINT shaderRegister, const UINT registerSpace,
                                                const D3D12_SHADER_VISIBILITY visibility)
    {
        CD3DX12_ROOT_PARAMETER slotParameter;
        slotParameter.InitAsUnorderedAccessView(shaderRegister, registerSpace, visibility);
        AddParameter(slotParameter);
    }

    void GRootSignature::AddStaticSampler(const CD3DX12_STATIC_SAMPLER_DESC& sampler)
    {
        staticSampler.push_back(sampler);
    }

    void GRootSignature::AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler)
    {
        staticSampler.push_back(sampler);
    }

    void GRootSignature::SetDesc(const D3D12_ROOT_SIGNATURE_DESC& desc)
    {
        rootSigDesc = desc;
    }


    void GRootSignature::Initialize(const std::shared_ptr<GDevice>& device, const bool force,
                                    const D3D12_ROOT_SIGNATURE_FLAGS flags)
    {
        if (IsInitialize) return;

        if (!force)
        {
            if (!staticSampler.empty())
            {
                rootSigDesc = CD3DX12_ROOT_SIGNATURE_DESC(static_cast<UINT>(slotRootParameters.size()), slotRootParameters.data(),
                                                           static_cast<UINT>(staticSampler.size()), staticSampler.data(),
                                                          flags);
            }
            else
            {
                // Graphics roots use the shared default static samplers (texture sampling). Pure compute roots
                // built with ALLOW_* cleared must not inherit those samplers—the debug layer can reject
                // CreateComputePipelineState when the root signature exposes unused sampler slots.
                const bool useImplicitGlobalSamplers =
                    (flags & D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT) != 0;
                if (useImplicitGlobalSamplers)
                {
                    rootSigDesc = CD3DX12_ROOT_SIGNATURE_DESC(static_cast<UINT>(slotRootParameters.size()), slotRootParameters.data(),
                                                               static_cast<UINT>(staticSamplersVector.size()), staticSamplersVector.data(),
                                                              flags);
                }
                else
                {
                    rootSigDesc = CD3DX12_ROOT_SIGNATURE_DESC(static_cast<UINT>(slotRootParameters.size()), slotRootParameters.data(),
                                                              0u, nullptr, flags);
                }
            }
        }

        ComPtr<ID3DBlob> serializedRootSig = nullptr;
        ComPtr<ID3DBlob> errorBlob = nullptr;
        const HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
                                                       serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

        if (errorBlob != nullptr)
        {
            OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr);

        ThrowIfFailed(device->GetDXDevice()->CreateRootSignature(
            0,
            serializedRootSig->GetBufferPointer(),
            serializedRootSig->GetBufferSize(),
            IID_PPV_ARGS(signature.GetAddressOf())));

        IsInitialize = true;
    }

    ComPtr<ID3D12RootSignature> GRootSignature::GetNativeSignature() const
    {
        return signature;
    }
}
