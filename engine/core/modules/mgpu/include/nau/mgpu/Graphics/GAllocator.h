#pragma once
#include "nau/mgpu/Allocator/MemoryAllocator.h"
#include "nau/mgpu/Graphics/GDescriptor.h"
#include "nau/mgpu/Utils/d3dx12.h"
#include <cstdint>
#include <mutex>
#include <memory>

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;

    class GDescriptorHeap;
    class GDevice;

    class GAllocator
    {
    public:
        GAllocator(const std::shared_ptr<GDevice>& device, D3D12_DESCRIPTOR_HEAP_TYPE type,
                   uint32_t descriptorsPerPage = 2048);

        virtual ~GAllocator();

        GDescriptor Allocate(uint32_t descriptorCount = 1);

        void ReleaseStaleDescriptors(uint64_t frameNumber);

    private:
        using GraphicMemoryPage = std::vector<std::shared_ptr<GDescriptorHeap>>;

        std::shared_ptr<GDescriptorHeap> CreateAllocatorPage();

        D3D12_DESCRIPTOR_HEAP_TYPE allocatorType;

        uint32_t numDescriptorsPerPage;

        GraphicMemoryPage pages = std::vector<std::shared_ptr<GDescriptorHeap>>();

        std::set<size_t> availablePages{};

        std::mutex allocationMutex;

        std::shared_ptr<GDevice> device;
    };
}
