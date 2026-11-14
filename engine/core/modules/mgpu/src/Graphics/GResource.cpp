#include "nau/mgpu/Graphics/GResource.h"

#include <utility>


#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GDescriptor.h"
#include "nau/mgpu/Graphics/GResourceStateTracker.h"

namespace nau::PEPEngine::Graphics
{
    GResource::GResource(const std::wstring& name)
        : resourceName(std::move(name))
    {
    }

    GResource::GResource(const std::shared_ptr<GDevice>& device, const D3D12_RESOURCE_DESC& resourceDesc,
                         const std::wstring& name, const D3D12_CLEAR_VALUE* clearValue,
                         const D3D12_RESOURCE_STATES initState,
                         const D3D12_HEAP_PROPERTIES& heapProp, const D3D12_HEAP_FLAGS heapFlags) : device(device)
    {
        description = resourceDesc;
        if (clearValue)
        {
            this->clearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
        }

        ThrowIfFailed(device->GetDXDevice()->CreateCommittedResource(
            &heapProp,
            heapFlags,
            &resourceDesc,
            initState,
            this->clearValue.get(),
            IID_PPV_ARGS(&dxResource)
        ));

        GResourceStateTracker::AddCurrentState(dxResource.Get(), initState);

        SetName(name);
    }

    GResource::GResource(const std::shared_ptr<GDevice>& device, const D3D12_RESOURCE_DESC& resourceDesc,
                         const ComPtr<ID3D12Heap>& heap, const std::wstring& name, const D3D12_CLEAR_VALUE* clearValue,
                         const D3D12_RESOURCE_STATES initState) : device(device)
    {
        description = resourceDesc;
        if (clearValue)
        {
            this->clearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
        }

        ThrowIfFailed(device->GetDXDevice()->CreatePlacedResource(
            heap.Get(),
            0,
            &resourceDesc,
            initState,
            this->clearValue.get(),
            IID_PPV_ARGS(&dxResource)
        ));

        GResourceStateTracker::AddCurrentState(dxResource.Get(), initState);

        SetName(name);
    }

    GResource::GResource(const std::shared_ptr<GDevice>& device, ComPtr<ID3D12Resource>& resource,
                         const std::wstring& name)
        : device(device), dxResource(std::move(resource))
    {
        description = dxResource->GetDesc();
        SetName(name);
    }

    GResource::GResource(const GResource& copy)
        : device(copy.device)
          , dxResource(copy.dxResource), resourceName(copy.resourceName), description(copy.description)
    {
        if (copy.clearValue)
            clearValue = (std::make_unique<D3D12_CLEAR_VALUE>(*copy.clearValue));
        else
            clearValue = nullptr;
    }

    GResource::GResource(GResource&& move)
        : device(std::move(move.device))
          , dxResource(std::move(move.dxResource))
          , clearValue(std::move(move.clearValue)), resourceName(std::move(move.resourceName)), description(move.description)
    {
    }

    GResource& GResource::operator=(const GResource& other)
    {
        if (this != &other)
        {
            description = other.description;
            dxResource = other.dxResource;
            resourceName = other.resourceName;
            device = other.device;
            if (other.clearValue)
            {
                clearValue = std::make_unique<D3D12_CLEAR_VALUE>(*other.clearValue);
            }
        }
        return *this;
    }

    GResource& GResource::operator=(GResource&& other) noexcept
    {
        if (this != &other)
        {
            description = other.description;
            dxResource = other.dxResource;
            resourceName = other.resourceName;
            device = other.device;
            clearValue = std::move(other.clearValue);
            other.dxResource.Reset();
            other.resourceName.clear();
        }
        return *this;
    }

    GResource::~GResource()
    {
        GResource::Reset();
    }

    bool GResource::IsValid() const
    {
        return (dxResource != nullptr);
    }

    ComPtr<ID3D12Resource> GResource::GetD3D12Resource() const
    {
        return dxResource;
    }


    D3D12_RESOURCE_DESC GResource::GetD3D12ResourceDesc() const
    {
        return description;
    }

    void GResource::SetD3D12Resource(const std::shared_ptr<GDevice> device, ComPtr<ID3D12Resource> d3d12Resource,
                                     const D3D12_CLEAR_VALUE* clearValue)
    {
        dxResource = std::move(d3d12Resource);
        description = dxResource->GetDesc();
        this->device = device;
        if (clearValue)
        {
            this->clearValue.reset();
            this->clearValue = std::make_unique<D3D12_CLEAR_VALUE>(*clearValue);
        }

        SetName(resourceName);
    }


    void GResource::CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, const GDescriptor* memory,
                                             const size_t offset) const
    {
        device->GetDXDevice()->CreateShaderResourceView(dxResource.Get(), srvDesc, memory->GetCPUHandle(static_cast<uint32_t>(offset)));
    }

    void GResource::CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, const GDescriptor* memory,
                                              const size_t offset, const ComPtr<ID3D12Resource>& counterResource) const
    {
        device->GetDXDevice()->CreateUnorderedAccessView(dxResource.Get(), counterResource.Get(), uavDesc,
                                                          memory->GetCPUHandle(static_cast<uint32_t>(offset)));
    }

    void GResource::CreateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, const GDescriptor* memory,
                                           const size_t offset) const
    {
        device->GetDXDevice()->CreateRenderTargetView(dxResource.Get(), rtvDesc, memory->GetCPUHandle(static_cast<uint32_t>(offset)));
    }

    void GResource::CreateDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, const GDescriptor* memory,
                                           const size_t offset) const
    {
        device->GetDXDevice()->CreateDepthStencilView(dxResource.Get(), dsvDesc, memory->GetCPUHandle(static_cast<uint32_t>(offset)));
    }

    void GResource::SetName(const std::wstring& name)
    {
        resourceName = name;
        if (dxResource && !resourceName.empty())
        {
            dxResource->SetName(resourceName.c_str());
        }
    }


    void GResource::Reset()
    {
        if (dxResource)
        {
            dxResource.Reset();
        }

        if (clearValue)
        {
            clearValue.reset();
        }
    }

    std::shared_ptr<GDevice>& GResource::GetDevice()
    {
        return device;
    }

    std::wstring GResource::GetName() const
    {
        return resourceName;
    }
}
