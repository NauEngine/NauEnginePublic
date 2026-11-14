#include "nau/mgpu/Graphics/GAllocator.h"
#include "nau/mgpu/Graphics/GDescriptorHeap.h"

namespace nau::PEPEngine::Graphics
{
    GAllocator::GAllocator(const std::shared_ptr<GDevice>& device, const D3D12_DESCRIPTOR_HEAP_TYPE type,
                           const uint32_t descriptorsPerPage)
        : allocatorType(type)
          , numDescriptorsPerPage(descriptorsPerPage), device(device)
    {
    }

    GAllocator::~GAllocator()
    {
        availablePages.clear();
        pages.clear();
    }

    std::shared_ptr<GDescriptorHeap> GAllocator::CreateAllocatorPage()
    {
        auto newPage = std::make_shared<GDescriptorHeap>(device, allocatorType, numDescriptorsPerPage);

        pages.emplace_back(newPage);
        availablePages.insert(pages.size() - 1);

        return newPage;
    }

    GDescriptor GAllocator::Allocate(const uint32_t descriptorCount)
    {
        std::lock_guard<std::mutex> lock(allocationMutex);

        GDescriptor allocation;

        auto iterator = availablePages.begin();
        while (iterator != availablePages.end())
        {
            auto page = pages[*iterator];

            allocation = page->Allocate(descriptorCount);

            if (allocation.IsNull())
            {
                ++iterator;
                continue;
            }

            if (page->FreeHandlerCount() == 0)
            {
                iterator = availablePages.erase(iterator);
            }

            return allocation;
        }

        numDescriptorsPerPage = std::max(numDescriptorsPerPage, descriptorCount);

        auto newPage = CreateAllocatorPage();
        allocation = newPage->Allocate(descriptorCount);

        return allocation;
    }

    void GAllocator::ReleaseStaleDescriptors(const uint64_t frameNumber)
    {
        std::lock_guard<std::mutex> lock(allocationMutex);

        for (size_t i = 0; i < pages.size(); ++i)
        {
            auto page = pages[i];

            page->ReleaseStaleDescriptors(frameNumber);

            if (page->FreeHandlerCount() > 0)
            {
                availablePages.insert(i);
            }
        }
    }
}
