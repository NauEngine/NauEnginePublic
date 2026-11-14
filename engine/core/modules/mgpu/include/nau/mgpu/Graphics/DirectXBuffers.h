#pragma once


#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GBuffer.h"
#include "nau/mgpu/Graphics/GCommandList.h"
using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    class UploadBuffer : public GBuffer
    {
    public:
        UploadBuffer(const std::shared_ptr<GDevice>& device, UINT elementCount, UINT elementByteSize,
                     const std::wstring& name = L"", D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_NONE,
                     D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ,
                     const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));

        UploadBuffer(const std::shared_ptr<GDevice>& device, UINT elementCount, UINT elementByteSize, UINT aligment,
                     const std::wstring& name = L"", D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_NONE,
                     D3D12_RESOURCE_STATES state = D3D12_RESOURCE_STATE_GENERIC_READ,
                     const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD));

        UploadBuffer(const UploadBuffer& rhs) = delete;
        UploadBuffer& operator=(const UploadBuffer& rhs) = delete;

        ~UploadBuffer() override;

        void CopyData(int elementIndex, const void* data, size_t size) const;
        void ReadData(int elementIndex, void* data, size_t size) const;

    protected:
        BYTE* mappedData = nullptr;
    };


    template <typename T>
    class ConstantUploadBuffer : public virtual UploadBuffer
    {
    public:
        // Constant buffer elements need to be multiples of 256 bytes.
        // This is because the hardware can only view constant data
        // at m*256 byte offsets and of n*256 byte lengths.
        // typedef struct D3D12_CONSTANT_BUFFER_VIEW_DESC {
        //  UINT64 OffsetInBytes; // multiple of 256
        //  UINT  SizeInBytes;  // multiple of 256
        // } D3D12_CONSTANT_BUFFER_VIEW_DESC;
        ConstantUploadBuffer(const std::shared_ptr<GDevice>& device, const UINT elementCount,
                             const std::wstring& name = L"") :
            UploadBuffer(
                device, elementCount, d3dUtil::CalcConstantBufferByteSize(sizeof(T)), name)
        {
        }

        void CopyData(const int elementIndex, const T& data)
        {
            UploadBuffer::CopyData(elementIndex, &data, sizeof(T));
        }
    };

    template <typename T>
    class ReadBackBuffer : public virtual GBuffer
    {
        // Get the timestamp values from the result buffers.
        D3D12_RANGE readRange = {};
        const D3D12_RANGE emptyRange = {};
        BYTE* mappedData = nullptr;

    public:
        ReadBackBuffer(const std::shared_ptr<GDevice>& device, const UINT elementCount,
                       const std::wstring& name = L"") :
            GBuffer(
                device, elementCount, (sizeof(T)), name, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_COPY_DEST,
                CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK))
        {
        }

        void ReadData(const int elementIndex, T& data)
        {
            ThrowIfFailed(dxResource->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

            data = reinterpret_cast<T*>(mappedData + elementIndex * sizeof(T))[0];

            dxResource->Unmap(0, nullptr);
        }
    };


    template <typename T>
    class StructuredUploadBuffer : public virtual UploadBuffer
    {
    public:
        StructuredUploadBuffer(const std::shared_ptr<GDevice>& device, const UINT elementCount,
                               const std::wstring& name = L"",
                               const D3D12_RESOURCE_FLAGS flag = D3D12_RESOURCE_FLAG_NONE) :
            UploadBuffer(
                device, elementCount, static_cast<UINT>(sizeof(T)), name, flag)
        {
        }

        void CopyData(const int elementIndex, const T& data)
        {
            UploadBuffer::CopyData(elementIndex, &data, sizeof(T));
        }

        void ReadData(const int elementIndex, T& data)
        {
            UploadBuffer::ReadData(elementIndex, &data, sizeof(T));
        }
    };

    template <typename T>
    class CounteredStructBuffer : public GBuffer
    {
        std::shared_ptr<UploadBuffer> upload;
        std::shared_ptr<ReadBackBuffer<DWORD>> read;

    public:
        CounteredStructBuffer(const std::shared_ptr<GDevice>& device, const UINT elementCount,
                              const std::wstring& name = L"") :
            GBuffer(
                device, static_cast<UINT>(sizeof(T)), elementCount, D3D12_UAV_COUNTER_PLACEMENT_ALIGNMENT, name,
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COMMON,
                CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT))
        {
            upload = std::make_shared<UploadBuffer>(device, 1, static_cast<UINT>(sizeof(DWORD)), name + L" Upload");
            read = std::make_shared<ReadBackBuffer<DWORD>>(device, 1, name + L" ReadBack");
        }

        CounteredStructBuffer(const CounteredStructBuffer& rhs) = delete;
        CounteredStructBuffer& operator=(const CounteredStructBuffer& rhs) = delete;

        void SetCounterValue(const std::shared_ptr<GCommandList>& cmdList, const DWORD value) const
        {
            cmdList->TransitionBarrier(dxResource, D3D12_RESOURCE_STATE_COPY_DEST);
            cmdList->FlushResourceBarriers();

            upload->CopyData(0, &value, sizeof(DWORD));
            cmdList->CopyBufferRegion(dxResource, bufferSize - sizeof(DWORD), upload->GetD3D12Resource(), 0,
                                      sizeof(DWORD), false);
        }

        void CopyCounterForRead(const std::shared_ptr<GCommandList>& cmdList) const
        {
            cmdList->CopyBufferRegion(read->GetD3D12Resource(), 0, dxResource, bufferSize - sizeof(DWORD),
                                      sizeof(DWORD), true);
        }

        void ReadCounter(DWORD* counterValue) const
        {
            read->ReadData(0, *counterValue);
        }
    };
}
