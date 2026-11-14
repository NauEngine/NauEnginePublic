#pragma once
#include <d3d12.h>
#include <memory>
#include <mutex>
#include <wrl/client.h>
#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/mgpu/Graphics/GDescriptor.h"
#include "nau/mgpu/Allocator/MemoryAllocator.h"

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;
    using namespace Microsoft::WRL;

    class GDevice;

    class GDescriptorHeap : public std::enable_shared_from_this<GDescriptorHeap>
    {
    public:
        GDescriptorHeap(const class std::shared_ptr<GDevice>& device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                        uint32_t descriptorsCount);

        D3D12_DESCRIPTOR_HEAP_TYPE GetType() const;

        bool HasSpace(uint32_t descriptorCount) const;

        uint32_t FreeHandlerCount() const;

        GDescriptor Allocate(uint32_t descriptorCount);

        void Free(GDescriptor&& descriptorHandle, uint64_t frameNumber);

        void ReleaseStaleDescriptors(uint64_t frameNumber);

        ID3D12DescriptorHeap* GetDirectxHeap() const;

        std::shared_ptr<GDevice> GetDevice() const;

    protected:
        uint32_t ComputeOffset(D3D12_CPU_DESCRIPTOR_HANDLE handle) const;

        void AddNewBlock(uint32_t offset, uint32_t descriptorCount);

        void FreeBlock(uint32_t offset, uint32_t descriptorCount);

    private:
        using OffsetType = size_t;

        using SizeType = size_t;

        struct FreeBlockInfo;


        struct FreeBlockInfo
        {
            FreeBlockInfo(const SizeType size)
                : Size(size)
            {
            }

            SizeType Size;
            custom_multimap<SizeType, custom_map<OffsetType, FreeBlockInfo>::iterator>::iterator FreeListBySizeIt;
        };

        struct DescriptorInfo
        {
            DescriptorInfo(const OffsetType offset, const SizeType size, const uint64_t frame)
                : Offset(offset)
                  , Size(size)
                  , FrameNumber(frame)
            {
            }

            OffsetType Offset;
            SizeType Size;
            uint64_t FrameNumber;
        };

        std::map<OffsetType, FreeBlockInfo> freeListByOffset {};
        std::multimap<SizeType, std::map<OffsetType, FreeBlockInfo>::iterator> freeListBySize =
            std::multimap<SizeType, std::map<OffsetType, FreeBlockInfo>::iterator>();

        std::queue<DescriptorInfo> staleDescriptors {};
        ComPtr<ID3D12DescriptorHeap> descriptorHeap;

        D3D12_DESCRIPTOR_HEAP_TYPE heapType;
        CD3DX12_CPU_DESCRIPTOR_HANDLE baseCPUPtr{};
        CD3DX12_GPU_DESCRIPTOR_HANDLE baseGPUPtr{};

        uint32_t descriptorHandleIncrementSize{};
        uint32_t descriptorCount;
        uint32_t freeHandlesCount;

        std::mutex allocationMutex;

        std::shared_ptr<GDevice> device;
    };
}
