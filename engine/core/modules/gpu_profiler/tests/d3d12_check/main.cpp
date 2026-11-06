// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Check of D3D12TimestampSource on the real adapters of the machine: every hardware adapter (up to two) gets a direct, a
// compute and a copy queue; each frame copies a buffer on every queue inside a profiler scope. The CSV must then have a
// row with a positive time for every queue that supports timestamps. Copy queues without
// CopyQueueTimestampQueriesSupported are reported and skipped, not failed.
//
// Usage: GpuProfilerD3D12Check [csv path]

#ifndef WIN32_LEAN_AND_MEAN
    #define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
    #define NOMINMAX
#endif
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "nau/gpu_profiler/d3d12_timestamp_source.h"
#include "nau/gpu_profiler/gpu_profiler.h"

namespace
{
    using Microsoft::WRL::ComPtr;
    namespace profiler = nau::gpu_profiler;

    constexpr UINT64 CopySize = 64ull << 20;
    constexpr int Frames = 30;
    constexpr int BurstScopes = 200000;

    // The profiler reports the dropped CSV records in its last log line about the CSV.
    uint64_t g_csvDropped = 0;

    void logSink(const profiler::LogLevel level, const char* message)
    {
        std::fprintf(stderr, "[gpu_profiler]%s %s\n", level == profiler::LogLevel::Info ? "" : " !", message);
        unsigned long long dropped = 0;
        if (std::sscanf(message, "CSV: %llu records dropped", &dropped) == 1)
        {
            g_csvDropped = dropped;
        }
    }

    struct QueueTest
    {
        std::string name;
        D3D12_COMMAND_LIST_TYPE type = D3D12_COMMAND_LIST_TYPE_DIRECT;
        ComPtr<ID3D12CommandQueue> queue;
        ComPtr<ID3D12CommandAllocator> allocator;
        ComPtr<ID3D12GraphicsCommandList> list;
        ComPtr<ID3D12Fence> fence;
        UINT64 fenceValue = 0;
        profiler::D3D12TimestampSource source;
        bool measured = false;
    };

    struct AdapterTest
    {
        std::string name;
        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12Resource> src;
        ComPtr<ID3D12Resource> dst;
        std::vector<std::unique_ptr<QueueTest>> queues;
    };

    std::string toUtf8(const wchar_t* text)
    {
        const int size = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
        std::string out(size > 0 ? size - 1 : 0, '\0');
        WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), size, nullptr, nullptr);
        return out;
    }

    ComPtr<ID3D12Resource> createBuffer(ID3D12Device* device)
    {
        D3D12_HEAP_PROPERTIES heap = {};
        heap.Type = D3D12_HEAP_TYPE_DEFAULT;
        D3D12_RESOURCE_DESC desc = {};
        desc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        desc.Width = CopySize;
        desc.Height = 1;
        desc.DepthOrArraySize = 1;
        desc.MipLevels = 1;
        desc.SampleDesc.Count = 1;
        desc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        ComPtr<ID3D12Resource> buffer;
        if (FAILED(device->CreateCommittedResource(&heap, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_COMMON, nullptr,
                                                   IID_PPV_ARGS(&buffer))))
        {
            return nullptr;
        }
        return buffer;
    }

    bool createQueue(AdapterTest& adapter, const profiler::GpuIndex gpu, const D3D12_COMMAND_LIST_TYPE type, const char* kind)
    {
        auto test = std::make_unique<QueueTest>();
        test->type = type;
        test->name = std::string(kind) + std::to_string(gpu) + ".check";

        D3D12_COMMAND_QUEUE_DESC desc = {};
        desc.Type = type;
        ID3D12Device* device = adapter.device.Get();
        if (FAILED(device->CreateCommandQueue(&desc, IID_PPV_ARGS(&test->queue))) ||
            FAILED(device->CreateCommandAllocator(type, IID_PPV_ARGS(&test->allocator))) ||
            FAILED(device->CreateCommandList(0, type, test->allocator.Get(), nullptr, IID_PPV_ARGS(&test->list))) ||
            FAILED(test->list->Close()) || FAILED(device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&test->fence))))
        {
            std::printf("  %-14s could not create the queue\n", test->name.c_str());
            return false;
        }

        if (!test->source.init(device, test->queue.Get(), gpu, test->name.c_str()))
        {
            std::printf("  %-14s no timestamps (see the log above), skipped\n", test->name.c_str());
            return true;
        }
        test->measured = true;
        adapter.queues.push_back(std::move(test));
        return true;
    }

    void waitIdle(QueueTest& test)
    {
        test.queue->Signal(test.fence.Get(), ++test.fenceValue);
        if (test.fence->GetCompletedValue() < test.fenceValue)
        {
            HANDLE event = CreateEventW(nullptr, FALSE, FALSE, nullptr);
            test.fence->SetEventOnCompletion(test.fenceValue, event);
            WaitForSingleObject(event, 5000);
            CloseHandle(event);
        }
    }

    void runFrame(std::vector<AdapterTest>& adapters)
    {
        profiler::GpuProfiler& instance = profiler::GpuProfiler::instance();
        instance.beginFrame();
        for (AdapterTest& adapter : adapters)
        {
            for (auto& test : adapter.queues)
            {
                test->allocator->Reset();
                test->list->Reset(test->allocator.Get(), nullptr);

                test->source.setCommandList(test->list.Get());
                {
                    NAU_GPU_TIMER_SRC(test->source.id(), "copy_64mb");
                    test->list->CopyBufferRegion(adapter.dst.Get(), 0, adapter.src.Get(), 0, CopySize);
                }
                test->source.resolve();
                test->source.setCommandList(nullptr);

                test->list->Close();
                ID3D12CommandList* lists[] = {test->list.Get()};
                test->queue->ExecuteCommandLists(1, lists);
                test->source.submitted();
                // The queues of one adapter share the buffers: one after another.
                waitIdle(*test);
            }
        }
        instance.endFrame();
    }
}  // namespace

int main(int argc, char** argv)
{
    const std::string csvPath = argc > 1 ? argv[1] : "gpu_profiler_d3d12_check.csv";

    profiler::Config config;
    config.enabled = true;
    std::snprintf(config.csvPath, sizeof(config.csvPath), "%s", csvPath.c_str());
    config.warmupFrames = 0;
    config.logEveryNFrames = 0;
    profiler::GpuProfiler& instance = profiler::GpuProfiler::instance();
    instance.setLogSink(&logSink);
    instance.init(config);
    if (!instance.isEnabled())
    {
        std::printf("the profiler could not start (CSV '%s')\n", csvPath.c_str());
        return 2;
    }

    ComPtr<IDXGIFactory1> factory;
    if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
    {
        std::printf("CreateDXGIFactory1 failed\n");
        return 2;
    }

    std::vector<AdapterTest> adapters;
    for (UINT i = 0; adapters.size() < profiler::MaxGpus; ++i)
    {
        ComPtr<IDXGIAdapter1> dxgiAdapter;
        if (factory->EnumAdapters1(i, &dxgiAdapter) == DXGI_ERROR_NOT_FOUND)
        {
            break;
        }
        DXGI_ADAPTER_DESC1 desc = {};
        dxgiAdapter->GetDesc1(&desc);
        if ((desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0)
        {
            continue;
        }

        AdapterTest adapter;
        adapter.name = toUtf8(desc.Description);
        if (FAILED(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&adapter.device))))
        {
            continue;
        }
        adapter.src = createBuffer(adapter.device.Get());
        adapter.dst = createBuffer(adapter.device.Get());
        if (!adapter.src || !adapter.dst)
        {
            std::printf("GPU %zu '%s': could not create the buffers\n", adapters.size(), adapter.name.c_str());
            continue;
        }

        const auto gpu = static_cast<profiler::GpuIndex>(adapters.size());
        std::printf("GPU %u: %s\n", gpu, adapter.name.c_str());
        createQueue(adapter, gpu, D3D12_COMMAND_LIST_TYPE_DIRECT, "gfx");
        createQueue(adapter, gpu, D3D12_COMMAND_LIST_TYPE_COMPUTE, "compute");
        createQueue(adapter, gpu, D3D12_COMMAND_LIST_TYPE_COPY, "copy");
        adapters.push_back(std::move(adapter));
    }

    if (adapters.empty())
    {
        std::printf("no hardware adapter\n");
        return 2;
    }

    for (int frame = 0; frame < Frames; ++frame)
    {
        runFrame(adapters);
    }

    // A source unregistered while the results of its last frame are not collected yet (a bridge switched off): that
    // frame is dropped for this source, the profiler must neither crash nor time out. Its count stays at Frames.
    runFrame(adapters);
    QueueTest& unregistered = *adapters.front().queues.front();
    unregistered.source.shutdown();

    // Everything has executed: one more frame collects the results of the last one.
    instance.beginFrame();
    instance.endFrame();

    // CSV writer: a burst far above the capacity of its ring in one frame. The render thread must not wait, the lost
    // records must be counted, and every written row must stay intact.
    instance.beginFrame();
    for (int i = 0; i < BurstScopes; ++i)
    {
        NAU_CPU_TIMER("writer_burst");
    }
    instance.endFrame();

    const uint64_t timedOut = instance.counters().framesTimedOut;
    const uint64_t unmeasured = instance.counters().scopesUnmeasured;
    instance.shutdown();
    const uint64_t dropped = g_csvDropped;
    for (AdapterTest& adapter : adapters)
    {
        for (auto& test : adapter.queues)
        {
            test->source.shutdown();
        }
    }

    // queue -> times of copy_64mb
    std::map<std::string, std::vector<double>> times;
    std::ifstream csv(csvPath);
    std::string line;
    std::getline(csv, line);
    const auto columns = std::count(line.begin(), line.end(), ',') + 1;
    uint64_t burstRows = 0;
    uint64_t brokenRows = 0;
    while (std::getline(csv, line))
    {
        if (std::count(line.begin(), line.end(), ',') + 1 != columns)
        {
            ++brokenRows;
        }
        if (line.find(",writer_burst,") != std::string::npos)
        {
            ++burstRows;
        }
        std::vector<std::string> fields;
        std::stringstream stream(line);
        std::string field;
        while (std::getline(stream, field, ','))
        {
            fields.push_back(field);
        }
        if (fields.size() >= 5 && fields[1] == "copy_64mb")
        {
            times[fields[3]].push_back(std::atof(fields[4].c_str()));
        }
    }

    bool ok = true;
    for (AdapterTest& adapter : adapters)
    {
        for (auto& test : adapter.queues)
        {
            const std::vector<double>& values = times[test->name];
            double sum = 0;
            bool positive = !values.empty();
            for (const double value : values)
            {
                sum += value;
                positive = positive && value > 0;
            }
            // The other queues also have the extra frame.
            const size_t expected = test.get() == &unregistered ? Frames : Frames + 1;
            const bool queueOk = values.size() == expected && positive;
            ok = ok && queueOk;
            std::printf("  %-14s %2zu/%zu frames, copy of 64 MB %.3f ms on average: %s%s\n", test->name.c_str(), values.size(),
                        expected, values.empty() ? 0.0 : sum / values.size(), queueOk ? "OK" : "FAILED",
                        test.get() == &unregistered ? " (unregistered with a frame in flight)" : "");
        }
    }
    // A few other records of the burst frame (cpu_frame) may be dropped as well.
    const bool writerOk = dropped > 0 && brokenRows == 0 && burstRows + dropped >= BurstScopes &&
                          burstRows + dropped <= BurstScopes + 4;
    std::printf("CSV writer burst: %d records pushed in one frame, %llu written, %llu dropped (counted), %llu broken rows: %s\n",
                BurstScopes, static_cast<unsigned long long>(burstRows), static_cast<unsigned long long>(dropped),
                static_cast<unsigned long long>(brokenRows), writerOk ? "OK" : "FAILED");
    ok = ok && writerOk;
    ok = ok && timedOut == 0;
    std::printf("timed out frames: %llu, scopes not measured: %llu (the Frame scope of each frame: no list open)\n",
                static_cast<unsigned long long>(timedOut), static_cast<unsigned long long>(unmeasured));
    std::printf("%s\n", ok ? "OK" : "FAILED");
    return ok ? 0 : 1;
}
