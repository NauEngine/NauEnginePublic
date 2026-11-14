#include "nau/mgpu/Graphics/GDataUploader.h"


#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GDevice.h"

namespace nau::PEPEngine::Graphics
{
    GDataUploader::GDataUploader(const std::shared_ptr<GDevice>& device, const size_t pageSize)
        : device(device), PageSize(pageSize)
    {
    }

    GDataUploader::~GDataUploader()
    {
        for (auto&& page : pages)
        {
            page.reset();
        }

        pages.clear();
    }

    UploadAllocation GDataUploader::Allocate(const size_t sizeInBytes, const size_t alignment)
    {
        for (auto&& page : pages)
        {
            if (page->HasSpace(sizeInBytes, alignment))
            {
                return page->Allocate(sizeInBytes, alignment);
            }
        }

        std::shared_ptr<UploadMemoryPage> page;

        if (PageSize >= sizeInBytes)
        {
            page = CreatePage(static_cast<uint32_t>(PageSize));
        }
        else
        {
            const auto newSize = Math::AlignUp(sizeInBytes, alignment);
            page = CreatePage(static_cast<uint32_t>(newSize));
        }
        pages.push_back(std::move(page));
        return pages.back()->Allocate(sizeInBytes, alignment);
    }

    std::shared_ptr<GDataUploader::UploadMemoryPage> GDataUploader::CreatePage(uint32_t pageSize) const
    {
        return std::make_shared<UploadMemoryPage>(device, pageSize);
    }

    void GDataUploader::Reset() const
    {
        for (auto&& page : pages)
        {
            page->Reset();
        }
    }

    void GDataUploader::Clear()
    {
        for (auto&& page : pages)
        {
            page->Reset();
        }

        pages.clear();
    }


    GDataUploader::UploadMemoryPage::UploadMemoryPage(const std::shared_ptr<GDevice>& device, const size_t sizeInBytes)
        : CPUPtr(nullptr)
          , GPUPtr(static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0))
          , PageSize(sizeInBytes)
          , Offset(0)
    {
        ThrowIfFailed(device->GetDXDevice()->CreateCommittedResource(
            &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
            D3D12_HEAP_FLAG_NONE,
            &CD3DX12_RESOURCE_DESC::Buffer(PageSize),
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(&d3d12Resource)
        ));

        d3d12Resource->SetName(L"Upload Buffer (Page)");

        GPUPtr = d3d12Resource->GetGPUVirtualAddress();
        d3d12Resource->Map(0, nullptr, reinterpret_cast<void**>(&CPUPtr));
    }

    GDataUploader::UploadMemoryPage::~UploadMemoryPage()
    {
        d3d12Resource->Unmap(0, nullptr);
        CPUPtr = nullptr;
        GPUPtr = static_cast<D3D12_GPU_VIRTUAL_ADDRESS>(0);
        d3d12Resource.Reset();
    }

    bool GDataUploader::UploadMemoryPage::HasSpace(const size_t sizeInBytes, const size_t alignment) const
    {
        const size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
        const size_t alignedOffset = Math::AlignUp(Offset, alignment);

        return alignedOffset + alignedSize <= PageSize;
    }

    UploadAllocation GDataUploader::UploadMemoryPage::Allocate(const size_t sizeInBytes, const size_t alignment)
    {
        const size_t alignedSize = Math::AlignUp(sizeInBytes, alignment);
        Offset = Math::AlignUp(Offset, alignment);

        const UploadAllocation allocation
        {
            (CPUPtr) + Offset,
            GPUPtr + Offset,
            *d3d12Resource.Get(),
            Offset
        };

        Offset += alignedSize;

        return allocation;
    }

    void GDataUploader::UploadMemoryPage::Reset()
    {
        Offset = 0;
    }


    size_t GDataUploader::GetPageSize() const
    {
        return PageSize;
    }
}
