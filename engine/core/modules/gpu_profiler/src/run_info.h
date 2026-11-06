// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Metadata of a profiler run, written as JSON next to the CSV (<csv name>.json): build (commit, local changes, preset,
// configuration), adapters with driver versions, resolution changes, vsync and other key/values set by the engine,
// warmup, start time, duration, power source, frame counts per mode and features, counters.
// Written once at start (complete=false) and again at shutdown (complete=true).
#pragma once

#include <EASTL/array.h>
#include <EASTL/string.h>
#include <EASTL/vector.h>

#include <cstdint>

#include "nau/gpu_profiler/gpu_profiler.h"

namespace nau::gpu_profiler
{
    class RunInfo
    {
    public:
        void start(const Config& config);
        void setGpuName(GpuIndex gpu, const char* name);
        void set(const char* key, const char* value);
        void setResolution(uint64_t frame, uint32_t width, uint32_t height);
        // A measured frame (after the warmup) with its features text.
        void countFrame(bool twoGpus, const eastl::string& features);
        void finish(uint64_t frames, const Counters& counters);

    private:
        struct Resolution
        {
            uint64_t frame = 0;
            uint32_t width = 0;
            uint32_t height = 0;
        };

        struct FeatureCount
        {
            eastl::string features;
            uint64_t frames = 0;
        };

        void write(bool complete, uint64_t frames, const Counters* counters) const;

        Config m_config;
        eastl::string m_path;
        eastl::string m_startTime;
        eastl::string m_powerAtStart;
        uint64_t m_startQpc = 0;
        eastl::array<eastl::string, MaxGpus> m_gpuNames;
        eastl::vector<eastl::pair<eastl::string, eastl::string>> m_values;
        eastl::vector<Resolution> m_resolutions;
        eastl::vector<FeatureCount> m_features;
        uint64_t m_frames1Gpu = 0;
        uint64_t m_frames2Gpu = 0;
    };
}  // namespace nau::gpu_profiler
