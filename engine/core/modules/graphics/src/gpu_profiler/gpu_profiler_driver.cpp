// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#include "gpu_profiler_driver.h"

#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>

#include "nau/3d/dag_drv3d.h"
#include "nau/3d/dag_drv3dCmd.h"
#include "nau/diag/logging.h"
#include "nau/gpu_profiler/gpu_profiler.h"

namespace nau::render
{
    namespace
    {
        namespace profiler = nau::gpu_profiler;

        // Timestamps of GPU 0 through Nau's driver: they are written into Nau's command stream on its graphics queue,
        // so render graph passes and everything recorded by the driver (including
        // DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE) can be timed.
        class DriverTimestampSource final : public profiler::ITimestampSource
        {
        public:
            uint64_t getFrequency() override
            {
                uint64_t frequency = 0;
                return d3d::driver_command(D3V3D_COMMAND_TIMESTAMPFREQ, &frequency, nullptr, nullptr) != 0 ? frequency : 0;
            }

            void issue(void*& query) override
            {
                d3d::driver_command(D3V3D_COMMAND_TIMESTAMPISSUE, &query, nullptr, nullptr);
            }

            bool tryGetValue(void* query, uint64_t& value) override
            {
                return query != nullptr && d3d::driver_command(D3V3D_COMMAND_TIMESTAMPGET, query, &value, nullptr) != 0;
            }

            void release(void* query) override
            {
                if (query != nullptr)
                {
                    d3d::driver_command(DRV3D_COMMAND_RELEASE_QUERY, &query, nullptr, nullptr);
                }
            }

            bool getClockCalibration(uint64_t& gpuTicks, uint64_t& cpuTicks, uint64_t& cpuFrequency) override
            {
                int frequencyType = DRV3D_CPU_FREQ_TYPE_UNKNOWN;
                if (d3d::driver_command(D3V3D_COMMAND_TIMECLOCKCALIBRATION, &cpuTicks, &gpuTicks, &frequencyType) == 0 ||
                    frequencyType != DRV3D_CPU_FREQ_TYPE_QPC)
                {
                    return false;
                }
                cpuFrequency = 0; // the profiler's own QPC frequency
                return true;
            }
        };

        DriverTimestampSource g_driverSource;
        bool g_registered = false;

        // NAU_FRAME_TIMES_CSV=<path>: CPU frame times of every frame, independent of NAU_GPU_PROFILER. It costs the
        // same with the profiler on and off, so it measures the overhead of the profiler itself (doc/gpu_profiler.md).
        // The times go into memory reserved at start and are written at shutdown.
        class FrameTimeRecorder
        {
        public:
            void start()
            {
                const char* path = std::getenv("NAU_FRAME_TIMES_CSV");
                if (path == nullptr || *path == '\0')
                {
                    return;
                }
                m_path = path;
                m_times.reserve(MaxFrames);
                m_active = true;
                NAU_LOG_INFO("frame times: recording up to {} frames into '{}'", MaxFrames, m_path);
            }

            void frameEnd()
            {
                if (!m_active)
                {
                    return;
                }
                const auto now = std::chrono::steady_clock::now();
                if (m_hasLast && m_times.size() < MaxFrames)
                {
                    m_times.push_back(std::chrono::duration<float, std::milli>(now - m_last).count());
                }
                m_last = now;
                m_hasLast = true;
            }

            void finish()
            {
                if (!m_active)
                {
                    return;
                }
                m_active = false;
                FILE* file = std::fopen(m_path.c_str(), "w");
                if (file == nullptr)
                {
                    NAU_LOG_ERROR("frame times: cannot write '{}'", m_path);
                    return;
                }
                std::fputs("frame,ms\n", file);
                for (size_t i = 0; i < m_times.size(); ++i)
                {
                    std::fprintf(file, "%zu,%.4f\n", i + 1, m_times[i]);
                }
                std::fclose(file);
                NAU_LOG_INFO("frame times: {} frames written to '{}'", m_times.size(), m_path);
            }

        private:
            static constexpr size_t MaxFrames = 1 << 20;

            bool m_active = false;
            bool m_hasLast = false;
            eastl::string m_path;
            eastl::vector<float> m_times;
            std::chrono::steady_clock::time_point m_last;
        };

        FrameTimeRecorder g_frameTimes;

        void logToNau(const profiler::LogLevel level, const char* message)
        {
            switch (level)
            {
                case profiler::LogLevel::Info:
                    NAU_LOG_INFO("gpu_profiler: {}", message);
                    break;
                case profiler::LogLevel::Warning:
                    NAU_LOG_WARNING("gpu_profiler: {}", message);
                    break;
                case profiler::LogLevel::Error:
                    NAU_LOG_ERROR("gpu_profiler: {}", message);
                    break;
            }
        }
    }  // namespace

    void gpu_profiler_startup()
    {
        g_frameTimes.start();

        profiler::GpuProfiler& instance = profiler::GpuProfiler::instance();
        instance.setLogSink(&logToNau);
        instance.init();
        if (!instance.isEnabled())
        {
            return;
        }

        // Nau's driver is the first source, so it gets profiler::PrimarySource.
        g_registered = instance.registerSource(profiler::PrimaryGpu, "gfx0", &g_driverSource) == profiler::PrimarySource;
        if (!g_registered)
        {
            NAU_LOG_WARNING("gpu_profiler: Nau's driver ({}) provides no timestamps, GPU 0 is not measured",
                            d3d::get_driver_name());
        }

        if (d3d::get_vsync_enabled())
        {
            NAU_LOG_WARNING("gpu_profiler: vsync is on, frame times are bound to the display refresh rate and the 1 GPU / "
                            "2 GPU comparison is distorted; run with NAU_VSYNC=0");
        }
    }

    void gpu_profiler_shutdown()
    {
        profiler::GpuProfiler::instance().shutdown();
        g_registered = false;
        g_frameTimes.finish();
    }

    void gpu_profiler_begin_frame()
    {
        profiler::GpuProfiler::instance().beginFrame();
    }

    void gpu_profiler_end_frame()
    {
        // Before the profiler's own work, at the same point of every frame in both runs.
        g_frameTimes.frameEnd();
        profiler::GpuProfiler::instance().endFrame();
    }
}  // namespace nau::render
