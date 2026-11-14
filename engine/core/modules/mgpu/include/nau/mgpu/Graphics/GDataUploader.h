#pragma once
#include "nau/mgpu/Allocator/pch.h"
#include <deque>
#include <memory>
#include <wrl.h>
#include <nau/mgpu/Allocator/MemoryAllocator.h>
#include <d3d12.h>

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;

    struct UploadAllocation
    {
        BYTE* CPU;
        D3D12_GPU_VIRTUAL_ADDRESS GPU;
        ID3D12Resource& d3d12Resource;
        size_t Offset;
    };

    class GDevice;

    class GDataUploader
    {
    public:
        explicit GDataUploader(const std::shared_ptr<GDevice>& device, size_t pageSize = 512);

        virtual ~GDataUploader();

        size_t GetPageSize() const;

        UploadAllocation Allocate(size_t sizeInBytes, size_t alignment);

        void Reset() const;
        void Clear();

    private:
        std::shared_ptr<GDevice> device;

        struct UploadMemoryPage
        {
            UploadMemoryPage(const std::shared_ptr<GDevice>& device, size_t sizeInBytes);
            ~UploadMemoryPage();

            bool HasSpace(size_t sizeInBytes, size_t alignment) const;

            UploadAllocation Allocate(size_t sizeInBytes, size_t alignment);

            void Reset();

        private:
            ComPtr<ID3D12Resource> d3d12Resource;

            BYTE* CPUPtr;
            D3D12_GPU_VIRTUAL_ADDRESS GPUPtr;

            size_t PageSize;
            size_t Offset;
        };


        std::list<std::shared_ptr<UploadMemoryPage>> pages{};


        std::shared_ptr<UploadMemoryPage> CreatePage(uint32_t pageSize) const;


        size_t PageSize;
    };
}
