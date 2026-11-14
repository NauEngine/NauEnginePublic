#include "nau/mgpu/Graphics/DirectXBuffers.h"
#include "nau/mgpu/Graphics/GResourceStateTracker.h"

namespace nau::PEPEngine::Graphics
{
    using namespace Utils;

    UploadBuffer::UploadBuffer(const std::shared_ptr<GDevice>& device, const UINT elementCount,
                               const UINT elementByteSize,
                               const std::wstring& name, const D3D12_RESOURCE_FLAGS flag,
                               const D3D12_RESOURCE_STATES state,
                               const D3D12_HEAP_PROPERTIES& heapProp) :
        GBuffer(device, elementByteSize, elementCount, name, flag,
                state, heapProp)
    {
        ThrowIfFailed(dxResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
    }

    UploadBuffer::UploadBuffer(const std::shared_ptr<GDevice>& device, const UINT elementCount,
                               const UINT elementByteSize,
                               const UINT aligment,
                               const std::wstring& name, const D3D12_RESOURCE_FLAGS flag,
                               const D3D12_RESOURCE_STATES state,
                               const D3D12_HEAP_PROPERTIES& heapProp) :
        GBuffer(device, elementByteSize, elementCount, aligment, name, flag,
                state, heapProp)
    {
        ThrowIfFailed(dxResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));
    }

    UploadBuffer::~UploadBuffer()
    {
        if (dxResource != nullptr)
            dxResource->Unmap(0, nullptr);
        mappedData = nullptr;
    }

    void UploadBuffer::CopyData(const int elementIndex, const void* data, const size_t size) const
    {
        memcpy(&mappedData[elementIndex * stride], data, size);
    }

    void UploadBuffer::ReadData(const int elementIndex, void* data, const size_t size) const
    {
        memcpy(data, &mappedData[elementIndex * stride], size);
    }
}
