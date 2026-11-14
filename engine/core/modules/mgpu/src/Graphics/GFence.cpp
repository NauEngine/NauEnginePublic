#include "nau/mgpu/Graphics/GFence.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GCommandQueue.h"

namespace nau::PEPEngine::Graphics
{
    GFence::GFence(const std::shared_ptr<GDevice>& device, const D3D12_FENCE_FLAGS flags) : FenceValue(0),
                                                                                            device(device)
    {
        device->GetDXDevice()->CreateFence(0, flags, IID_PPV_ARGS(&fence));
    }

    ComPtr<ID3D12Fence> GFence::GetDXFence() const
    {
        return fence;
    }

    void GFence::Signal(const std::shared_ptr<GCommandQueue>& queue)
    {
        const auto fenceValue = ++FenceValue;
        queue->GetD3D12CommandQueue()->Signal(fence.Get(), fenceValue);
    }

    UINT64 GFence::GetFenceValue() const
    {
        return FenceValue;
    }

    bool GFence::IsFenceComplete(const uint64_t fenceValue) const
    {
        return fence->GetCompletedValue() >= fenceValue;
    }

    void GFence::WaitForFenceValue(const uint64_t fenceValue) const
    {
        if (IsFenceComplete(fenceValue))
            return;

        auto event = ::CreateEvent(nullptr, FALSE, FALSE, nullptr);
        assert(event && "Failed to create fence event handle.");

        fence->SetEventOnCompletion(fenceValue, event);
        WaitForSingleObject(event, DWORD_MAX);
        CloseHandle(event);
    }
}
