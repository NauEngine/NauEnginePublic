#pragma once
#include "nau/mgpu/Graphics/GResource.h"
#include <cstddef>

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    class GCommandList;

    class GBuffer : public GResource
    {
    protected:
        ComPtr<ID3DBlob> bufferCPU;
        UINT count;
        UINT stride;
        DWORD bufferSize = 0;
        D3D12_GPU_VIRTUAL_ADDRESS address{};

    public:
        GBuffer(const std::shared_ptr<GCommandList>& cmdList, size_t elementSize, UINT elementCount, const void* data,
                const std::wstring& name = L"", D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

        GBuffer(const std::shared_ptr<GDevice>& device, size_t elementSize, UINT elementCount,
                const std::wstring& name = L"",
                D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
                const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

        GBuffer(const std::shared_ptr<GDevice>& device, size_t elementSize, UINT elementCount, UINT aligment,
                const std::wstring& name = L"", D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE,
                D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
                const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);

        void LoadData(const void* data, const std::shared_ptr<GCommandList>& cmdList) const;

        GBuffer(const GBuffer& rhs);

        GBuffer& operator=(const GBuffer& a);

        D3D12_GPU_VIRTUAL_ADDRESS GetElementResourceAddress(UINT index) const;

        void Reset() override;

        UINT GetElementCount() const;

        DWORD GetBufferSize() const;

        UINT GetStride() const;

        ComPtr<ID3DBlob> GetCPUResource() const;
    };
}
