// Copyright 2024 N-GINN LLC. All rights reserved.
// Use of this source code is governed by a BSD-3 Clause license that can be found in the LICENSE file.

// Connects the GPU profiler (nau/gpu_profiler/gpu_profiler.h) to Graphics: Nau's driver as the timestamp source of
// GPU 0, the log, the frame of the render thread.
#pragma once

namespace nau::render
{
    // After the driver and the swapchain are created. Does nothing unless NAU_GPU_PROFILER=1.
    void gpu_profiler_startup();
    // Before the driver is released.
    void gpu_profiler_shutdown();

    // Around the frame on the render thread, while the driver is owned (ACQUIRE_OWNERSHIP .. update_screen).
    void gpu_profiler_begin_frame();
    void gpu_profiler_end_frame();
}  // namespace nau::render
