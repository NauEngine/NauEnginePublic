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
run on this laptop is not stable enough to show a difference of that size.

After stage 5 (asynchronous CSV writer; GPU 1 scopes and transfers measured as well, ~650 000 CSV rows per 60 s run,
0 records dropped), same method, ms:

| Run | `NAU_GPU_PROFILER` | Frames | Mean | Median | p95 | p99 |
|---|---|---|---|---|---|---|
| off 1 | 0 | 14861 | 3.269 | 3.206 | 3.962 | 5.741 |
| on 1 | 1 | 14950 | 3.274 | 3.201 | 3.994 | 5.558 |
| off 2 | 0 | 15239 | 3.217 | 3.192 | 3.897 | 4.428 |
| on 2 | 1 | 14803 | 3.313 | 3.217 | 4.157 | 5.848 |

These runs were quieter than the first ones (the two runs without the profiler are within 0.05 ms in mean and 0.07 ms
in p95). The medians of all four runs are within 0.025 ms; with the profiler the mean is ~0.05 ms higher (~1.5%) and
the p95 0.03-0.26 ms higher. The overhead of a measured frame is in the order of 0.05 ms on the render thread.

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

## Transfers between GPUs

```cpp
{ NAU_GPU_TRANSFER_SRC("csm_atlas", 1, 0, token, source1); /* copy on GPU 1 */ }
{ NAU_GPU_TRANSFER_DST("csm_atlas", 1, 0, token, source0); /* copy on GPU 0, maybe frames later */ }
GpuProfiler::instance().markTransferConsumed("csm_atlas", token); // in the frame that uses the result
```

Each half is timed on the GPU and the queue that executes it. The halves are matched by **name and token**, not by
the frame: the shadow atlas is drawn and sent in one frame, moved into local memory of GPU 0 frames later and used by
Nau later still. A transfer is written once both halves have arrived and the result was used (or the bridge discarded
it), with these rows, all with the frame that started the transfer and its `age`:

| Row | `gpu` | Value |
|---|---|---|
| `name/src` | source GPU | time of the source half on its queue |
| `name/dst` | destination GPU | time of the destination half on its queue |
| `name/sum` | `1>0` | `src + dst`: pure GPU work of the transfer |
| `name/span` | `1>0` | from the start of `src` to the end of `dst` on the CPU clock, through `GetClockCalibration` of both queues: includes waiting for fences, queues and the CPU between the halves; only when both queues are calibrated |

`age` is the number of frames between the frame that started the transfer and the frame that used its result (0 for a
synchronous transfer).

Counters in the summary at shutdown: `complete`; `unused` (the bridge discarded the result, e.g. an atlas superseded by
a newer one, or it was never used); `incomplete` (a half did not arrive within `NAU_GPU_PROFILER_TIMEOUT_FRAMES`: lost
with a dropped frame, or a bridge bug); `in flight at shutdown`. Transfers that started during the warmup are not
written.

| Transfer | `src` | `dst` | Token | Used |
|---|---|---|---|---|
| `csm_atlas` | GPU 1 `gfx1.csm`: atlas -> shared heap | GPU 0 `copy0.csm` (Mgpu's copy queue): shared heap -> local memory | `FrameDesc::userToken` | `mgpu_nodes.cpp`, when the atlas is copied into Nau's atlas |
| `dof_in` | GPU 0 `gfx0.dof` (the bridge's list on Nau's queue): color, depth -> shared buffers | GPU 1 `gfx1.dof`: shared buffers -> textures | frame | same frame |
| `dof_out` | GPU 1 `gfx1.dof`: results -> shared buffers | GPU 0 `gfx0.dof`: shared buffers -> Nau's textures | frame | same frame |

`csm_atlas_import` (GPU 0, `gfx0`) is the copy of the atlas from local memory into Nau's atlas
(`DRV3D_COMMAND_D3D12_COPY_FROM_EXTERNAL_RESOURCE`), a pass of Nau's frame, not a half of the transfer.

The `TransferBridge` demo (`MgpuBridgeTransferTest`) delivers wrong pixels on the test laptop (NVIDIA + Intel); if it is
measured, its times are real but its data is not valid.

`GpuProfilerD3D12Check` (built with `NAU_CORE_TESTS`) checks the source on every hardware adapter of the machine:
direct, compute and copy queue, 64 MB buffer copy per frame, including a source unregistered while its last frame is
still in flight. Exit code 0 means every supported queue was measured.

## CSV

```
frame,pass,gpu,queue,ms,mode,features,age,bytes
```

| Column | Meaning |
|---|---|
| `frame` | profiler frame number |
| `pass` | node or timer name |
| `gpu` | `0`, `1`, or `cpu` for CPU timers and `cpu_frame` |
| `queue` | queue the time was measured on (`gfx0`: Nau's graphics queue), `cpu` |
| `ms` | milliseconds |
| `mode` | `2gpu` when at least one feature used a result of GPU 1 in that frame, else `1gpu` |
| `features` | where each feature actually ran in that frame, sorted by name: `csm=gpu1;dof=off` |
| `age` | for transfers: frames between the start of the transfer and the use of its result |
| `bytes` | for transfers: payload size in bytes (logical size of the copied data), for the bandwidth `bytes / ms` |

GPU rows are written a few frames after their frame, once the GPU has written the timestamps; rows are not sorted by
frame.

The file is written by a background thread. The render thread puts fixed-size records (20 bytes: frame, name id,
GPU, queue, kind, mode, age, ms, bytes) into a lock-free single-producer single-consumer ring of 65 536 records and
never waits: when the ring is full the record is dropped and counted. Names are replaced by ids; a new name is handed to
the writer thread once (names are rare, the mutex of that hand-over is not taken for ordinary records). The thread wakes
up every 20 ms, formats the records and writes them through a 1 MB file buffer; at shutdown it writes everything still
queued and stops. Dropped records appear in the log as soon as they grow (with the periodic averages) and in the
summary at shutdown (`CSV: N records dropped`).

## Mode of a frame

The mode is the **actual** path of every frame, not the requested one. The engine reports where each feature ran
(`GpuProfiler::setPlacement`):

| Feature | Set in | `off` | `gpu0` | `gpu1` |
|---|---|---|---|---|
| `csm` | CSM node (`gbuffer_nodes.cpp`) | no cascades this frame | Nau drew the cascades | Nau's atlas holds an atlas of the second GPU |
| `dof` | `depth_of_field` node (`render_graph.cpp`) | DOF off (F9) | the local DOF chain | the result of the second GPU was used |

A frame is `2gpu` only when a result of GPU 1 was really used in it. With M switched on but the second GPU not ready
yet, or before the first atlas has arrived, frames are `1gpu` with `csm=gpu0`. Switching M or F8 during a run shows in
the following frames. All rows of a frame carry its mode and features (GPU rows, CPU rows, and the transfers started in
it).

## Run metadata

Next to the CSV the profiler writes `<csv name>.json` at start (`"complete": false`) and completes it at shutdown:

- `build`: commit, branch and `local_changes` (tracked files modified) at build time, preset (build directory), config;
- `adapters`: name, PCI id and driver version of GPU 0 and GPU 1;
- `resolution`: the render resolution with the frame of every change (more than one entry: the window was resized);
- `settings`: vsync, `NAU_VSYNC`, driver, whether Mgpu is linked;
- `profiler`: warmup, pending frames, timeouts;
- `start_time`, `end_time`, `duration_s`, `power_at_start`, `power_at_end` (mains or battery);
- `frames`, measured frames per mode and per features, and the counters of the summary.

## Comparing runs

`tools/scripts/compare_gpu_timings.py` compares runs; the first one is the baseline. `a.csv+b.csv` pools repeated
runs of one configuration. It uses the frames with the dominant `features` of each run and drops 60 frames after every
change of `features` (`--settle-frames`), so the frames before a key press do not mix in.

```powershell
py tools/scripts/compare_gpu_timings.py `
  runsgpu_1.csv+runsgpu_2.csv runsgpu_1.csv+runsgpu_2.csv `
  --label "1 GPU" --label "2 GPU" --md summary.md --json summary.json
```

The summary has:

- the runs (mode, features, frames used, resolution, vsync, commit);
- **warnings** when runs differ in features, resolution, commit, local changes, vsync, preset or configuration,
  adapters or drivers; when a run was built with local changes, not on mains power, resized, with vsync, or had many
  frames of other features;
- **CPU frame time** (`cpu_frame`): mean, median, p95, p99, FPS and the change against the baseline. This is the
  comparison of 1 GPU and 2 GPU;
- **shadow atlas from the second GPU**: share of frames that got a new atlas, the age of the atlases when used, the age
  of the atlas on screen in every frame (mean, median, p95, max), next to the FPS against the baseline: what the stale
  shadow bought;
- CPU timers, GPU passes (mean / median / p95), transfers (src, dst, sum, span with payload and GB/s).

The JSON has the same numbers plus the metadata of every run and SHA-256 of the source files. Recorded comparisons are
in `doc/measurements/` (the CSVs stay outside the repository).

### Two runs, step by step

1. Build `win_vs2022_x64_mgpu` Release from a clean tree (otherwise `local_changes` is true and the summary warns).
2. Same window size, laptop on mains power, nothing else heavy running.
3. For each configuration, start `SceneBaseSample` with `NAU_GPU_PROFILER=1`, `NAU_VSYNC=0` and its own
   `NAU_GPU_PROFILER_CSV`; set the mode with the keys (M: shadows on the second GPU, F9: DOF, F8: DOF on the second
   GPU); keep it running for a minute; close the window. Alternate the configurations and repeat them.
4. Run the script on the CSVs.

## Robustness

- Results are read without waiting. Every GPU has its own queue of frames waiting for results. When it is full, the
  frame is not measured on that GPU and the `skipped` counter grows.
- A frame whose results do not arrive in time (a lost device, a bug in a bridge) is dropped with a log line and counted
  as `timed out`; the profiler never blocks rendering. "In time": `NAU_GPU_PROFILER_TIMEOUT_FRAMES` frames, or
  `NAU_GPU_PROFILER_TIMEOUT_MS` once more than `NAU_GPU_PROFILER_MAX_PENDING` frames have passed (Nau's driver finalizes
  its queries as frames go on, so a CPU stall alone must not drop a frame).
- The log gets a summary at shutdown: frames measured, skipped, timed out, still in flight at shutdown; transfers
  complete, unused, incomplete; CSV records dropped.
- `GpuProfilerD3D12Check` also pushes 200 000 records in one frame: the ring overflows, the render thread does not wait,
  written + dropped = pushed, no broken rows.

## Limitations

- One process-wide profiler, used from the render thread only. It lives in a static library linked by Graphics and by
  Mgpu; this works because Mgpu is static too (it is only built with `NAU_ENABLE_MGPU`, which requires static
  libraries). A shared Mgpu would need the profiler exported from one module.
- The CPU frame time is measured at the end of the frame, before the present; with vsync on it is bound to the refresh
  rate.
- `TransferBridge` (the demo in `MgpuBridgeTransferTest`) delivers wrong pixels on the test laptop; its transfers are not
  measured.
- Timestamps of different GPUs and different queues are never subtracted from each other; they are related only through
  the clock calibration to the CPU counter.
