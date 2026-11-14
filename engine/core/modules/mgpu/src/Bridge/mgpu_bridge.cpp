// Mgpu <-> Nau transfer bridge, see include/nau/mgpu/bridge/mgpu_bridge.h.
// No Nau headers here: the Mgpu headers below leak global `using namespace` directives and macros.

#include "nau/mgpu/bridge/mgpu_bridge.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstring>
#include <exception>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

#include <d3dcompiler.h>

#include "nau/mgpu/Utils/d3dUtil.h"
#include "nau/mgpu/Utils/d3dx12.h"
#include "nau/mgpu/Graphics/ComputePSO.h"
#include "nau/mgpu/Graphics/GCommandList.h"
#include "nau/mgpu/Graphics/GCommandQueue.h"
#include "nau/mgpu/Graphics/GDescriptor.h"
#include "nau/mgpu/Graphics/GDevice.h"
#include "nau/mgpu/Graphics/GDeviceFactory.h"
#include "nau/mgpu/Graphics/GResource.h"
#include "nau/mgpu/Graphics/GRootSignature.h"
#include "nau/mgpu/Graphics/GShader.h"
#include "nau/mgpu/Graphics/GTexture.h"
#include "nau/mgpu/Common/GCrossAdapterResource.h"

#include "bridge_common.h"

namespace nau::mgpu::bridge
{
    namespace gfx = nau::PEPEngine::Graphics;
    using detail::SourceShader;

    namespace
    {
        std::atomic<bool> g_enabled{false};

        std::mutex g_statusMutex;
        Status g_status; // guarded by g_statusMutex

        constexpr uint32_t kWidth = 640;
        constexpr uint32_t kHeight = 360;
        constexpr uint32_t kBytesPerPixel = 4;
        constexpr DXGI_FORMAT kFormat = DXGI_FORMAT_R8G8B8A8_UNORM;

        // Images in flight: GPU2 draws one, GPU1 copies another, the CPU reads a third.
        constexpr uint32_t kSlotCount = 3;

        // Workload of GPU2: sine waves summed per pixel. The cost grows linearly with it.
        constexpr uint32_t kPlasmaWaves = 64;
        constexpr uint32_t kGroupSize = 8;

        // The image is produced on GPU2 by this compute shader: an animated plasma made of kPlasmaWaves independent
        // sine waves (one animation cycle per 120 frames). The CPU does not take part in producing it.
        constexpr const char* kPlasmaShaderSource = R"(
cbuffer PlasmaParams : register(b0)
{
    uint phase;
    uint width;
    uint height;
    uint waves;
};

RWTexture2D<unorm float4> output : register(u0);

[numthreads(8, 8, 1)]
void GeneratePlasma(uint3 id : SV_DispatchThreadID)
{
    if (id.x >= width || id.y >= height)
    {
        return;
    }

    const float twoPi = 6.28318530718;
    const float t = (float)(phase % 120) * (twoPi / 120.0);
    const float2 uv = float2((float)id.x / (float)width, (float)id.y / (float)height);

    float3 sum = 0.0;
    for (uint i = 0; i < waves; ++i)
    {
        const float frequency = 1.0 + (float)(i % 7) * 0.35;
        const float angle = (float)i * 2.39996323;
        const float w = twoPi * frequency * dot(uv, float2(cos(angle), sin(angle)));
        sum += float3(sin(w + t), sin(1.3 * w + 2.0 * t), sin(0.7 * w - t));
    }

    const float3 color = 0.5 + 0.5 * sin(sum * (3.0 / sqrt((float)max(waves, 1u))));
    output[id.xy] = float4(color, 1.0);
}
)";

        uint64_t Fnv1a(const uint8_t* data, const size_t size)
        {
            uint64_t hash = 1469598103934665603ull;
            for (size_t i = 0; i < size; ++i)
            {
                hash ^= data[i];
                hash *= 1099511628211ull;
            }
            return hash;
        }

        // A real plasma image is opaque and not flat; zeros or a constant colour mean GPU2 did not draw it.
        bool LooksLikePlasma(const std::vector<uint8_t>& image)
        {
            uint8_t lo[3] = {255, 255, 255};
            uint8_t hi[3] = {0, 0, 0};
            for (size_t i = 0; i < image.size(); i += kBytesPerPixel * 97)
            {
                if (image[i + 3] != 255)
                {
                    return false;
                }
                for (size_t c = 0; c < 3; ++c)
                {
                    lo[c] = std::min(lo[c], image[i + c]);
                    hi[c] = std::max(hi[c], image[i + c]);
                }
            }
            return hi[0] - lo[0] > 32 || hi[1] - lo[1] > 32 || hi[2] - lo[2] > 32;
        }
    }

    void setEnabled(const bool enabled)
    {
        g_enabled.store(enabled);
    }

    bool isEnabled()
    {
        return g_enabled.load();
    }

    void publishStatus(const Status& status)
    {
        std::lock_guard lock(g_statusMutex);
        g_status = status;
    }

    Status getStatus()
    {
        Status status;
        {
            std::lock_guard lock(g_statusMutex);
            status = g_status;
        }
        status.linked = true;
        status.enabled = g_enabled.load();
        return status;
    }

    struct TransferBridge::Impl
    {
        using Clock = std::chrono::steady_clock;

        // One image in flight: GPU2 -> shared heap -> GPU1 -> read-back.
        struct Slot
        {
            std::shared_ptr<GCrossAdapterResource> shared; // shared heap, placed resource on each GPU
            std::shared_ptr<gfx::GResource> receivedReadback; // GPU1 copy of the shared texture
            std::shared_ptr<gfx::GResource> producedReadback; // GPU2 copy of what it drew (bit-exact check)
            uint64_t secondaryQueueValue = 0; // GPU2 compute queue fence value of the slot's work
            uint64_t primaryQueueValue = 0; // GPU1 copy queue fence value of the slot's copy
            uint64_t sequence = 0; // submission order
            Clock::time_point started;
            bool inFlight = false;
        };

        bool ready = false;
        std::string error;
        std::string primaryName;
        std::string secondaryName;

        std::shared_ptr<gfx::GDevice> primaryDevice;
        std::shared_ptr<gfx::GDevice> secondaryDevice;

        // GPU2 side: compute shader -> source texture (UAV) -> shared heap. All GPU2 work runs on its compute queue:
        // GCommandList picks the compute root-signature calls by the command list type. One source texture is
        // enough: the work of all slots is ordered on that queue.
        std::shared_ptr<gfx::GTexture> sourceTexture;
        std::shared_ptr<gfx::GRootSignature> plasmaSignature;
        std::shared_ptr<SourceShader> plasmaShader;
        std::shared_ptr<gfx::ComputePSO> plasmaPSO;
        gfx::GDescriptor plasmaUav;

        std::array<Slot, kSlotCount> slots;
        uint64_t nextSequence = 0;
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT receivedFootprint = {}; // row-major shared texture
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT producedFootprint = {};

        // GPU2 -> GPU1 ordering on the GPU: Signal on the second device, Wait on the primary one.
        ComPtr<ID3D12Fence> primaryFence;
        ComPtr<ID3D12Fence> secondaryFence;
        UINT64 fenceValue = 0;

        std::vector<uint8_t> pixels; // last verified image
        std::vector<uint8_t> receivedPixels;
        std::vector<uint8_t> producedPixels;
        TransferInfo info;
        TransferStats stats;

        bool Initialize(const char* nauAdapterName)
        {
            try
            {
                if (!detail::SelectDevices(nauAdapterName, primaryDevice, secondaryDevice, error))
                {
                    return false;
                }
                primaryName = detail::ToUtf8(primaryDevice->GetName());
                secondaryName = detail::ToUtf8(secondaryDevice->GetName());

                // Source texture on GPU2, written by the plasma compute shader through a UAV.
                const D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
                    kFormat, kWidth, kHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
                sourceTexture = std::make_shared<gfx::GTexture>(secondaryDevice, textureDesc, L"MgpuBridge Source");

                plasmaUav = secondaryDevice->AllocateDescriptors(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1);
                D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
                uavDesc.Format = kFormat;
                uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
                sourceTexture->CreateUnorderedAccessView(&uavDesc, &plasmaUav);

                // Root signature: b0 = 4 root constants (PlasmaParams), table with u0.
                CD3DX12_DESCRIPTOR_RANGE uavRange;
                uavRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0, 0);
                plasmaSignature = std::make_shared<gfx::GRootSignature>();
                plasmaSignature->AddConstantParameter(4, 0);
                plasmaSignature->AddDescriptorParameter(&uavRange, 1);
                plasmaSignature->Initialize(secondaryDevice, false, D3D12_ROOT_SIGNATURE_FLAG_NONE);

                plasmaShader = std::make_shared<SourceShader>(kPlasmaShaderSource, gfx::ComputeShader, "GeneratePlasma",
                                                              "cs_5_1");
                plasmaShader->LoadAndCompile();

                plasmaPSO = std::make_shared<gfx::ComputePSO>(*plasmaSignature);
                plasmaPSO->SetShader(plasmaShader.get());
                plasmaPSO->Initialize(secondaryDevice);

                const D3D12_RESOURCE_DESC plainDesc = CD3DX12_RESOURCE_DESC::Tex2D(kFormat, kWidth, kHeight, 1, 1);
                UINT64 producedSize = 0;
                secondaryDevice->GetDXDevice()->GetCopyableFootprints(&plainDesc, 0, 1, 0, &producedFootprint,
                                                                      nullptr, nullptr, &producedSize);

                for (uint32_t i = 0; i < kSlotCount; ++i)
                {
                    Slot& slot = slots[i];
                    const std::wstring suffix = L" " + std::to_wstring(i);

                    // Shared heap: placed resource on GPU1 (prime) and on GPU2 (shared). GCrossAdapterResource
                    // replaces the flags with ALLOW_CROSS_ADAPTER and the layout with ROW_MAJOR.
                    D3D12_RESOURCE_DESC sharedDesc = plainDesc;
                    slot.shared = std::make_shared<GCrossAdapterResource>(sharedDesc, primaryDevice, secondaryDevice,
                                                                          L"MgpuBridge Shared" + suffix);

                    // Read-back buffer on GPU1 sized for the row-major (256-byte aligned pitch) shared texture.
                    UINT64 receivedSize = 0;
                    primaryDevice->GetDXDevice()->GetCopyableFootprints(&sharedDesc, 0, 1, 0, &receivedFootprint,
                                                                        nullptr, nullptr, &receivedSize);
                    slot.receivedReadback = std::make_shared<gfx::GResource>(
                        primaryDevice, CD3DX12_RESOURCE_DESC::Buffer(receivedSize), L"MgpuBridge Readback" + suffix,
                        nullptr, D3D12_RESOURCE_STATE_COPY_DEST, CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK));

                    slot.producedReadback = std::make_shared<gfx::GResource>(
                        secondaryDevice, CD3DX12_RESOURCE_DESC::Buffer(producedSize),
                        L"MgpuBridge Produced Readback" + suffix, nullptr, D3D12_RESOURCE_STATE_COPY_DEST,
                        CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK));
                }

                // GPU-side ordering between the two devices. If a shared fence cannot be created, Start() falls
                // back to waiting for GPU2 on the CPU.
                primaryDevice->TrySharedFence(primaryFence, secondaryDevice, secondaryFence, 0);

                const size_t imageSize = static_cast<size_t>(kWidth) * kHeight * kBytesPerPixel;
                pixels.assign(imageSize, 0);
                receivedPixels.assign(imageSize, 0);
                producedPixels.assign(imageSize, 0);
                info = {};
                info.width = kWidth;
                info.height = kHeight;
                info.rowPitch = kWidth * kBytesPerPixel;
                stats = {};

                ready = true;
                error.clear();
                return true;
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during initialize: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during initialize (see the D3D12 debug output)";
            }

            Release();
            return false;
        }

        // Records and submits the work of one image; does not wait unless there is no shared fence.
        void Start(Slot& slot, const uint32_t phase)
        {
            // GPU2 (compute queue): plasma -> source texture -> GPU2 read-back + shared heap.
            auto secondaryQueue = secondaryDevice->GetCommandQueue(gfx::GQueueType::Compute);
            {
                auto cmdList = secondaryQueue->GetCommandList();

                cmdList->SetPipelineState(*plasmaPSO);
                cmdList->SetDescriptorsHeap(&plasmaUav);
                const uint32_t params[4] = {phase, kWidth, kHeight, kPlasmaWaves};
                cmdList->SetRoot32BitConstants(0, 4, params, 0);
                cmdList->SetRootDescriptorTable(1, &plasmaUav);

                cmdList->TransitionBarrier(*sourceTexture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
                cmdList->FlushResourceBarriers();
                cmdList->Dispatch((kWidth + kGroupSize - 1) / kGroupSize, (kHeight + kGroupSize - 1) / kGroupSize, 1);
                cmdList->UAVBarrier(*sourceTexture, true);

                cmdList->TransitionBarrier(*sourceTexture, D3D12_RESOURCE_STATE_COPY_SOURCE);
                cmdList->FlushResourceBarriers();
                const CD3DX12_TEXTURE_COPY_LOCATION producedDst(slot.producedReadback->GetD3D12Resource().Get(),
                                                                producedFootprint);
                const CD3DX12_TEXTURE_COPY_LOCATION producedSrc(sourceTexture->GetD3D12Resource().Get(), 0);
                cmdList->GetGraphicsCommandList()->CopyTextureRegion(&producedDst, 0, 0, 0, &producedSrc, nullptr);

                cmdList->CopyResource(slot.shared->GetSharedResource(), *sourceTexture);
                cmdList->TransitionBarrier(slot.shared->GetSharedResource(), D3D12_RESOURCE_STATE_COMMON);
                cmdList->TransitionBarrier(*sourceTexture, D3D12_RESOURCE_STATE_COMMON);
                cmdList->FlushResourceBarriers();

                slot.secondaryQueueValue = secondaryQueue->ExecuteCommandList(cmdList);
            }

            // Order GPU1 after GPU2.
            auto primaryQueue = primaryDevice->GetCommandQueue(gfx::GQueueType::Copy);
            if (primaryFence && secondaryFence)
            {
                ++fenceValue;
                secondaryQueue->Signal(secondaryFence, fenceValue);
                primaryQueue->Wait(primaryFence, fenceValue);
            }
            else
            {
                secondaryQueue->WaitForFenceValue(slot.secondaryQueueValue);
            }

            // GPU1 (copy queue): shared heap -> read-back buffer.
            {
                auto cmdList = primaryQueue->GetCommandList();

                cmdList->TransitionBarrier(slot.shared->GetPrimeResource(), D3D12_RESOURCE_STATE_COPY_SOURCE);
                cmdList->FlushResourceBarriers();

                const CD3DX12_TEXTURE_COPY_LOCATION dst(slot.receivedReadback->GetD3D12Resource().Get(),
                                                        receivedFootprint);
                const CD3DX12_TEXTURE_COPY_LOCATION src(slot.shared->GetPrimeResource().GetD3D12Resource().Get(), 0);
                cmdList->GetGraphicsCommandList()->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

                cmdList->TransitionBarrier(slot.shared->GetPrimeResource(), D3D12_RESOURCE_STATE_COMMON);
                cmdList->FlushResourceBarriers();

                slot.primaryQueueValue = primaryQueue->ExecuteCommandList(cmdList);
            }

            slot.sequence = ++nextSequence;
            slot.started = Clock::now();
            slot.inFlight = true;
        }

        bool IsArrived(const Slot& slot) const
        {
            return slot.inFlight &&
                   primaryDevice->GetCommandQueue(gfx::GQueueType::Copy)->IsFinish(slot.primaryQueueValue);
        }

        static void ReadBack(gfx::GResource& buffer, const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& footprint,
                             std::vector<uint8_t>& destination)
        {
            const D3D12_RANGE readRange = {0, static_cast<SIZE_T>(footprint.Footprint.RowPitch) * kHeight +
                                                  static_cast<SIZE_T>(footprint.Offset)};
            const uint8_t* mapped = nullptr;
            ThrowIfFailed(buffer.GetD3D12Resource()->Map(0, &readRange,
                                                          reinterpret_cast<void**>(const_cast<uint8_t**>(&mapped))));
            for (uint32_t y = 0; y < kHeight; ++y)
            {
                std::memcpy(destination.data() + static_cast<size_t>(y) * kWidth * kBytesPerPixel,
                            mapped + footprint.Offset + static_cast<size_t>(y) * footprint.Footprint.RowPitch,
                            static_cast<size_t>(kWidth) * kBytesPerPixel);
            }
            const D3D12_RANGE noWrite = {0, 0};
            buffer.GetD3D12Resource()->Unmap(0, &noWrite);
        }

        // The slot's copy on GPU1 has finished: read both sides back and check them. Frees the slot.
        bool Collect(Slot& slot)
        {
            // GPU1 waited for GPU2's signal, so GPU2 is normally done already; this matters on the CPU-ordered path.
            secondaryDevice->GetCommandQueue(gfx::GQueueType::Compute)->WaitForFenceValue(slot.secondaryQueueValue);
            slot.inFlight = false;

            ReadBack(*slot.receivedReadback, receivedFootprint, receivedPixels);
            ReadBack(*slot.producedReadback, producedFootprint, producedPixels);

            info.checksum = Fnv1a(receivedPixels.data(), receivedPixels.size());
            info.expectedChecksum = Fnv1a(producedPixels.data(), producedPixels.size());
            const bool transported = receivedPixels == producedPixels;
            const bool drawn = LooksLikePlasma(producedPixels);
            info.checksumMatches = transported && drawn;

            stats.lastLatencyMs =
                std::chrono::duration<float, std::milli>(Clock::now() - slot.started).count();

            if (!info.checksumMatches)
            {
                ++stats.imagesFailed;
                error = !transported ? "pixels received on GPU1 differ from the pixels GPU2 produced"
                                     : "GPU2 image is empty or flat";
                return false;
            }

            ++stats.imagesOk;
            error.clear();
            pixels.swap(receivedPixels);
            return true;
        }

        Slot* FreeSlot()
        {
            for (Slot& slot : slots)
            {
                if (!slot.inFlight)
                {
                    return &slot;
                }
            }
            return nullptr;
        }

        bool Pump(const uint32_t frameIndex)
        {
            if (!ready)
            {
                return false;
            }

            try
            {
                // 1. Collect the arrived images in submission order; the newest verified one stays in pixels.
                bool newImage = false;
                for (;;)
                {
                    Slot* oldest = nullptr;
                    for (Slot& slot : slots)
                    {
                        if (IsArrived(slot) && (oldest == nullptr || slot.sequence < oldest->sequence))
                        {
                            oldest = &slot;
                        }
                    }
                    if (oldest == nullptr)
                    {
                        break;
                    }
                    newImage = Collect(*oldest) || newImage;
                }

                // 2. Keep GPU2 busy: start the next image if a slot is free.
                if (Slot* slot = FreeSlot())
                {
                    Start(*slot, frameIndex);
                }

                return newImage;
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during pump: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during pump (see the D3D12 debug output)";
            }

            ++stats.imagesFailed;
            return false;
        }

        bool Update(const uint32_t frameIndex)
        {
            if (!ready)
            {
                return false;
            }

            try
            {
                // Drain whatever pump() left in flight, then run one image start to end.
                auto primaryQueue = primaryDevice->GetCommandQueue(gfx::GQueueType::Copy);
                for (Slot& slot : slots)
                {
                    if (slot.inFlight)
                    {
                        primaryQueue->WaitForFenceValue(slot.primaryQueueValue);
                        slot.inFlight = false;
                    }
                }

                Slot& slot = slots[0];
                Start(slot, frameIndex);
                primaryQueue->WaitForFenceValue(slot.primaryQueueValue);
                return Collect(slot);
            }
            catch (const std::exception& e)
            {
                error = std::string("exception during update: ") + e.what();
            }
            catch (...)
            {
                error = "unknown exception during update (see the D3D12 debug output)";
            }

            ++stats.imagesFailed;
            info.checksumMatches = false;
            return false;
        }

        void Release()
        {
            ready = false;

            // Waits for all work in flight on both GPUs.
            if (primaryDevice)
            {
                primaryDevice->Flush();
            }
            if (secondaryDevice)
            {
                secondaryDevice->Flush();
            }

            primaryFence.Reset();
            secondaryFence.Reset();
            for (Slot& slot : slots)
            {
                slot = Slot();
            }
            plasmaPSO.reset();
            plasmaShader.reset();
            plasmaSignature.reset();
            plasmaUav = gfx::GDescriptor();
            sourceTexture.reset();
            primaryDevice.reset();
            secondaryDevice.reset();
        }
    };

    TransferBridge::TransferBridge() : m_impl(std::make_unique<Impl>())
    {
    }

    TransferBridge::~TransferBridge()
    {
        shutdown();
    }

    bool TransferBridge::initialize(const char* nauAdapterName)
    {
        if (m_impl->ready)
        {
            return true;
        }
        return m_impl->Initialize(nauAdapterName);
    }

    bool TransferBridge::pump(const uint32_t frameIndex)
    {
        return m_impl->Pump(frameIndex);
    }

    bool TransferBridge::update(const uint32_t frameIndex)
    {
        return m_impl->Update(frameIndex);
    }

    bool TransferBridge::isReady() const
    {
        return m_impl->ready;
    }

    void TransferBridge::shutdown()
    {
        if (m_impl)
        {
            m_impl->Release();
        }
    }

    const char* TransferBridge::lastError() const
    {
        return m_impl->error.c_str();
    }

    const char* TransferBridge::primaryAdapterName() const
    {
        return m_impl->primaryName.c_str();
    }

    const char* TransferBridge::secondaryAdapterName() const
    {
        return m_impl->secondaryName.c_str();
    }

    const void* TransferBridge::pixels() const
    {
        return m_impl->pixels.empty() ? nullptr : m_impl->pixels.data();
    }

    const TransferInfo& TransferBridge::info() const
    {
        return m_impl->info;
    }

    const TransferStats& TransferBridge::stats() const
    {
        return m_impl->stats;
    }
}
