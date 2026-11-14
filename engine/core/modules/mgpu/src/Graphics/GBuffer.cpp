#include "nau/mgpu/Graphics/GBuffer.h"
#include "nau/mgpu/Graphics/GCommandList.h"
#include "d3dcompiler.h"
#include "nau/mgpu/Utils/d3dUtil.h"
#include <cassert>
#include <limits>

namespace nau::PEPEngine::Graphics
{
    D3D12_GPU_VIRTUAL_ADDRESS GBuffer::GetElementResourceAddress(const UINT index) const
    {
        return address + (stride * index);
    }

    void GBuffer::Reset()
    {
        GResource::Reset();
        address = 0;
    }

    GBuffer::GBuffer(const std::shared_ptr<GCommandList>& cmdList,
                     const size_t elementSize, const UINT elementCount, const void* data, const std::wstring& name,
                     const D3D12_RESOURCE_FLAGS flags) :
        GResource(cmdList->GetDevice(), CD3DX12_RESOURCE_DESC::Buffer(elementSize * elementCount, flags), name),
        count(elementCount), stride(static_cast<UINT>(elementSize)), bufferSize(stride * count)
    {
        assert(elementSize <= (std::numeric_limits<UINT>::max)());
        address = dxResource->GetGPUVirtualAddress();
        ThrowIfFailed(D3DCreateBlob(bufferSize, bufferCPU.GetAddressOf()));
        LoadData(data, cmdList);
    }

    GBuffer::GBuffer(const std::shared_ptr<GDevice>& device,
                     const size_t elementSize, const UINT elementCount, const std::wstring& name,
                     const D3D12_RESOURCE_FLAGS flags,
                     const D3D12_RESOURCE_STATES initState,
                     const D3D12_HEAP_PROPERTIES& heapProp, const D3D12_HEAP_FLAGS heapFlags) :
        GResource(device, CD3DX12_RESOURCE_DESC::Buffer(elementSize * elementCount, flags), name, nullptr, initState,
                  heapProp, heapFlags), count(elementCount), stride(static_cast<UINT>(elementSize)), bufferSize(stride * count)
    {
        assert(elementSize <= (std::numeric_limits<UINT>::max)());
        address = dxResource->GetGPUVirtualAddress();
        ThrowIfFailed(D3DCreateBlob(bufferSize, bufferCPU.GetAddressOf()));
    }

    GBuffer::GBuffer(const std::shared_ptr<GDevice>& device,
                     const size_t elementSize, const UINT elementCount, const UINT aligment, const std::wstring& name,
                     const D3D12_RESOURCE_FLAGS flags, const D3D12_RESOURCE_STATES initState,
                     const D3D12_HEAP_PROPERTIES& heapProp, const D3D12_HEAP_FLAGS heapFlags) :
        GResource(device, CD3DX12_RESOURCE_DESC::Buffer(
                       (((elementSize * elementCount) + static_cast<size_t>(aligment) - 1) &
                        ~(static_cast<size_t>(aligment) - 1)) + sizeof(UINT), flags), name,
                  nullptr, initState, heapProp, heapFlags), count(elementCount), stride(static_cast<UINT>(elementSize)),
        bufferSize(static_cast<DWORD>(((elementSize * elementCount) + static_cast<size_t>(aligment) - 1) &
                                      ~(static_cast<size_t>(aligment) - 1)) + sizeof(UINT))
    {
        assert(elementSize <= (std::numeric_limits<UINT>::max)());
        address = dxResource->GetGPUVirtualAddress();
        ThrowIfFailed(D3DCreateBlob(bufferSize, bufferCPU.GetAddressOf()));
    }

    void GBuffer::LoadData(const void* data, const std::shared_ptr<GCommandList>& cmdList) const
    {
        CopyMemory(bufferCPU->GetBufferPointer(), data, bufferSize);

        D3D12_SUBRESOURCE_DATA subResourceData = {};
        subResourceData.pData = bufferCPU->GetBufferPointer();
        subResourceData.RowPitch = bufferSize;
        subResourceData.SlicePitch = subResourceData.RowPitch;

        cmdList->UpdateSubresource(dxResource, &subResourceData, 1);
        cmdList->FlushResourceBarriers();
    }


    GBuffer::GBuffer(const GBuffer& rhs) : GResource(rhs)
    {
        this->address = rhs.address;
        this->bufferCPU = rhs.bufferCPU;
        this->bufferSize = rhs.bufferSize;
        this->stride = rhs.stride;
        this->count = rhs.count;
    }

    GBuffer& GBuffer::operator=(const GBuffer& a)
    {
        GResource::operator=(a);
        this->address = a.address;
        this->bufferCPU = a.bufferCPU;
        this->bufferSize = a.bufferSize;
        this->stride = a.stride;
        this->count = a.count;
        return *this;
    }

    UINT GBuffer::GetElementCount() const
    {
        return count;
    }

    DWORD GBuffer::GetBufferSize() const
    {
        return bufferSize;
    }

    UINT GBuffer::GetStride() const
    {
        return stride;
    }

    ComPtr<ID3DBlob> GBuffer::GetCPUResource() const
    {
        return bufferCPU;
    }
}
