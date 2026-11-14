#pragma once
#include <d3d12.h>
#include <wrl.h>
#include <string>
#include <memory>
#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/mgpu/Graphics/GDevice.h"

namespace nau::PEPEngine::Graphics
{
    class GResource
    {
    public:
        GResource(const std::wstring& name = L"");
        GResource(const std::shared_ptr<GDevice>& device, const D3D12_RESOURCE_DESC& resourceDesc,
                  const std::wstring& name = L"",
                  const D3D12_CLEAR_VALUE* clearValue = nullptr,
                  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON,
                  const D3D12_HEAP_PROPERTIES& heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                  D3D12_HEAP_FLAGS heapFlags = D3D12_HEAP_FLAG_NONE);
        GResource(const std::shared_ptr<GDevice>& device, const D3D12_RESOURCE_DESC& resourceDesc,
                  const ComPtr<ID3D12Heap>& heap,
                  const std::wstring& name = L"",
                  const D3D12_CLEAR_VALUE* clearValue = nullptr,
                  D3D12_RESOURCE_STATES initState = D3D12_RESOURCE_STATE_COMMON);
        GResource(const std::shared_ptr<GDevice>& device, ComPtr<ID3D12Resource>& resource,
                  const std::wstring& name = L"");
        GResource(const GResource& copy);
        GResource(GResource&& move);

        GResource& operator=(const GResource& other);
        GResource& operator=(GResource&& other) noexcept;

        virtual ~GResource();

        bool IsValid() const;


        ComPtr<ID3D12Resource> GetD3D12Resource() const;

        D3D12_RESOURCE_DESC GetD3D12ResourceDesc() const;

        virtual void SetD3D12Resource(std::shared_ptr<GDevice> device,
                                      ComPtr<ID3D12Resource> d3d12Resource,
                                      const D3D12_CLEAR_VALUE* clearValue = nullptr);


        void CreateShaderResourceView(const D3D12_SHADER_RESOURCE_VIEW_DESC* srvDesc, const GDescriptor* memory,
                                      size_t offset = 0) const;
        void CreateUnorderedAccessView(const D3D12_UNORDERED_ACCESS_VIEW_DESC* uavDesc, const GDescriptor* memory,
                                       size_t offset = 0,
                                       const ComPtr<ID3D12Resource>& counterResource = nullptr) const;
        void CreateRenderTargetView(const D3D12_RENDER_TARGET_VIEW_DESC* rtvDesc, const GDescriptor* memory,
                                    size_t offset = 0) const;
        void CreateDepthStencilView(const D3D12_DEPTH_STENCIL_VIEW_DESC* dsvDesc, const GDescriptor* memory,
                                    size_t offset = 0) const;


        void SetName(const std::wstring& name);


        virtual void Reset();

        std::shared_ptr<GDevice>& GetDevice();
        std::wstring GetName() const;

    protected:
        std::shared_ptr<GDevice> device;
        ComPtr<ID3D12Resource> dxResource;
        std::unique_ptr<D3D12_CLEAR_VALUE> clearValue;
        std::wstring resourceName;
        D3D12_RESOURCE_DESC description;
    };
}
