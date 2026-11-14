#pragma once
#include <d3d12.h>
#include <string>
#include <memory>
#include "nau/mgpu/Graphics/GResource.h"


namespace nau::PEPEngine::Graphics
{
    class GCrossAdapterResource
    {
        std::shared_ptr<GResource> primeResource;
        std::shared_ptr<GResource> sharedResource;
        ComPtr<ID3D12Heap> crossAdapterResourceHeap[2];

        bool isInit = false;

    public:
        bool IsInit() const;

        GCrossAdapterResource()
        {
        }

        GCrossAdapterResource(D3D12_RESOURCE_DESC& desc, const std::shared_ptr<GDevice>& primeDevice,
                              const std::shared_ptr<GDevice>
                              & sharedDevice, const std::wstring& name = L"",
                              D3D12_RESOURCE_STATES initialState = D3D12_RESOURCE_STATE_COMMON,
                              const D3D12_CLEAR_VALUE* clearValue = nullptr);

        const GResource& GetPrimeResource() const;

        const GResource& GetSharedResource() const;
        void Reset();

        void Resize(UINT newWidth, UINT newHeight);
    };
}
