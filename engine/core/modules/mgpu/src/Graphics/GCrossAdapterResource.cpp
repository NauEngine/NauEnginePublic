#include "nau/mgpu/Graphics/GCrossAdapterResource.h"

#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GResource.h"


namespace nau::PEPEngine::Graphics
{
    bool GCrossAdapterResource::IsInit() const
    {
        return isInit;
    }

    GCrossAdapterResource::GCrossAdapterResource(D3D12_RESOURCE_DESC& desc, const std::shared_ptr<GDevice>& primeDevice,
                                                 const std::shared_ptr<GDevice>& sharedDevice, const std::wstring& name,
                                                 const D3D12_RESOURCE_STATES initialState,
                                                 const D3D12_CLEAR_VALUE* clearValue)
    {
        desc.Flags = D3D12_RESOURCE_FLAG_ALLOW_CROSS_ADAPTER;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

        D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout;
        primeDevice->GetDXDevice()->GetCopyableFootprints(&desc, 0, 1, 0, &layout, nullptr, nullptr, nullptr);
        auto textureSize = Align(layout.Footprint.RowPitch * layout.Footprint.Height);


        // Create a heap that will be shared by both adapters.
        CD3DX12_HEAP_DESC heapDesc(
            textureSize,
            D3D12_HEAP_TYPE_DEFAULT,
            0,
            D3D12_HEAP_FLAG_SHARED | D3D12_HEAP_FLAG_SHARED_CROSS_ADAPTER);

        ThrowIfFailed(primeDevice->GetDXDevice()->CreateHeap(&heapDesc, IID_PPV_ARGS(&crossAdapterResourceHeap[0])));


        HANDLE heapHandle = nullptr;
        ThrowIfFailed(primeDevice->GetDXDevice()->CreateSharedHandle(
            crossAdapterResourceHeap[0].Get(),
            nullptr,
            GENERIC_ALL,
            nullptr,
            &heapHandle));

        HRESULT openSharedHandleResult = sharedDevice->GetDXDevice()->OpenSharedHandle(
            heapHandle, IID_PPV_ARGS(&crossAdapterResourceHeap[1]));

        // We can close the handle after opening the cross-adapter shared resource.
        CloseHandle(heapHandle);

        ThrowIfFailed(openSharedHandleResult);

        primeResource = std::make_shared<GResource>(primeDevice, desc, crossAdapterResourceHeap[0], name, clearValue,
                                                    initialState);

        sharedResource = std::make_shared<GResource
        >(sharedDevice, desc, crossAdapterResourceHeap[1], name + L" Shared");

        isInit = true;
    }

    const GResource& GCrossAdapterResource::GetPrimeResource() const
    {
        return *primeResource.get();
    }

    const GResource& GCrossAdapterResource::GetSharedResource() const
    {
        return *sharedResource.get();
    }

    void GCrossAdapterResource::Reset()
    {
        if (!isInit) return;

        if (primeResource)
        {
            primeResource->Reset();
            primeResource.reset();
        }

        if (sharedResource)
        {
            sharedResource->Reset();
            sharedResource.reset();
        }

        isInit = false;
    }

    void GCrossAdapterResource::Resize(const UINT newWidth, const UINT newHeight)
    {
        auto desc = primeResource->GetD3D12ResourceDesc();
        desc.Width = newWidth;
        desc.Height = newHeight;


        primeResource = std::make_shared<GResource>(primeResource->GetDevice(), desc, crossAdapterResourceHeap[0],
                                                    primeResource->GetName());

        sharedResource = std::make_shared<GResource>(sharedResource->GetDevice(), desc, crossAdapterResourceHeap[1],
                                                     sharedResource->GetName());
    }
}
