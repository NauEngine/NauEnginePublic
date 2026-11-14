// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

#pragma once

#include "nau/gpu_profiler/gpu_profiler.h"

namespace nau::gpu_profiler
{
    // printf-style message to the sink set with GpuProfiler::setLogSink().
    void logMessage(LogLevel level, const char* format, ...);
}  // namespace nau::gpu_profiler
