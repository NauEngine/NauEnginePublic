// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Timestamp source for a raw D3D12 queue that does not belong to Nau's driver (the queues of the Mgpu module, on
// GPU 0 and GPU 1). Only for .cpp files that already use D3D12: it includes <d3d12.h>, not the Mgpu headers, and must
// not appear in the public bridge headers.
//
// The source owns a query heap, a readback buffer and a fence on the device of the queue. It is bound to one queue:
// TIMESTAMP heap for direct and compute queues, COPY_QUEUE_TIMESTAMP for copy queues (when
// D3D12_FEATURE_D3D12_OPTIONS3::CopyQueueTimestampQueriesSupported; otherwise the queue is not measured and the reason
// is logged). The frequency is GetTimestampFrequency of that queue.
//
// The owner of the command list drives the source:
//
//     source.setCommandList(list);         // timers of this source write into `list` from now on
//     { NAU_GPU_TIMER_SRC(source.id(), "pass"); ... }
//     source.resolve();                    // before list->Close(): ResolveQueryData of what was written into `list`
//     source.setCommandList(nullptr);      // timers of this source are skipped outside of recording
//     ... ExecuteCommandLists ...
//     source.submitted();                  // after ExecuteCommandLists: signals the fence the values wait for
//
// The results become readable when the fence has been passed; nothing waits for it.
#pragma once

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <wrl/client.h>

#include <cstdint>
#include <vector>

#include "nau/gpu_profiler/gpu_profiler.h"

namespace nau::gpu_profiler
{
    class D3D12TimestampSource final : public ITimestampSource
    {
    public:
        static constexpr uint32_t DefaultCapacity = 512;

        D3D12TimestampSource() = default;
        ~D3D12TimestampSource() override;

        D3D12TimestampSource(const D3D12TimestampSource&) = delete;
        D3D12TimestampSource& operator=(const D3D12TimestampSource&) = delete;

        // Creates the D3D12 objects and registers the source with the profiler. Does nothing (returns false) when the
        // profiler is disabled or the queue cannot provide timestamps.
        bool init(ID3D12Device* device, ID3D12CommandQueue* queue, GpuIndex gpu, const char* queueName,
                  uint32_t capacity = DefaultCapacity);
        // Unregisters the source. Must be called before the device or the queue is destroyed.
        void shutdown();

        bool isValid() const
        {
            return m_id != InvalidSource;
        }
        // NAU_GPU_TIMER_SRC(source.id(), ...); InvalidSource when not measured (the timers do nothing then).
        SourceId id() const
        {
            return m_id;
        }

        void setCommandList(ID3D12GraphicsCommandList* list);
        void resolve();
        void submitted();

        // ITimestampSource
        uint64_t getFrequency() override;
        bool canIssue() override;
        void issue(void*& query) override;
        bool tryGetValue(void* query, uint64_t& value) override;
        void release(void* query) override;
        bool getClockCalibration(uint64_t& gpuTicks, uint64_t& cpuTicks, uint64_t& cpuFrequency) override;

    private:
        enum class SlotState : uint8_t
        {
            Free,
            Issued, // EndQuery recorded, not resolved yet
            Resolved, // ResolveQueryData recorded, the list is not submitted yet
            Submitted, // waits for fenceValue
            Released // given back while the GPU may still write it: free once fenceValue has passed
        };

        struct Slot
        {
            SlotState state = SlotState::Free;
            uint64_t fenceValue = 0;
        };

        bool toSlot(void* query, uint32_t& slot) const;
        void reclaimReleased();

        Microsoft::WRL::ComPtr<ID3D12CommandQueue> m_queue;
        Microsoft::WRL::ComPtr<ID3D12QueryHeap> m_heap;
        Microsoft::WRL::ComPtr<ID3D12Resource> m_readback;
        Microsoft::WRL::ComPtr<ID3D12Fence> m_fence;
        const uint64_t* m_mapped = nullptr;
        uint64_t m_fenceValue = 0;
        uint64_t m_frequency = 0;
        SourceId m_id = InvalidSource;

        ID3D12GraphicsCommandList* m_list = nullptr;
        std::vector<Slot> m_slots;
        std::vector<uint32_t> m_freeSlots;
        std::vector<uint32_t> m_issued;
        std::vector<uint32_t> m_resolved;
        std::vector<uint32_t> m_released;
    };
}  // namespace nau::gpu_profiler
