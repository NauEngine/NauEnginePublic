#pragma once

#include <cstdint>
#include <memory>

namespace nau::mgpu::bridge
{
    struct DofParameters
    {
        float focusDistance = 25.0f;
        float focusRange = 6.0f;
        float farZ = 1000.0f;
        float maxCoC = 3.4f;
        float filterRadius = 3.0f;
        float taps = 32.0f;
    };

    enum class DofResult { Completed, Fallback, PrimaryFailure };

    // Plain facade: no Mgpu/D3D headers leak into Graphics.
    // Called on the render thread after Nau finishes its recorded work.
    // Inputs: HDR RGBA16F + normalized linear R32F depth in COPY_SOURCE.
    // Outputs: RGBA16F ColorFilled + R16F WFiltered in COPY_DEST.
    // Their states are not changed by this bridge. No CPU pixel readback.
    // Fallback is latched; a new DofBridge is required to retry initialization.
    class DofBridge
    {
    public:
        DofBridge();
        ~DofBridge();
        DofBridge(const DofBridge&) = delete;
        DofBridge& operator=(const DofBridge&) = delete;

        bool prepare(void* primaryDevice, void* primaryQueue, uint32_t width, uint32_t height);
        DofResult execute(void* color, void* linearDepth, void* colorFilled, void* filtered,
                          const DofParameters& parameters);
        const char* lastError() const;
        const char* secondaryAdapterName() const;

    private:
        struct Impl;
        std::unique_ptr<Impl> m_impl;
    };
}
