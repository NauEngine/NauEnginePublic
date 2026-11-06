// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// GPU timestamp profiler for comparing a scene rendered on one and on two GPUs.
//
// Only plain C++ types are used here on purpose: this header is included both by Nau code and by the Mgpu module,
// whose headers must not meet Nau's (see nau/mgpu/bridge/mgpu_bridge.h). The implementation (EASTL) is hidden.
//
// Timestamp sources. Every queue that executes measured work has its own source (ITimestampSource), registered with
// the GPU it runs on and a queue name: Nau's driver on GPU 0 (always source PrimarySource), and raw D3D12 queues of
// the Mgpu module. Timestamps of different sources are never subtracted from each other: they come from different
// clocks, possibly with different frequencies. Only the clock calibration to the CPU counter (QueryPerformanceCounter)
// relates them.
//
// Frames. beginFrame()/endFrame() are called on the render thread around the frame; beginFrame() opens the "Frame"
// scope on GPU 0. The results of a frame are read without waiting, once the GPU has written them: frames wait in a
// queue of pending frames per GPU (Config::maxPendingFrames). When that queue is full the frame is not measured on that
// GPU (counted as skipped); a frame whose results do not arrive in time (Config::timeoutFrames / timeoutMs) is dropped
// and logged, so a lost device never blocks the profiler.
//
// CPU time. endFrame() writes "cpu_frame": the time between the ends of two neighbouring frames (the present follows
// endFrame()). NAU_CPU_TIMER times work on the render thread. The 1 GPU / 2 GPU comparison is based on the CPU frame
// time; GPU times explain where the time goes.
//
// Configuration (environment variables, read by init()):
//   NAU_GPU_PROFILER=1                  enable (disabled by default; then every macro costs one bool check)
//   NAU_GPU_PROFILER_CSV=<path>         CSV output (default gpu_timings.csv in the working directory)
//   NAU_GPU_PROFILER_LOG_EVERY=<N>      averaged timings in the log every N frames (default 300, 0 = never)
//   NAU_GPU_PROFILER_WARMUP=<N>         the first N frames are not measured (default 120)
//   NAU_GPU_PROFILER_MAX_PENDING=<N>    frames waiting for the results of one GPU (default 8)
//   NAU_GPU_PROFILER_TIMEOUT_FRAMES=<N> a pending frame is dropped after N frames (default 64)
//   NAU_GPU_PROFILER_TIMEOUT_MS=<N>     ... or after N milliseconds (default 2000)
//
// All calls must come from one thread, the render thread.
#pragma once

#include <cstdint>

namespace nau::gpu_profiler
{
    using GpuIndex = uint32_t;
    inline constexpr GpuIndex PrimaryGpu = 0;
    inline constexpr GpuIndex SecondaryGpu = 1;
    inline constexpr GpuIndex MaxGpus = 2;

    // Registered timestamp source (one GPU queue).
    using SourceId = uint32_t;
    inline constexpr SourceId InvalidSource = ~0u;
    // Nau's driver on GPU 0, registered first by Graphics.
    inline constexpr SourceId PrimarySource = 0;

    class ITimestampSource
    {
    public:
        virtual ~ITimestampSource() = default;

        // Timestamp ticks per second, 0 when the queue cannot provide timestamps.
        virtual uint64_t getFrequency() = 0;
        // False while there is no command stream to write a timestamp into; scopes opened then are not measured.
        virtual bool canIssue()
        {
            return true;
        }
        // Writes a timestamp into the command stream. A null query is created by the source; a query handed back by
        // the profiler after its value was read is reused.
        virtual void issue(void*& query) = 0;
        // Non-blocking: true and the value once the GPU has written the timestamp.
        virtual bool tryGetValue(void* query, uint64_t& value) = 0;
        // The query is not used by the profiler any more; the GPU may still write it.
        virtual void release(void* query) = 0;
        // A GPU timestamp and the CPU counter sampled at the same moment (ID3D12CommandQueue::GetClockCalibration).
        // cpuFrequency: ticks per second of cpuTicks, 0 for QueryPerformanceCounter. False when unavailable.
        virtual bool getClockCalibration(uint64_t& gpuTicks, uint64_t& cpuTicks, uint64_t& cpuFrequency)
        {
            return false;
        }
    };

    enum class LogLevel : uint8_t
    {
        Info,
        Warning,
        Error
    };

    using LogSink = void (*)(LogLevel level, const char* message);

    struct Config
    {
        bool enabled = false;
        char csvPath[260] = "gpu_timings.csv";
        uint32_t logEveryNFrames = 300;
        uint32_t warmupFrames = 120;
        uint32_t maxPendingFrames = 8;
        uint32_t timeoutFrames = 64;
        uint32_t timeoutMs = 2000;
    };

    struct Counters
    {
        uint64_t framesMeasured = 0; // frames of one GPU whose results were written
        uint64_t framesSkipped = 0; // the pending queue of a GPU was full
        uint64_t framesTimedOut = 0; // results did not arrive in time
        uint64_t framesInFlightAtShutdown = 0; // dropped at shutdown, not an error
        uint64_t scopesUnmeasured = 0; // the source could not record, or the scope was not closed
        uint64_t recordsDropped = 0; // CSV records lost
    };

    class GpuProfiler
    {
    public:
        static GpuProfiler& instance();

        // Reads the environment (see the top of this file). Does nothing when the profiler is disabled.
        void init();
        void init(const Config& config);
        // Collects what has arrived, writes the summary and closes the CSV. Sources stay registered until then.
        void shutdown();

        // Where the profiler's messages go (stderr by default). May be set before init().
        void setLogSink(LogSink sink);

        bool isEnabled() const
        {
            return m_enabled;
        }
        const Config& config() const;
        const Counters& counters() const;

        // The source must stay alive until unregisterSource() or shutdown(). Returns InvalidSource when the profiler
        // is disabled or the source has no timestamps. queueName goes to the "queue" column ("gfx0", "copy0.csm"...).
        SourceId registerSource(GpuIndex gpu, const char* queueName, ITimestampSource* source);
        // Scopes of the source still in flight are dropped.
        void unregisterSource(SourceId source);
        // The source of gpu that can record right now; a raw source with an open command list goes before Nau's driver.
        SourceId findSource(GpuIndex gpu) const;

        void beginFrame();
        void endFrame();
        uint64_t frameIndex() const;

        void beginScope(const char* name, SourceId source);
        void endScope(SourceId source);

        // CPU time of work on the render thread (QueryPerformanceCounter), written with gpu = "cpu".
        void beginCpuScope(const char* name);
        void endCpuScope();

        struct Impl;

    private:
        GpuProfiler() = default;
        ~GpuProfiler();

        bool m_enabled = false;
        Impl* m_impl = nullptr;
    };

    class ScopedGpuTimer
    {
    public:
        ScopedGpuTimer(const char* name, const SourceId source)
        {
            GpuProfiler& profiler = GpuProfiler::instance();
            if (profiler.isEnabled())
            {
                m_source = source;
                profiler.beginScope(name, m_source);
            }
        }

        // The int tag selects the GPU form (NAU_GPU_TIMER_ON).
        ScopedGpuTimer(const char* name, const GpuIndex gpu, int)
        {
            GpuProfiler& profiler = GpuProfiler::instance();
            if (profiler.isEnabled())
            {
                m_source = profiler.findSource(gpu);
                profiler.beginScope(name, m_source);
            }
        }

        ~ScopedGpuTimer()
        {
            if (m_source != InvalidSource)
            {
                GpuProfiler::instance().endScope(m_source);
            }
        }

        ScopedGpuTimer(const ScopedGpuTimer&) = delete;
        ScopedGpuTimer& operator=(const ScopedGpuTimer&) = delete;

    private:
        SourceId m_source = InvalidSource;
    };

    class ScopedCpuTimer
    {
    public:
        explicit ScopedCpuTimer(const char* name)
        {
            GpuProfiler& profiler = GpuProfiler::instance();
            m_active = profiler.isEnabled();
            if (m_active)
            {
                profiler.beginCpuScope(name);
            }
        }

        ~ScopedCpuTimer()
        {
            if (m_active)
            {
                GpuProfiler::instance().endCpuScope();
            }
        }

        ScopedCpuTimer(const ScopedCpuTimer&) = delete;
        ScopedCpuTimer& operator=(const ScopedCpuTimer&) = delete;

    private:
        bool m_active = false;
    };
}  // namespace nau::gpu_profiler

#define NAU_GPU_PROFILER_CONCAT_(a, b) a##b
#define NAU_GPU_PROFILER_CONCAT(a, b) NAU_GPU_PROFILER_CONCAT_(a, b)

// Times the enclosing scope on GPU 0, through Nau's driver.
#define NAU_GPU_TIMER(name) \
    ::nau::gpu_profiler::ScopedGpuTimer NAU_GPU_PROFILER_CONCAT(nauGpuTimer_, __LINE__)(name, ::nau::gpu_profiler::PrimarySource)
// Times the enclosing scope on the source of `gpu` that records right now (a raw D3D12 source with an open command
// list, otherwise Nau's driver on GPU 0).
#define NAU_GPU_TIMER_ON(gpu, name) \
    ::nau::gpu_profiler::ScopedGpuTimer NAU_GPU_PROFILER_CONCAT(nauGpuTimer_, __LINE__)(name, gpu, 0)
// Times the enclosing scope on the given registered source.
#define NAU_GPU_TIMER_SRC(source, name) \
    ::nau::gpu_profiler::ScopedGpuTimer NAU_GPU_PROFILER_CONCAT(nauGpuTimer_, __LINE__)(name, source)
// CPU time of the enclosing scope on the render thread.
#define NAU_CPU_TIMER(name) ::nau::gpu_profiler::ScopedCpuTimer NAU_GPU_PROFILER_CONCAT(nauCpuTimer_, __LINE__)(name)
