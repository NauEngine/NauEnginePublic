# GPU profiler

Measures the same scene rendered on one GPU and on two GPUs (`nau::mgpu`), so the two modes can be compared:
GPU time of every pass on every GPU, the cost of transfers between the GPUs and the CPU frame time.

Code: `engine/core/modules/gpu_profiler` (static library `GpuProfiler`, linked by Graphics and by the Mgpu module).
Its header `nau/gpu_profiler/gpu_profiler.h` has only plain C++ types, so the Mgpu module uses it without seeing Nau's
headers.

## Enabling

The profiler is off by default and then costs one `bool` check per timer. It is configured with environment
variables:

| Variable | Default | Meaning |
|---|---|---|
| `NAU_GPU_PROFILER` | `0` | `1` enables the profiler |
| `NAU_GPU_PROFILER_CSV` | `gpu_timings.csv` | CSV output, relative to the working directory |
| `NAU_GPU_PROFILER_LOG_EVERY` | `300` | averages in the log every N frames, `0` = never |
| `NAU_GPU_PROFILER_WARMUP` | `120` | the first N frames are not measured |
| `NAU_GPU_PROFILER_MAX_PENDING` | `8` | frames waiting for the results of one GPU |
| `NAU_GPU_PROFILER_TIMEOUT_FRAMES` | `64` | a frame whose results have not arrived after N frames is dropped |
| `NAU_GPU_PROFILER_TIMEOUT_MS` | `2000` | ... or after N milliseconds |
| `NAU_VSYNC` | on | `0` switches vsync off (DX12 driver) |

**Switch vsync off for measurements** (`NAU_VSYNC=0`): with vsync the frame time is bound to the refresh rate of the
display and both modes show the same frame rate. The profiler warns in the log when vsync is on. Without vsync the
swapchain presents with `DXGI_PRESENT_ALLOW_TEARING` when the adapter supports it, so the frame rate is not limited in
windowed mode either. The driver logs `DX12: vsync on|off` at start.

PowerShell, `SceneBaseSample` from the `win_vs2022_x64_mgpu` preset:

```powershell
$env:NAU_GPU_PROFILER = "1"
$env:NAU_VSYNC = "0"
$env:NAU_GPU_PROFILER_CSV = "C:\temp\run.csv"
Start-Process build\win_vs2022_x64_mgpu\bin\Release\SceneBaseSample.exe -WorkingDirectory samples\sceneBase
```

## Measurement method

- `NAU_VSYNC=0`, otherwise both modes run at the refresh rate of the display.
- **Do not resize the window during a run.** A resize recreates the render targets and recomputes the environment
  cubemaps (`compute_env_cubemaps`): single frames of ~700 ms on GPU 0 that distort the mean and the tail. Set the size
  before the run, and skip the first seconds (warmup).
- Same scene, same camera, same window size in both runs; the laptop on mains power, nothing else heavy running.
- Laptop GPUs and CPUs change their clocks with temperature: alternate the runs (1 GPU, 2 GPU, 1 GPU, 2 GPU) instead of
  doing all runs of one mode first, and compare the spread between runs of the same mode with the difference between
  the modes.

`NAU_FRAME_TIMES_CSV=<path>` writes the CPU time of every frame (`frame,ms`) independently of `NAU_GPU_PROFILER`; it
costs the same with the profiler on and off and is used to measure the overhead of the profiler itself.

## Overhead of the profiler

CPU frame time from `NAU_FRAME_TIMES_CSV`, `SceneBaseSample` Release (`win_vs2022_x64_mgpu`), `NAU_VSYNC=0`, default
window 650x650 never resized, default scene state (shadows on the second GPU, DOF off), 60 s per run, the first 10 s
skipped, runs alternated. Laptop on mains power: NVIDIA RTX 3050 Ti Laptop (scene), Intel Iris Xe (shadows).

After stage 1 (synchronous CSV, every framegraph node timed on GPU 0), ms:

| Run | `NAU_GPU_PROFILER` | Frames | Mean | Median | p95 | p99 |
|---|---|---|---|---|---|---|
| off 1 | 0 | 13587 | 3.544 | 3.265 | 6.202 | 7.414 |
| on 1 | 1 | 14866 | 3.292 | 3.212 | 3.981 | 5.753 |
| off 2 | 0 | 15325 | 3.196 | 3.184 | 3.826 | 4.133 |
| on 2 | 1 | 14635 | 3.351 | 3.241 | 4.479 | 5.855 |

The overhead is below the noise of the machine: the two runs without the profiler differ more (mean 3.54 vs 3.20 ms,
p95 6.2 vs 3.8 ms) than the runs with and without it, and the medians are within 0.06 ms (about 2%). The p95 of a single
run on this laptop is not stable enough to show a difference of that size. To be repeated after stage 5 (asynchronous
CSV).

## What is measured

- **GPU 0, every render graph node**, by node name (`/CSM_0`, `/fill_gbuffer_0`...): the framegraph executor
  (`daBfg/runtime/nodeExecutor.cpp`) wraps every node in `NAU_GPU_TIMER`, through the timestamps of Nau's driver.
- **`Frame`** on GPU 0: the whole frame, from `beginFrame()` to the end of the last node.
- **`cpu_frame`**: CPU time between the ends of two neighbouring frames (QueryPerformanceCounter, the present follows).
  The 1 GPU / 2 GPU comparison is based on this value; GPU times explain where the time goes.
- **CPU timers** (`NAU_CPU_TIMER`): `CSM` (the whole CSM node), `CSM/mgpu` (the second-GPU part of it: collecting
  casters, handing the frame over, taking the ready atlas), `dof_multi_gpu` (the DOF bridge, which waits on the CPU for
  both GPUs).

A GPU time of a node only covers the work recorded through Nau's driver. Work that the Mgpu bridges submit to their own
queues is measured by the bridges themselves.

## Raw D3D12 queues

Queues that do not belong to Nau's driver (the Mgpu bridges: on GPU 1, and their own device and copy queue on GPU 0)
are measured with `D3D12TimestampSource` (`nau/gpu_profiler/d3d12_timestamp_source.h`, Windows only, included only by
`.cpp` files that already use D3D12). Each source owns a query heap, a readback buffer and a fence on the device of its
queue and is registered with the GPU index and a queue name, so one physical GPU can have several sources (`queue`
column). Direct and compute queues use `D3D12_QUERY_HEAP_TYPE_TIMESTAMP`, copy queues
`D3D12_QUERY_HEAP_TYPE_COPY_QUEUE_TIMESTAMP` when `D3D12_OPTIONS3::CopyQueueTimestampQueriesSupported` is set; otherwise
the copy queue is not measured and the log says so. Every source uses the frequency of its own queue: on the test laptop
the Intel queues tick at 19.2 MHz and the NVIDIA queues at 1 GHz.

The owner of the command list drives the source: `setCommandList(list)` before the timers, `resolve()` before
`Close()`, `setCommandList(nullptr)`, then `submitted()` after `ExecuteCommandLists`. Nothing waits for the GPU: values
are read once the source's fence has passed.

### Second GPU in the bridges

| Pass | GPU / queue | What |
|---|---|---|
| `csm_atlas_draw` | `1` / `gfx1.csm` | the shadow atlas on the second GPU: clear and all cascades, without the copy to the shared heap (`mgpu_shadows.cpp`) |
| `dof_compute` | `1` / `gfx1.dof` | the nine DOF passes on the second GPU, without the copies from and to the shared buffers (`dof_bridge.cpp`) |

The sources only add timestamps to the bridges' command lists and signal their own fence after the bridge's signal;
they add no CPU wait and do not change the order of the bridges' fences.

Reading the GPU 0 rows of these features:

- `/CSM_0` with the atlas from the second GPU is the import of that atlas (`DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE`)
  in the frames that get a new one; without it, Nau draws the cascades.
- `/dof_multi_gpu_0` with DOF on the second GPU spans the bridge's own submissions on Nau's queue, including the GPU wait
  of that queue for the second GPU (`dof_bridge.cpp` waits on both GPUs). It is a wall-clock span of the queue, not
  pure work. The CPU timer `dof_multi_gpu` shows the CPU wait of the same synchronous pass.

`GpuProfilerD3D12Check` (built with `NAU_CORE_TESTS`) checks the source on every hardware adapter of the machine:
direct, compute and copy queue, 64 MB buffer copy per frame, including a source unregistered while its last frame is
still in flight. Exit code 0 means every supported queue was measured.

## CSV

```
frame,pass,gpu,queue,ms,mode,features,age
```

| Column | Meaning |
|---|---|
| `frame` | profiler frame number |
| `pass` | node or timer name |
| `gpu` | `0`, `1`, or `cpu` for CPU timers and `cpu_frame` |
| `queue` | queue the time was measured on (`gfx0`: Nau's graphics queue), `cpu` |
| `ms` | milliseconds |
| `mode` | `1gpu` / `2gpu` |
| `features` | where each feature ran in that frame |
| `age` | for transfers: frames between the start of the transfer and the use of its result |

GPU rows are written a few frames after their frame, once the GPU has written the timestamps; rows are not sorted by
frame.

## Robustness

- Results are read without waiting. Every GPU has its own queue of frames waiting for results. When it is full, the
  frame is not measured on that GPU and the `skipped` counter grows.
- A frame whose results do not arrive in time (a lost device, a bug in a bridge) is dropped with a log line and counted
  as `timed out`; the profiler never blocks rendering.
- The log gets a summary at shutdown: frames measured, skipped, timed out, still in flight at shutdown.

## Limitations

- One process-wide profiler, used from the render thread only.
- Timestamps of different GPUs and different queues are never subtracted from each other; they are related only through
  the clock calibration to the CPU counter.
