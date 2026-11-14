#include "nau/mgpu/Graphics/GDescriptor.h"
#include <utility>
#include "nau/mgpu/Graphics/GCommandQueue.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GDescriptorHeap.h"

namespace nau::PEPEngine::Graphics
{
    GDescriptor::GDescriptor()
        : cpuBase{}
          , gpuBase{}
          , descriptorCount(0)
          , size(0)
          , ownsAllocation(false)
          , page(nullptr)
    {
    }

    GDescriptor::GDescriptor(const D3D12_CPU_DESCRIPTOR_HANDLE descriptor,
                             const D3D12_GPU_DESCRIPTOR_HANDLE gpuDescriptor,
                             const uint32_t count, const uint32_t descriptorSize,
                             const std::shared_ptr<GDescriptorHeap>& heap,
                             const bool ownsAllocation)
        : cpuBase(descriptor)
          , gpuBase(gpuDescriptor)
          , descriptorCount(count)
          , size(descriptorSize)
          , ownsAllocation(ownsAllocation)
          , page(heap)
    {
    }


    GDescriptor::~GDescriptor()
    {
        Free();
    }

    GDescriptor::GDescriptor(GDescriptor&& allocation) noexcept
        : cpuBase(allocation.cpuBase)
          , gpuBase(allocation.gpuBase)
          , descriptorCount(allocation.descriptorCount)
          , size(allocation.size)
          , ownsAllocation(allocation.ownsAllocation)
          , page(std::move(allocation.page))
    {
        allocation.cpuBase.ptr = 0;
        allocation.gpuBase.ptr = 0;
        allocation.descriptorCount = 0;
        allocation.size = 0;
        allocation.ownsAllocation = false;
    }

    GDescriptor& GDescriptor::operator=(GDescriptor&& other) noexcept
    {
        Free();

        cpuBase = other.cpuBase;
        gpuBase = other.gpuBase;
        descriptorCount = other.descriptorCount;
        size = other.size;
        ownsAllocation = other.ownsAllocation;
        page = std::move(other.page);

        other.cpuBase.ptr = 0;
        other.gpuBase.ptr = 0;
        other.descriptorCount = 0;
        other.size = 0;
        other.ownsAllocation = false;

        return *this;
    }

    void GDescriptor::Free()
    {
        if (ownsAllocation && !IsNull() && page)
        {
            const auto frameValue = page->GetDevice()->GetCommandQueue()->GetFenceValue();

            page->Free(std::move(*this), frameValue);

            cpuBase = CD3DX12_CPU_DESCRIPTOR_HANDLE();
            gpuBase = CD3DX12_GPU_DESCRIPTOR_HANDLE();
            descriptorCount = 0;
            size = 0;
            ownsAllocation = false;
            page.reset();
        }
    }

    bool GDescriptor::IsNull() const
    {
        return cpuBase.ptr == 0;
    }

    GDescriptor GDescriptor::Offset(const uint32_t offset) const
    {
        assert(offset < descriptorCount && "Bad GMemmory offset");

        return GDescriptor(CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, offset, size),
                           CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuBase, offset, size), descriptorCount - offset, size, page,
                           false);
    }

    D3D12_CPU_DESCRIPTOR_HANDLE GDescriptor::GetCPUHandle(const uint32_t offset) const
    {
        assert(offset < descriptorCount && "Bad GMemmory offset");

        return CD3DX12_CPU_DESCRIPTOR_HANDLE(cpuBase, offset, size);
    }

    D3D12_GPU_DESCRIPTOR_HANDLE GDescriptor::GetGPUHandle(const uint32_t offset) const
    {
        assert(offset < descriptorCount && "Bad GMemmory offset");

        return CD3DX12_GPU_DESCRIPTOR_HANDLE(gpuBase, offset, size);
    }

    uint32_t GDescriptor::GetDescriptorCount() const
    {
        return descriptorCount;
    }

    D3D12_DESCRIPTOR_HEAP_TYPE GDescriptor::GetType() const
    {
        return page->GetType();
    }

    const std::shared_ptr<GDescriptorHeap>& GDescriptor::GetDescriptorHeap() const
    {
        return page;
    }
}
