// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "nau/gpu_profiler/gpu_profiler.h"

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/unordered_map.h>
#include <EASTL/vector.h>

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "profiler_clock.h"
#include "profiler_log.h"
#include "record_writer.h"

namespace nau::gpu_profiler
{
    namespace
    {
        constexpr uint32_t MaxSources = 16;

        const char* getEnv(const char* name)
        {
            const char* value = std::getenv(name);
            return value != nullptr && *value != '\0' ? value : nullptr;
        }

        void readUint(const char* name, uint32_t& out)
        {
            if (const char* value = getEnv(name))
            {
                const long parsed = std::strtol(value, nullptr, 10);
                out = parsed > 0 ? static_cast<uint32_t>(parsed) : 0;
            }
        }

        void defaultLogSink(const LogLevel level, const char* message)
        {
            static const char* const prefixes[] = {"[gpu_profiler] ", "[gpu_profiler] warning: ", "[gpu_profiler] error: "};
            std::fprintf(stderr, "%s%s\n", prefixes[static_cast<int>(level)], message);
        }

        LogSink g_logSink = &defaultLogSink;
    }  // namespace

    void logMessage(const LogLevel level, const char* format, ...)
    {
        char buffer[512];
        va_list args;
        va_start(args, format);
        std::vsnprintf(buffer, sizeof(buffer), format, args);
        va_end(args);
        g_logSink(level, buffer);
    }

    struct GpuProfiler::Impl
    {
        using Clock = std::chrono::steady_clock;

        struct Source
        {
            ITimestampSource* source = nullptr;
            GpuIndex gpu = PrimaryGpu;
            uint8_t queueId = 0;
            uint64_t frequency = 0;
            eastl::vector<void*> freeQueries;
            eastl::vector<uint32_t> openScopes; // indices into the current frame of its GPU; ~0u: not measured
        };

        struct Scope
        {
            SourceId source = InvalidSource;
            uint16_t name = 0;
            void* begin = nullptr;
            void* end = nullptr;
        };

        // The work of one frame on one GPU.
        struct FrameRecord
        {
            bool used = false;
            uint64_t frame = 0;
            uint8_t mode = 0;
            Clock::time_point closed;
            eastl::vector<Scope> scopes;
        };

        // Independent per GPU: a lost or stuck GPU 1 does not hold the results of GPU 0 back.
        struct GpuState
        {
            FrameRecord current;
            bool frameActive = false;
            eastl::vector<FrameRecord> pending; // fixed size (Config::maxPendingFrames), FrameRecord::used marks a frame
        };

        struct Accum
        {
            double sumMs = 0;
            double maxMs = 0;
            uint32_t count = 0;
        };

        struct CpuScope
        {
            uint16_t name = 0;
            uint64_t start = 0;
        };

        Config config;
        Counters counters;
        RecordWriter writer;

        eastl::array<Source, MaxSources> sources;
        eastl::array<bool, MaxSources> sourceUsed = {};
        eastl::array<GpuState, MaxGpus> gpus;

        uint64_t frame = 0;
        bool frameOpen = false;
        bool measuring = false; // the current frame is measured (not warmup, not skipped)
        uint64_t lastFrameEnd = 0;
        uint8_t cpuQueue = 0;
        eastl::vector<CpuScope> cpuScopes;

        eastl::unordered_map<eastl::string, uint16_t> namesByText;
        eastl::unordered_map<uint64_t, Accum> accum; // key: kind, gpu, queue, name
        eastl::vector<uint64_t> accumOrder;
        uint32_t framesSinceLog = 0;

        template <typename... Args>
        void log(const LogLevel level, const char* format, Args... args)
        {
            logMessage(level, format, args...);
        }

        uint16_t nameId(const char* name)
        {
            if (name == nullptr)
            {
                name = "<null>";
            }
            // By text, without allocating: names may come from temporary buffers.
            const auto found = namesByText.find_as(name, eastl::hash<const char*>(), eastl::equal_to_2<eastl::string, const char*>());
            if (found != namesByText.end())
            {
                return found->second;
            }
            const uint16_t id = writer.addString(name);
            namesByText.emplace(eastl::string(name), id);
            return id;
        }

        static double toMs(const uint64_t begin, const uint64_t end, const uint64_t frequency)
        {
            return end >= begin && frequency != 0 ? static_cast<double>(end - begin) * 1000.0 / static_cast<double>(frequency) : -1.0;
        }

        void* acquireQuery(Source& source)
        {
            if (!source.freeQueries.empty())
            {
                void* query = source.freeQueries.back();
                source.freeQueries.pop_back();
                return query;
            }
            return nullptr; // the source creates it in issue()
        }

        void recycleQuery(const SourceId id, void* query)
        {
            if (query != nullptr && id < MaxSources && sourceUsed[id])
            {
                sources[id].freeQueries.push_back(query);
            }
        }

        // A query that may still be written by the GPU is given back to its source instead of being reused.
        void dropQuery(const SourceId id, void* query)
        {
            if (query != nullptr && id < MaxSources && sourceUsed[id])
            {
                sources[id].source->release(query);
            }
        }

        void emit(const RecordKind kind, const uint64_t frameIndex, const uint16_t name, const uint8_t gpu, const uint8_t queue,
                  const uint8_t mode, const double ms, const uint16_t age = NoAge)
        {
            Record record;
            record.frame = static_cast<uint32_t>(frameIndex);
            record.name = name;
            record.gpu = gpu;
            record.queue = queue;
            record.kind = kind;
            record.mode = mode;
            record.age = age;
            record.ms = static_cast<float>(ms);
            if (!writer.push(record))
            {
                ++counters.recordsDropped;
            }

            const uint64_t key = (static_cast<uint64_t>(kind) << 48) | (static_cast<uint64_t>(gpu) << 40) |
                                 (static_cast<uint64_t>(queue) << 32) | name;
            auto [it, inserted] = accum.try_emplace(key);
            if (inserted)
            {
                accumOrder.push_back(key);
            }
            it->second.sumMs += ms;
            it->second.maxMs = eastl::max(it->second.maxMs, ms);
            ++it->second.count;
        }

        FrameRecord* freePendingSlot(GpuState& state)
        {
            for (FrameRecord& record : state.pending)
            {
                if (!record.used)
                {
                    return &record;
                }
            }
            return nullptr;
        }

        void releaseRecord(FrameRecord& record, const bool gpuMayWrite)
        {
            for (const Scope& scope : record.scopes)
            {
                if (scope.source == InvalidSource)
                {
                    continue;
                }
                if (gpuMayWrite)
                {
                    dropQuery(scope.source, scope.begin);
                    dropQuery(scope.source, scope.end);
                }
                else
                {
                    recycleQuery(scope.source, scope.begin);
                    recycleQuery(scope.source, scope.end);
                }
            }
            record.scopes.clear();
            record.used = false;
        }

        // Writes the frame once every timestamp of it is available; false while the GPU has not written them yet.
        bool tryFinish(const GpuIndex gpu, FrameRecord& record)
        {
            for (const Scope& scope : record.scopes)
            {
                if (scope.source == InvalidSource)
                {
                    continue;
                }
                uint64_t value = 0;
                Source& source = sources[scope.source];
                if (!source.source->tryGetValue(scope.begin, value) || !source.source->tryGetValue(scope.end, value))
                {
                    return false;
                }
            }

            for (const Scope& scope : record.scopes)
            {
                if (scope.source == InvalidSource)
                {
                    continue;
                }
                Source& source = sources[scope.source];
                uint64_t begin = 0;
                uint64_t end = 0;
                source.source->tryGetValue(scope.begin, begin);
                source.source->tryGetValue(scope.end, end);
                const double ms = toMs(begin, end, source.frequency);
                if (ms >= 0)
                {
                    emit(RecordKind::Pass, record.frame, scope.name, static_cast<uint8_t>(gpu), source.queueId, record.mode, ms);
                }
            }
            ++counters.framesMeasured;
            releaseRecord(record, false);
            return true;
        }

        void collect(const bool final)
        {
            const Clock::time_point now = Clock::now();
            for (GpuIndex gpu = 0; gpu < MaxGpus; ++gpu)
            {
                for (FrameRecord& record : gpus[gpu].pending)
                {
                    if (!record.used || tryFinish(gpu, record))
                    {
                        continue;
                    }

                    if (final)
                    {
                        // The last frames in flight at shutdown: expected, not a timeout.
                        ++counters.framesInFlightAtShutdown;
                        releaseRecord(record, true);
                        continue;
                    }

                    const uint64_t ageFrames = frame - record.frame;
                    const auto ageMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - record.closed).count();
                    if (ageFrames > config.timeoutFrames || ageMs > config.timeoutMs)
                    {
                        if (counters.framesTimedOut++ % 100 == 0)
                        {
                            log(LogLevel::Warning, "frame %llu on GPU %u: results did not arrive (%llu frames, %lld ms), dropped "
                                "(%llu dropped so far)", static_cast<unsigned long long>(record.frame), gpu,
                                static_cast<unsigned long long>(ageFrames), static_cast<long long>(ageMs),
                                static_cast<unsigned long long>(counters.framesTimedOut));
                        }
                        releaseRecord(record, true);
                    }
                }
            }
        }

        void logSummary()
        {
            if (accumOrder.empty())
            {
                return;
            }
            log(LogLevel::Info, "--- averages over %u frames (frame %llu) ---", framesSinceLog, static_cast<unsigned long long>(frame));
            for (const uint64_t key : accumOrder)
            {
                Accum& value = accum[key];
                if (value.count == 0)
                {
                    continue;
                }
                const auto kind = static_cast<RecordKind>(key >> 48);
                const auto gpu = static_cast<uint8_t>(key >> 40);
                const auto queue = static_cast<uint8_t>(key >> 32);
                const auto name = static_cast<uint16_t>(key);
                char gpuText[16];
                formatGpu(gpuText, sizeof(gpuText), kind, gpu);
                log(LogLevel::Info, "  %-40s gpu=%-4s queue=%-12s avg %8.3f ms  max %8.3f ms  (%u)", writer.string(name, kind).c_str(),
                    gpuText, writer.queue(queue).c_str(), value.sumMs / value.count, value.maxMs, value.count);
                value = {};
            }
            framesSinceLog = 0;
        }
    };

    GpuProfiler& GpuProfiler::instance()
    {
        static GpuProfiler profiler;
        return profiler;
    }

    GpuProfiler::~GpuProfiler()
    {
        shutdown();
    }

    void GpuProfiler::init()
    {
        Config config;
        if (const char* value = getEnv("NAU_GPU_PROFILER"))
        {
            config.enabled = std::atoi(value) != 0;
        }
        if (const char* value = getEnv("NAU_GPU_PROFILER_CSV"))
        {
            std::snprintf(config.csvPath, sizeof(config.csvPath), "%s", value);
        }
        readUint("NAU_GPU_PROFILER_LOG_EVERY", config.logEveryNFrames);
        readUint("NAU_GPU_PROFILER_WARMUP", config.warmupFrames);
        readUint("NAU_GPU_PROFILER_MAX_PENDING", config.maxPendingFrames);
        readUint("NAU_GPU_PROFILER_TIMEOUT_FRAMES", config.timeoutFrames);
        readUint("NAU_GPU_PROFILER_TIMEOUT_MS", config.timeoutMs);
        init(config);
    }

    void GpuProfiler::init(const Config& config)
    {
        if (m_impl != nullptr || !config.enabled)
        {
            return;
        }

        m_impl = new Impl();
        Impl& impl = *m_impl;
        impl.cpuQueue = impl.writer.addQueue("cpu");
        impl.config = config;
        impl.config.maxPendingFrames = eastl::max<uint32_t>(impl.config.maxPendingFrames, 1);
        for (Impl::GpuState& state : impl.gpus)
        {
            state.pending.resize(impl.config.maxPendingFrames);
        }

        if (!impl.writer.open(impl.config.csvPath))
        {
            impl.log(LogLevel::Error, "cannot open '%s', the profiler stays disabled", impl.config.csvPath);
            delete m_impl;
            m_impl = nullptr;
            return;
        }

        m_enabled = true;
        impl.log(LogLevel::Info, "enabled: csv '%s', log every %u frames, warmup %u frames, %u pending frames, timeout %u frames / %u ms",
                 impl.config.csvPath, impl.config.logEveryNFrames, impl.config.warmupFrames, impl.config.maxPendingFrames,
                 impl.config.timeoutFrames, impl.config.timeoutMs);
    }

    void GpuProfiler::shutdown()
    {
        if (m_impl == nullptr)
        {
            return;
        }
        Impl& impl = *m_impl;
        m_enabled = false;

        impl.collect(true);
        impl.logSummary();
        for (SourceId id = 0; id < MaxSources; ++id)
        {
            if (impl.sourceUsed[id])
            {
                for (void* query : impl.sources[id].freeQueries)
                {
                    impl.sources[id].source->release(query);
                }
            }
        }

        const Counters& c = impl.counters;
        impl.log(LogLevel::Info, "summary: %llu GPU frames measured, %llu skipped (pending queue full), %llu timed out, %llu still "
                 "in flight at shutdown, %llu scopes not measured", static_cast<unsigned long long>(c.framesMeasured),
                 static_cast<unsigned long long>(c.framesSkipped), static_cast<unsigned long long>(c.framesTimedOut),
                 static_cast<unsigned long long>(c.framesInFlightAtShutdown), static_cast<unsigned long long>(c.scopesUnmeasured));
        impl.writer.close();
        const uint64_t dropped = c.recordsDropped + impl.writer.dropped();
        if (dropped != 0)
        {
            impl.log(LogLevel::Warning, "%llu CSV records were dropped (writer queue full)", static_cast<unsigned long long>(dropped));
        }

        delete m_impl;
        m_impl = nullptr;
    }

    void GpuProfiler::setLogSink(const LogSink sink)
    {
        g_logSink = sink != nullptr ? sink : &defaultLogSink;
    }

    const Config& GpuProfiler::config() const
    {
        static const Config disabled;
        return m_impl != nullptr ? m_impl->config : disabled;
    }

    const Counters& GpuProfiler::counters() const
    {
        static const Counters none;
        return m_impl != nullptr ? m_impl->counters : none;
    }

    SourceId GpuProfiler::registerSource(const GpuIndex gpu, const char* queueName, ITimestampSource* source)
    {
        if (m_impl == nullptr || source == nullptr || gpu >= MaxGpus)
        {
            return InvalidSource;
        }
        Impl& impl = *m_impl;

        const uint64_t frequency = source->getFrequency();
        if (frequency == 0)
        {
            impl.log(LogLevel::Warning, "queue '%s' on GPU %u has no timestamps, it is not measured", queueName, gpu);
            return InvalidSource;
        }

        for (SourceId id = 0; id < MaxSources; ++id)
        {
            if (!impl.sourceUsed[id])
            {
                impl.sourceUsed[id] = true;
                Impl::Source& entry = impl.sources[id];
                entry = {};
                entry.source = source;
                entry.gpu = gpu;
                entry.frequency = frequency;
                entry.queueId = static_cast<uint8_t>(impl.writer.addQueue(queueName));
                impl.log(LogLevel::Info, "queue '%s' on GPU %u: %llu timestamp ticks per second", queueName, gpu,
                         static_cast<unsigned long long>(frequency));
                return id;
            }
        }
        impl.log(LogLevel::Error, "too many timestamp sources, queue '%s' is not measured", queueName);
        return InvalidSource;
    }

    void GpuProfiler::unregisterSource(const SourceId id)
    {
        if (m_impl == nullptr || id >= MaxSources || !m_impl->sourceUsed[id])
        {
            return;
        }
        Impl& impl = *m_impl;
        Impl::Source& source = impl.sources[id];

        // Its scopes in flight are dropped: the source (and its device) goes away.
        for (Impl::GpuState& state : impl.gpus)
        {
            auto removeFrom = [&](Impl::FrameRecord& record)
            {
                for (Impl::Scope& scope : record.scopes)
                {
                    if (scope.source == id)
                    {
                        source.source->release(scope.begin);
                        source.source->release(scope.end);
                        scope.source = InvalidSource;
                    }
                }
            };
            removeFrom(state.current);
            for (Impl::FrameRecord& record : state.pending)
            {
                removeFrom(record);
            }
        }
        for (void* query : source.freeQueries)
        {
            source.source->release(query);
        }
        source = {};
        impl.sourceUsed[id] = false;
    }

    SourceId GpuProfiler::findSource(const GpuIndex gpu) const
    {
        if (m_impl == nullptr)
        {
            return InvalidSource;
        }
        SourceId fallback = InvalidSource;
        for (SourceId id = 0; id < MaxSources; ++id)
        {
            const Impl::Source& source = m_impl->sources[id];
            if (m_impl->sourceUsed[id] && source.gpu == gpu && source.source->canIssue())
            {
                if (id != PrimarySource)
                {
                    return id;
                }
                fallback = id;
            }
        }
        return fallback;
    }

    void GpuProfiler::beginFrame()
    {
        if (m_impl == nullptr)
        {
            return;
        }
        Impl& impl = *m_impl;
        if (impl.frameOpen)
        {
            endFrame();
        }

        ++impl.frame;
        impl.frameOpen = true;
        impl.collect(false);

        impl.measuring = impl.frame > impl.config.warmupFrames;
        for (GpuIndex gpu = 0; gpu < MaxGpus; ++gpu)
        {
            Impl::GpuState& state = impl.gpus[gpu];
            state.frameActive = impl.measuring;
            state.current.frame = impl.frame;
            state.current.scopes.clear();

            // Every GPU has its own queue of pending frames: a full queue of one GPU does not stop the other one.
            if (state.frameActive && impl.freePendingSlot(state) == nullptr)
            {
                state.frameActive = false;
                if (impl.counters.framesSkipped++ % 100 == 0)
                {
                    impl.log(LogLevel::Warning, "frame %llu not measured on GPU %u: %u frames are still waiting for its "
                             "results (%llu skipped so far)", static_cast<unsigned long long>(impl.frame), gpu,
                             impl.config.maxPendingFrames, static_cast<unsigned long long>(impl.counters.framesSkipped));
                }
            }
        }
        beginScope("Frame", PrimarySource);
    }

    void GpuProfiler::endFrame()
    {
        if (m_impl == nullptr || !m_impl->frameOpen)
        {
            return;
        }
        Impl& impl = *m_impl;
        endScope(PrimarySource);

        while (!impl.cpuScopes.empty())
        {
            endCpuScope();
        }
        for (Impl::Source& source : impl.sources)
        {
            source.openScopes.clear();
        }
        // Scopes still open at the end of the frame have no end timestamp: not measured.
        for (Impl::GpuState& state : impl.gpus)
        {
            for (Impl::Scope& scope : state.current.scopes)
            {
                if (scope.source != InvalidSource && scope.end == nullptr)
                {
                    impl.dropQuery(scope.source, scope.begin);
                    scope.source = InvalidSource;
                    ++impl.counters.scopesUnmeasured;
                }
            }
        }

        // CPU time of the frame: between the ends of two neighbouring frames (the present follows endFrame()).
        const uint64_t now = qpcNow();
        const uint8_t mode = 0;
        if (impl.measuring && impl.lastFrameEnd != 0)
        {
            impl.emit(RecordKind::CpuFrame, impl.frame, impl.nameId("cpu_frame"), CpuGpu, impl.cpuQueue, mode,
                      qpcToMs(now - impl.lastFrameEnd));
        }
        impl.lastFrameEnd = now;

        for (Impl::GpuState& state : impl.gpus)
        {
            if (state.frameActive && !state.current.scopes.empty())
            {
                Impl::FrameRecord* slot = impl.freePendingSlot(state);
                // beginFrame() has checked that a slot is free
                slot->used = true;
                slot->frame = state.current.frame;
                slot->mode = mode;
                slot->closed = Impl::Clock::now();
                eastl::swap(slot->scopes, state.current.scopes);
            }
            state.current.scopes.clear();
            state.frameActive = false;
        }

        impl.frameOpen = false;
        if (impl.measuring && impl.config.logEveryNFrames != 0 && ++impl.framesSinceLog >= impl.config.logEveryNFrames)
        {
            impl.logSummary();
        }
    }

    uint64_t GpuProfiler::frameIndex() const
    {
        return m_impl != nullptr ? m_impl->frame : 0;
    }

    void GpuProfiler::beginScope(const char* name, const SourceId id)
    {
        if (m_impl == nullptr || id >= MaxSources || !m_impl->sourceUsed[id])
        {
            return;
        }
        Impl& impl = *m_impl;
        Impl::Source& source = impl.sources[id];
        Impl::GpuState& state = impl.gpus[source.gpu];

        if (!state.frameActive || !source.source->canIssue())
        {
            if (state.frameActive)
            {
                ++impl.counters.scopesUnmeasured;
            }
            source.openScopes.push_back(~0u);
            return;
        }

        Impl::Scope scope;
        scope.source = id;
        scope.name = impl.nameId(name);
        scope.begin = impl.acquireQuery(source);
        source.source->issue(scope.begin);
        source.openScopes.push_back(static_cast<uint32_t>(state.current.scopes.size()));
        state.current.scopes.push_back(scope);
    }

    void GpuProfiler::endScope(const SourceId id)
    {
        if (m_impl == nullptr || id >= MaxSources || !m_impl->sourceUsed[id])
        {
            return;
        }
        Impl& impl = *m_impl;
        Impl::Source& source = impl.sources[id];
        if (source.openScopes.empty())
        {
            return;
        }
        const uint32_t index = source.openScopes.back();
        source.openScopes.pop_back();

        Impl::GpuState& state = impl.gpus[source.gpu];
        if (index == ~0u || index >= state.current.scopes.size())
        {
            return;
        }

        Impl::Scope& scope = state.current.scopes[index];
        if (scope.source == InvalidSource)
        {
            return;
        }
        if (!source.source->canIssue())
        {
            // The command list was closed inside the scope: the begin timestamp has no end. The scope stays in the
            // list (the indices of other open scopes point into it) and is skipped.
            impl.dropQuery(id, scope.begin);
            scope.source = InvalidSource;
            ++impl.counters.scopesUnmeasured;
            return;
        }
        scope.end = impl.acquireQuery(source);
        source.source->issue(scope.end);
    }

    void GpuProfiler::beginCpuScope(const char* name)
    {
        if (m_impl == nullptr)
        {
            return;
        }
        m_impl->cpuScopes.push_back({m_impl->nameId(name), qpcNow()});
    }

    void GpuProfiler::endCpuScope()
    {
        if (m_impl == nullptr || m_impl->cpuScopes.empty())
        {
            return;
        }
        Impl& impl = *m_impl;
        const Impl::CpuScope scope = impl.cpuScopes.back();
        impl.cpuScopes.pop_back();
        if (impl.measuring)
        {
            impl.emit(RecordKind::Cpu, impl.frame, scope.name, CpuGpu, impl.cpuQueue, 0, qpcToMs(qpcNow() - scope.start));
        }
    }
}  // namespace nau::gpu_profiler
