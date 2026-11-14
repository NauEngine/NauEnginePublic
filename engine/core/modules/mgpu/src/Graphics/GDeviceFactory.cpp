#include "nau/mgpu/Graphics/GDeviceFactory.h"


#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GCommandQueue.h"

#include <algorithm>
#include <cwchar>
#include <string>
#include <mutex>

namespace nau::PEPEngine::Graphics
{
    namespace
    {
        struct AdapterSelectionInfo
        {
            ComPtr<IDXGIAdapter3> Adapter;
            DXGI_ADAPTER_DESC2 Desc = {};
            bool SupportsD3D12 = false;
            bool Uma = false;
            bool CacheCoherentUma = false;
            bool CrossAdapterRowMajorTextureSupported = false;
        };

        bool IsSoftwareAdapter(const DXGI_ADAPTER_DESC2& desc)
        {
            return (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        }

        bool IsSamePhysicalAdapter(const DXGI_ADAPTER_DESC2& first, const DXGI_ADAPTER_DESC2& second)
        {
            return first.VendorId == second.VendorId &&
                first.DeviceId == second.DeviceId &&
                first.SubSysId == second.SubSysId &&
                first.Revision == second.Revision &&
                first.DedicatedVideoMemory == second.DedicatedVideoMemory &&
                first.DedicatedSystemMemory == second.DedicatedSystemMemory &&
                first.SharedSystemMemory == second.SharedSystemMemory &&
                std::wcscmp(first.Description, second.Description) == 0;
        }

        void FillD3D12AdapterInfo(AdapterSelectionInfo& info)
        {
            ComPtr<ID3D12Device> device;
            if (FAILED(D3D12CreateDevice(info.Adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device))))
            {
                return;
            }

            info.SupportsD3D12 = true;

            D3D12_FEATURE_DATA_ARCHITECTURE architecture = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE,
                                                      &architecture, sizeof(architecture))))
            {
                info.Uma = architecture.UMA != FALSE;
                info.CacheCoherentUma = architecture.CacheCoherentUMA != FALSE;
            }

            D3D12_FEATURE_DATA_D3D12_OPTIONS options = {};
            if (SUCCEEDED(device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,
                                                      &options, sizeof(options))))
            {
                info.CrossAdapterRowMajorTextureSupported = options.CrossAdapterRowMajorTextureSupported != FALSE;
            }
        }

        void AddAdapterInfo(std::vector<AdapterSelectionInfo>& adapterInfos, const ComPtr<IDXGIAdapter1>& adapter)
        {
            DXGI_ADAPTER_DESC1 desc1 = {};
            adapter->GetDesc1(&desc1);
            if ((desc1.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
            {
                return;
            }

            ComPtr<IDXGIAdapter3> adapter3;
            if (FAILED(adapter->QueryInterface(IID_PPV_ARGS(&adapter3))))
            {
                return;
            }

            AdapterSelectionInfo info;
            info.Adapter = adapter3;
            ThrowIfFailed(adapter3->GetDesc2(&info.Desc));

            if (IsSoftwareAdapter(info.Desc))
            {
                return;
            }

            for (const auto& existing : adapterInfos)
            {
                if (IsSamePhysicalAdapter(info.Desc, existing.Desc))
                {
                    return;
                }
            }

            FillD3D12AdapterInfo(info);
            if (!info.SupportsD3D12)
            {
                return;
            }

            adapterInfos.emplace_back(info);
        }

        std::vector<AdapterSelectionInfo> EnumerateAdapters(const ComPtr<IDXGIFactory4>& factory)
        {
            std::vector<AdapterSelectionInfo> adapterInfos;

            ComPtr<IDXGIFactory6> factory6;
            if (SUCCEEDED(factory.As(&factory6)))
            {
                for (UINT adapterIndex = 0;; ++adapterIndex)
                {
                    ComPtr<IDXGIAdapter1> adapter;
                    const HRESULT hr = factory6->EnumAdapterByGpuPreference(
                        adapterIndex,
                        DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                        IID_PPV_ARGS(&adapter));

                    if (hr == DXGI_ERROR_NOT_FOUND)
                    {
                        break;
                    }

                    if (SUCCEEDED(hr))
                    {
                        AddAdapterInfo(adapterInfos, adapter);
                    }
                }
            }

            if (adapterInfos.empty())
            {
                for (UINT adapterIndex = 0;; ++adapterIndex)
                {
                    ComPtr<IDXGIAdapter1> adapter;
                    const HRESULT hr = factory->EnumAdapters1(adapterIndex, &adapter);
                    if (hr == DXGI_ERROR_NOT_FOUND)
                    {
                        break;
                    }

                    if (SUCCEEDED(hr))
                    {
                        AddAdapterInfo(adapterInfos, adapter);
                    }
                }
            }

            std::stable_sort(adapterInfos.begin(), adapterInfos.end(),
                             [](const AdapterSelectionInfo& first, const AdapterSelectionInfo& second)
                             {
                                 if (first.Uma != second.Uma)
                                 {
                                     return !first.Uma;
                                 }

                                 return false;
                             });

            return adapterInfos;
        }

        void LogAdapterSelection(const std::vector<AdapterSelectionInfo>& adapterInfos)
        {
            OutputDebugStringW(L"[GDeviceFactory] Hardware adapter selection order:\n");
            for (size_t i = 0; i < adapterInfos.size(); ++i)
            {
                const auto& info = adapterInfos[i];
                std::wstring line = L"  [";
                line += std::to_wstring(i);
                line += L"] ";
                line += info.Desc.Description;
                line += L" | UMA=";
                line += (info.Uma ? L"true" : L"false");
                line += L" | CacheCoherentUMA=";
                line += (info.CacheCoherentUma ? L"true" : L"false");
                line += L" | CrossAdapterRowMajorTexture=";
                line += (info.CrossAdapterRowMajorTextureSupported ? L"true" : L"false");
                line += L"\n";
                OutputDebugStringW(line.c_str());
            }
        }
    }

    // B: A creates the factory, the adapters and the devices during static initialization. Inside Nau that would run
    // at process start (and enable the D3D12 debug layer) even when Mgpu is switched off, so the same steps now run
    // in the same order on first use (EnsureInitialized).
    ComPtr<IDXGIFactory4> GDeviceFactory::dxgiFactory;
    Lazy<bool> GDeviceFactory::isTearingSupport = Lazy<bool>(CheckTearingSupport);
    std::vector<ComPtr<IDXGIAdapter3>> GDeviceFactory::adapters;
    std::vector<std::shared_ptr<GDevice>> GDeviceFactory::hardwareDevices;
    bool GDeviceFactory::debugLayerEnabled = true;
    std::shared_ptr<GDevice> GDeviceFactory::wrapDevice = nullptr;
    std::vector<std::shared_ptr<GDevice>> GDeviceFactory::devicesWithWarp;

    void GDeviceFactory::SetDebugLayerEnabled(const bool enabled)
    {
        debugLayerEnabled = enabled;
    }

    void GDeviceFactory::EnsureInitialized()
    {
        static std::once_flag initialized;
        std::call_once(initialized, []
        {
            dxgiFactory = CreateFactory();
            adapters = GetAdapters();
            hardwareDevices = CreateDevices();
        });
    }

    ComPtr<IDXGIFactory4> GDeviceFactory::CreateFactory()
    {
#if defined(DEBUG) || defined(_DEBUG)
        if (debugLayerEnabled)
        {
            ComPtr<ID3D12Debug> debugController;
            ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
            debugController->EnableDebugLayer();

            ComPtr<ID3D12Debug1> spDebugController1;
            ThrowIfFailed(debugController->QueryInterface(IID_PPV_ARGS(&spDebugController1)));
            //spDebugController1->SetEnableGPUBasedValidation(true);
        }
#endif

        ComPtr<IDXGIFactory4> factory;

        UINT createFactoryFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
        if (debugLayerEnabled)
        {
            createFactoryFlags = DXGI_CREATE_FACTORY_DEBUG;
        }
#endif

        ThrowIfFailed(CreateDXGIFactory2(createFactoryFlags, IID_PPV_ARGS(&factory)));

        return factory;
    }

    std::vector<ComPtr<IDXGIAdapter3>> GDeviceFactory::GetAdapters()
    {
        std::vector<ComPtr<IDXGIAdapter3>> existAdapters;

        auto adapterInfos = EnumerateAdapters(dxgiFactory);
        LogAdapterSelection(adapterInfos);

        existAdapters.reserve(adapterInfos.size());
        for (auto& adapterInfo : adapterInfos)
        {
            existAdapters.emplace_back(adapterInfo.Adapter);
        }

        return existAdapters;
    }

    std::vector<std::shared_ptr<GDevice>> GDeviceFactory::CreateDevices()
    {
        std::vector<std::shared_ptr<GDevice>> devices;
        std::vector<DXGI_ADAPTER_DESC2> uniqueAdapterDescriptions;

        for (const auto& adapter : adapters)
        {
            DXGI_ADAPTER_DESC2 desc = {};
            ThrowIfFailed(adapter->GetDesc2(&desc));

            bool isDuplicate = false;
            for (const auto& uniqueDesc : uniqueAdapterDescriptions)
            {
                if (IsSamePhysicalAdapter(desc, uniqueDesc))
                {
                    isDuplicate = true;
                    break;
                }
            }

            if (isDuplicate)
            {
                continue;
            }

            uniqueAdapterDescriptions.emplace_back(desc);

            auto device = std::make_shared<GDevice>(adapter);
            device->Initialize();
            devices.emplace_back(device);
            //break;
        }

        return devices;
    }

    bool GDeviceFactory::CheckTearingSupport()
    {
        BOOL allowTearing = FALSE;
        ComPtr<IDXGIFactory5> factory5;
        if (SUCCEEDED(dxgiFactory.As(&factory5)))
        {
            factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                                          &allowTearing, sizeof(allowTearing));
        }

        return allowTearing == TRUE;
    }

    ComPtr<IDXGISwapChain4> GDeviceFactory::CreateSwapChain(const std::shared_ptr<GDevice>& device,
                                                            DXGI_SWAP_CHAIN_DESC1& desc, const HWND hwnd)
    {
        EnsureInitialized();

        ComPtr<IDXGISwapChain4> swapChain;

        desc.Flags = IsTearingSupport()
                         ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING
                         : DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

        ComPtr<IDXGISwapChain1> swapChain1;
        ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
            device->GetCommandQueue(GQueueType::Graphics)->GetD3D12CommandQueue().Get(),
            hwnd,
            &desc,
            nullptr,
            nullptr,
            &swapChain1));

        // Disable the Alt+Enter fullscreen toggle feature. Switching to fullscreen
        // will be handled manually.
        ThrowIfFailed(dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER));

        ThrowIfFailed(swapChain1.As(&swapChain));

        return swapChain;
    }

    ComPtr<IDXGIFactory4> GDeviceFactory::GetFactory()
    {
        EnsureInitialized();
        return dxgiFactory;
    }

    std::shared_ptr<GDevice>& GDeviceFactory::GetDevice(const UINT adapterIndex)
    {
        EnsureInitialized();
        return hardwareDevices[adapterIndex];
    }


    std::vector<std::shared_ptr<GDevice>>& GDeviceFactory::GetAllDevices(const bool useWrap)
    {
        EnsureInitialized();

        if (!useWrap)
        {
            return hardwareDevices;
        }

        if (wrapDevice == nullptr)
        {
            ComPtr<IDXGIAdapter1> adapter;
            {
                dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&adapter));
                ComPtr<IDXGIAdapter3> adapter3;
                ThrowIfFailed(adapter->QueryInterface(IID_PPV_ARGS(&adapter3)));
                wrapDevice = (std::make_shared<GDevice>(adapter3));
                wrapDevice->Initialize();
            }
        }

        devicesWithWarp = hardwareDevices;
        devicesWithWarp.push_back(wrapDevice);
        return devicesWithWarp;
    }

    bool GDeviceFactory::IsTearingSupport()
    {
        EnsureInitialized();
        return isTearingSupport.value();
    }
}
