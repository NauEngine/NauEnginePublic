#pragma once


#include "nau/mgpu/Allocator/MemoryAllocator.h"
#include <d3d12.h>
#include <wrl/client.h>

#include "nau/mgpu/Utils/d3dx12.h"

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;

    class GDevice;

    class GRootSignature
    {
        ComPtr<ID3D12RootSignature> signature;
        D3D12_ROOT_SIGNATURE_DESC rootSigDesc;


        std::vector<CD3DX12_ROOT_PARAMETER> slotRootParameters = std::vector<
            CD3DX12_ROOT_PARAMETER>();

        void AddParameter(const CD3DX12_ROOT_PARAMETER& parameter);

        std::vector<D3D12_STATIC_SAMPLER_DESC> staticSampler = std::vector<
            D3D12_STATIC_SAMPLER_DESC>();

        std::vector<uint32_t> descriptorPerTableCount = std::vector<uint32_t>();

        uint32_t samplerTableBitMask;

        uint32_t descriptorTableBitMask;

        bool IsInitialize = false;

    public:
        GRootSignature() = default;
        GRootSignature(ID3D12RootSignature* rootSignature);
        virtual ~GRootSignature();

        const D3D12_ROOT_SIGNATURE_DESC& GetRootSignatureDesc() const;

        void AddDescriptorParameter(const CD3DX12_DESCRIPTOR_RANGE* rangeParameters, UINT size,
                                    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        void AddConstantBufferParameter(UINT shaderRegister, UINT registerSpace = 0,
                                        D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        void AddConstantParameter(UINT valueCount, UINT shaderRegister, UINT registerSpace = 0,
                                  D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        void AddShaderResourceView(UINT shaderRegister, UINT registerSpace = 0,
                                   D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        void AddUnorderedAccessView(UINT shaderRegister, UINT registerSpace = 0,
                                    D3D12_SHADER_VISIBILITY visibility = D3D12_SHADER_VISIBILITY_ALL);

        void AddStaticSampler(const CD3DX12_STATIC_SAMPLER_DESC& sampler);
        void AddStaticSampler(const D3D12_STATIC_SAMPLER_DESC& sampler);;

        void SetDesc(const D3D12_ROOT_SIGNATURE_DESC& desc);

        void Initialize(const std::shared_ptr<GDevice>& device, bool force = false,
                        D3D12_ROOT_SIGNATURE_FLAGS flags =
                            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

        ComPtr<ID3D12RootSignature> GetNativeSignature() const;
    };
}
