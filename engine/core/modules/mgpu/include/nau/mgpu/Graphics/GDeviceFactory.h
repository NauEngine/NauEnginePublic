#pragma once
#include <wrl/client.h>
#include  "dxgi1_6.h"
#include "nau/mgpu/Allocator/MemoryAllocator.h"
#include "nau/mgpu/Utils/Lazy.h"

using namespace Microsoft::WRL;

namespace nau::PEPEngine::Graphics
{
    using namespace Allocator;
    using namespace Utils;

    enum GraphicsAdapter : UINT
    {
        GraphicAdapterPrimary = 0,
        GraphicAdapterSecond = GraphicAdapterPrimary + 1,
        GraphicAdapterThird = GraphicAdapterSecond + 1,
        GraphicAdapterCount = GraphicAdapterThird + 1
    };

    class GDevice;

    class GDeviceFactory
    {
        static ComPtr<IDXGIFactory4> CreateFactory();
        static ComPtr<IDXGIFactory4> dxgiFactory;

        // B: first use initialization, see GDeviceFactory.cpp.
        static void EnsureInitialized();
        static bool debugLayerEnabled;

        static bool CheckTearingSupport();
        static Lazy<bool> isTearingSupport;

        static std::vector<std::shared_ptr<GDevice>> CreateDevices();
        static std::vector<std::shared_ptr<GDevice>> hardwareDevices;
        static std::shared_ptr<GDevice> wrapDevice;
        static std::vector<std::shared_ptr<GDevice>> devicesWithWarp;

        static std::vector<ComPtr<IDXGIAdapter3>> GetAdapters();
        static std::vector<ComPtr<IDXGIAdapter3>> adapters;

    public:
        // B: must be called before the first use of the factory; the debug layer (Debug builds) is on by default.
        // When Mgpu runs inside a process that already owns a D3D12 device (Nau) it has to stay off.
        static void SetDebugLayerEnabled(bool enabled);

        static ComPtr<IDXGISwapChain4> CreateSwapChain(const std::shared_ptr<GDevice>& device,
                                                       DXGI_SWAP_CHAIN_DESC1& desc,
                                                       HWND hwnd);

        static ComPtr<IDXGIFactory4> GetFactory();

        // Device selection is index-based so callers are not limited by the
        // legacy two-adapter enum. The enum names remain for older samples.
        static std::shared_ptr<GDevice>& GetDevice(UINT adapterIndex = GraphicAdapterPrimary);

        static std::vector<std::shared_ptr<GDevice>>& GetAllDevices(bool useWrap = false);

        static bool IsTearingSupport();
    };
}
