// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// CPU clock of the profiler: QueryPerformanceCounter on Windows, the one GetClockCalibration relates GPU clocks to.
#pragma once

#include <cstdint>

#if defined(_WIN32)
    #ifndef WIN32_LEAN_AND_MEAN
        #define WIN32_LEAN_AND_MEAN
    #endif
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <chrono>
#endif

namespace nau::gpu_profiler
{
    inline uint64_t qpcFrequency()
    {
#if defined(_WIN32)
        static const uint64_t frequency = []
        {
            LARGE_INTEGER value;
            QueryPerformanceFrequency(&value);
            return static_cast<uint64_t>(value.QuadPart);
        }();
        return frequency;
#else
        return 1000000000ull;
#endif
    }

    inline uint64_t qpcNow()
    {
#if defined(_WIN32)
        LARGE_INTEGER value;
        QueryPerformanceCounter(&value);
        return static_cast<uint64_t>(value.QuadPart);
#else
        return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                         std::chrono::steady_clock::now().time_since_epoch()).count());
#endif
    }

    inline double qpcToMs(const uint64_t ticks)
    {
        return static_cast<double>(ticks) * 1000.0 / static_cast<double>(qpcFrequency());
    }
}  // namespace nau::gpu_profiler
