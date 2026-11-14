# Shadows and DOF on a second GPU: RTX 3050 Ti Laptop + Intel Iris Xe

`SceneBaseSample` with the GPU profiler (`doc/gpu_profiler.md`) in four configurations: the scene alone on one GPU, the
shadow cascades on the second GPU, shadows and DOF on the second GPU, DOF on one GPU. GPU 0 (Nau, scene):
NVIDIA GeForce RTX 3050 Ti Laptop GPU, PCI `10DE:25A0`, driver 32.0.15.7680. GPU 1 (helper): Intel Iris Xe Graphics,
PCI `8086:46A6`, driver 30.0.101.2079. Windows 11 26200, laptop on mains power during every run.

Machine-readable results, including the metadata of every run and SHA-256 of every source CSV and JSON:
[mgpu_profiler_rtx3050ti_irisxe_2026_11_06.json](mgpu_profiler_rtx3050ti_irisxe_2026_11_06.json). The CSVs stay outside
the repository.

## Method

- Build: `win_vs2022_x64_mgpu` Release, commit `ebf1606d` without local changes (recorded by each run).
- `NAU_GPU_PROFILER=1`, `NAU_VSYNC=0` (no vsync, tearing present in windowed mode), default window 634x611, never
  resized; profiler warmup 120 frames.
- Four configurations, each run 60 s, two rounds in opposite order (1 GPU, 2 GPU shadows, 2 GPU shadows + DOF,
  1 GPU + DOF, then the reverse) to spread the effect of the laptop's clocks and temperature. The two runs of a
  configuration are pooled.
- The mode is set with the keys 12 s after start (M: shadows on the second GPU off, F9: DOF on, F8: DOF on the second
  GPU off), each key confirmed by the log of that run. The shadows start on the second GPU by default.
- `tools/scripts/compare_gpu_timings.py` keeps the frames with the dominant `features` of each run and drops 60 frames
  after every change of `features`; so the frames before the key presses are left out. The comparison is based on the
  CPU frame time (`cpu_frame`, between the ends of two frames); GPU times explain it.

| Configuration | `features` | Frames used |
| --- | --- | ---: |
| 1 GPU (baseline) | `csm=gpu0;dof=off` | 29 501 |
| 2 GPU: shadows | `csm=gpu1;dof=off` | 35 511 |
| 2 GPU: shadows + DOF | `csm=gpu1;dof=gpu1` | 7 395 |
| 1 GPU + DOF | `csm=gpu0;dof=gpu0` | 22 137 |

## Results

### Frame time

| Configuration | Mean, ms | Median, ms | p95, ms | p99, ms | FPS (1000 / mean) |
| --- | ---: | ---: | ---: | ---: | ---: |
| 1 GPU | 3.175 | 3.146 | 3.691 | 4.842 | 315.0 |
| 2 GPU: shadows | 3.232 | 3.156 | 3.703 | 3.962 | 309.4 |
| 1 GPU + DOF | 3.877 | 3.829 | 4.867 | 5.222 | 257.9 |
| 2 GPU: shadows + DOF | 12.468 | 12.466 | 12.989 | 13.347 | 80.2 |

**Shadows on the second GPU do not make the frame faster on this pair**: the mean is 1.8% higher (−5.6 FPS), the
median the same within 0.01 ms; only the p99 is lower (3.96 vs 4.84 ms). **DOF on the second GPU makes it 3.2x slower**
than DOF on the scene GPU (12.47 vs 3.88 ms, 80 vs 258 FPS).

### What the stale shadow bought

| Configuration | Frames with a new atlas | Age when used (frames) | Age on screen: mean | median | p95 | max | FPS vs 1 GPU |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 2 GPU: shadows | 50.5% | 3 (98%), 2 (2%) | 3.48 | 3 | 4 | 4 | −5.6 |
| 2 GPU: shadows + DOF | 99.9% | 2 (100%) | 2.00 | 2 | 2 | 2 | (DOF dominates) |

With the shadows on Intel the scene shows an atlas drawn 3-4 frames earlier, and only every second frame gets a new
one; in exchange the frame is not faster. Intel delivers an atlas every ~6 ms (`csm_atlas/span` 6.3 ms), the scene runs
at ~3.2 ms per frame.

### Where the time goes

| | 1 GPU | 2 GPU: shadows |
| --- | --- | --- |
| Shadows on NVIDIA (GPU) | `/CSM_0` 0.097 ms: Nau draws the cascades | `csm_atlas_import` 0.124 ms in frames with a new atlas (copy of the atlas into Nau's) |
| Shadows on Intel (GPU) | - | `csm_atlas_draw` 0.342 ms + `csm_atlas/src` (atlas -> shared heap) 2.935 ms |
| Atlas into NVIDIA local memory | - | `csm_atlas/dst` on Mgpu's copy queue 0.660 ms (does not block the graphics queue) |
| CSM node, CPU (median / p95) | 0.278 / 0.338 ms | 0.175 / 0.288 ms (of which `CSM/mgpu` 0.166) |
| `Frame` on NVIDIA (median) | 0.565 ms | 0.550 ms |

- Importing the atlas on NVIDIA (0.124 ms) costs more than drawing the cascades there (0.097 ms): the GPU time of the
  scene GPU does not go down.
- In these runs the CSM node is ~0.1 ms cheaper on the CPU with the shadows on Intel. An earlier session (stage 3 of the
  profiler) measured the opposite (0.18 vs 0.26 ms); the CPU difference is at the level of the noise between sessions and
  does not show in the frame time.
- On Intel, copying the atlas into the shared heap (2.94 ms, 2.86 GB/s for 8 MiB through a row-major texture) is 8.6x
  the drawing (0.34 ms). DOF writes into shared buffers at 7.73 GB/s on the same GPU. See issue 4 for the mgpu team
  (copy the atlas through a buffer).

| DOF | 1 GPU + DOF | 2 GPU: shadows + DOF |
| --- | --- | --- |
| DOF passes (GPU) | nine local nodes on NVIDIA: ~0.36 ms | `dof_compute` on Intel 3.351 ms |
| Transfers | - | `dof_in` sum 0.64 ms, span 2.40 ms; `dof_out` sum 0.81 ms, span 1.18 ms |
| NVIDIA's queue in the DOF node | - | `/dof_multi_gpu_0` 7.90 ms (waits for Intel, not work) |
| CPU in the DOF node | 0 | `dof_multi_gpu` 8.33 ms |

The DOF path is synchronous: Nau's queue and the CPU wait for Intel and both transfers in every frame (issue 3 for the
mgpu team).

## Counters and validity

- Every run: 0 frames timed out, 0 frames skipped, 0 CSV records dropped, no scope unmeasured.
- Transfers: all `csm_atlas`, `dof_in` and `dof_out` transfers of the 2 GPU runs complete (a few in flight at shutdown).
  The runs where M switches the shadow path off each report 2 incomplete `csm_atlas` transfers: atlases in flight at the
  switch, whose last values the profiler dropped when the bridge unregistered its sources. Fixed afterwards in
  `6ac247d2` (values taken at unregistration, dropped atlases counted as unused); the timings above are not affected.
- The comparison script warns that the runs differ in `features`: expected, that is what is compared. The other
  warnings list the share of frames before the key presses, left out of the statistics.

## Limitations

- One scene, one camera position, one window size (634x611), one machine. At a larger resolution the scene GPU has more
  work and the balance may change; the atlas cost does not depend on the window.
- The CPU frame time is measured before the present; no queue trace (PIX/GPUView). Overlap of the GPUs is not measured
  directly, only through `span`.
- The helper GPU is an integrated GPU sharing memory and power with the CPU; a second discrete GPU would give different
  numbers.
- `TransferBridge` (the demo of `MgpuBridgeTransferTest`) is not part of these runs; it delivers wrong pixels on this
  machine (issue 1 for the mgpu team).

## Appendix: output of compare_gpu_timings.py

### Runs

Frames: the dominant features of each run, 60 frames after every change of features dropped.

| Run | Files | Mode | Features | Frames used | Of all | Resolution | vsync | Commit |
| --- | ---: | --- | --- | ---: | ---: | --- | --- | --- |
| 1 GPU | 2 | 1gpu | `csm=gpu0;dof=off` | 29501 | 35512 | 634x611 | off | `ebf1606dd8` |
| 2 GPU: shadows | 2 | 2gpu | `csm=gpu1;dof=off` | 35511 | 35645 | 634x611 | off | `ebf1606dd8` |
| 2 GPU: shadows + DOF | 2 | 2gpu | `csm=gpu1;dof=gpu1` | 7395 | 12829 | 634x611 | off | `ebf1606dd8` |
| 1 GPU + DOF | 2 | 1gpu | `csm=gpu0;dof=gpu0` | 22137 | 28603 | 634x611 | off | `ebf1606dd8` |

### Warnings

- 1 GPU: 16.6% of the frames had other features than 'csm=gpu0;dof=off' ({('2gpu', 'csm=gpu1;dof=off'): 5877, ('1gpu', 'csm=gpu0;dof=off'): 29621, ('1gpu', 'csm=off;dof=off'): 7, ('1gpu', 'dof=off'): 7})
- 2 GPU: shadows + DOF: 41.4% of the frames had other features than 'csm=gpu1;dof=gpu1' ({('2gpu', 'csm=gpu1;dof=off'): 5310, ('2gpu', 'csm=gpu1;dof=gpu1'): 7515, ('2gpu', 'csm=off;dof=gpu1'): 3, ('2gpu', 'dof=gpu1'): 1})
- 1 GPU + DOF: 22.2% of the frames had other features than 'csm=gpu0;dof=gpu0' ({('2gpu', 'csm=gpu1;dof=off'): 5262, ('1gpu', 'csm=gpu0;dof=off'): 840, ('2gpu', 'csm=gpu0;dof=gpu1'): 230, ('1gpu', 'csm=gpu0;dof=gpu0'): 22257, ('1gpu', 'csm=off;dof=gpu0'): 11, ('1gpu', 'dof=gpu0'): 3})
- runs differ in features: 1 GPU #1: csm=gpu0;dof=off; 1 GPU #2: csm=gpu0;dof=off; 2 GPU: shadows #1: csm=gpu1;dof=off; 2 GPU: shadows #2: csm=gpu1;dof=off; 2 GPU: shadows + DOF #1: csm=gpu1;dof=gpu1; 2 GPU: shadows + DOF #2: csm=gpu1;dof=gpu1; 1 GPU + DOF #1: csm=gpu0;dof=gpu0; 1 GPU + DOF #2: csm=gpu0;dof=gpu0

### CPU frame time (the comparison)

| Run | Mean, ms | Median, ms | p95, ms | p99, ms | FPS (1000 / mean) | Mean vs baseline | p95 vs baseline |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| 1 GPU | 3.175 | 3.146 | 3.691 | 4.842 | 315.0 | +0.0% | +0.0% |
| 2 GPU: shadows | 3.232 | 3.156 | 3.703 | 3.962 | 309.4 | +1.8% | +0.3% |
| 2 GPU: shadows + DOF | 12.468 | 12.466 | 12.989 | 13.347 | 80.2 | +292.7% | +251.9% |
| 1 GPU + DOF | 3.877 | 3.829 | 4.867 | 5.222 | 257.9 | +22.1% | +31.9% |

### Shadow atlas from the second GPU

A new atlas reaches Nau only in some frames; the others show the previous one. Age: frames between the frame that drew the atlas and the frame that shows it.

| Run | Frames with a new atlas | Age when used | Age on screen: mean | median | p95 | max | FPS vs baseline |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| 2 GPU: shadows | 50.5% | 2: 2%, 3: 98% | 3.48 | 3.0 | 4.0 | 4 | -5.6 |
| 2 GPU: shadows + DOF | 99.9% | 2: 100% | 2.00 | 2.0 | 2.0 | 2 | -234.8 |

### CPU timers (render thread)

| Timer | 1 GPU median / p95, ms | 2 GPU: shadows median / p95, ms | 2 GPU: shadows + DOF median / p95, ms | 1 GPU + DOF median / p95, ms |
| --- | ---: | ---: | ---: | ---: |
| `CSM` | 0.278 / 0.338 | 0.175 / 0.288 | 0.247 / 0.334 | 0.260 / 0.317 |
| `CSM/mgpu` | 0.001 / 0.001 | 0.166 / 0.275 | 0.233 / 0.319 | 0.001 / 0.001 |
| `dof_multi_gpu` | 0.000 / 0.000 | 0.000 / 0.000 | 8.331 / 8.480 | 0.000 / 0.000 |

### GPU passes

Passes with a median below 0.005 ms in every run are left out (--min-ms).

| Pass | GPU | Queue | 1 GPU mean / median / p95, ms | 2 GPU: shadows mean / median / p95, ms | 2 GPU: shadows + DOF mean / median / p95, ms | 1 GPU + DOF mean / median / p95, ms |
| --- | --- | --- | ---: | ---: | ---: | ---: |
| `Frame` | 0 | gfx0 | 0.565 / 0.565 / 0.596 | 0.605 / 0.550 / 0.608 | 8.960 / 8.958 / 9.179 | 1.192 / 1.191 / 1.231 |
| `/CSM_0` | 0 | gfx0 | 0.096 / 0.097 / 0.107 | 0.063 / 0.110 / 0.126 | 0.126 / 0.126 / 0.126 | 0.093 / 0.093 / 0.103 |
| `/debug_gui_0` | 0 | gfx0 | 0.018 / 0.017 / 0.018 | 0.018 / 0.018 / 0.019 | 0.019 / 0.019 / 0.019 | 0.014 / 0.014 / 0.014 |
| `/depth_of_field_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.323 / 0.323 / 0.324 | 0.244 / 0.243 / 0.249 |
| `/dof_depth_prepare_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.025 / 0.026 / 0.027 | 0.022 / 0.021 / 0.022 |
| `/dof_holes_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.017 / 0.017 / 0.017 |
| `/dof_multi_gpu_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 7.906 / 7.902 / 8.136 | 0.000 / 0.000 / 0.001 |
| `/dof_patch_0_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.023 / 0.022 / 0.024 |
| `/dof_patch_1_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.011 / 0.011 / 0.011 |
| `/dof_patch_2_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.007 / 0.007 / 0.007 |
| `/dof_prime_coc_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.022 / 0.022 / 0.024 | 0.018 / 0.017 / 0.018 |
| `/dof_pyramid_1_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.016 / 0.016 / 0.017 |
| `/dof_pyramid_2_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.008 / 0.008 / 0.009 |
| `/dof_second_coc_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.019 / 0.019 / 0.021 |
| `/dof_wfilter_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.122 / 0.122 / 0.125 |
| `/dof_wmax_0` | 0 | gfx0 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.000 / 0.000 / 0.000 | 0.133 / 0.132 / 0.136 |
| `/fill_gbuffer_0` | 0 | gfx0 | 0.201 / 0.200 / 0.227 | 0.180 / 0.178 / 0.218 | 0.220 / 0.212 / 0.295 | 0.205 / 0.203 / 0.229 |
| `/gbuffer_resolve_0` | 0 | gfx0 | 0.079 / 0.077 / 0.095 | 0.081 / 0.079 / 0.096 | 0.085 / 0.082 / 0.100 | 0.070 / 0.069 / 0.085 |
| `/post_fx_nodes_0` | 0 | gfx0 | 0.014 / 0.014 / 0.016 | 0.015 / 0.014 / 0.016 | 0.018 / 0.018 / 0.019 | 0.015 / 0.015 / 0.016 |
| `/render_envi_0` | 0 | gfx0 | 0.005 / 0.005 / 0.006 | 0.001 / 0.001 / 0.002 | 0.001 / 0.001 / 0.002 | 0.004 / 0.004 / 0.005 |
| `/z-prepass_0` | 0 | gfx0 | 0.150 / 0.150 / 0.171 | 0.171 / 0.168 / 0.236 | 0.211 / 0.211 / 0.256 | 0.147 / 0.147 / 0.164 |
| `csm_atlas_import` | 0 | gfx0 |  | 0.123 / 0.124 / 0.125 | 0.125 / 0.125 / 0.125 |  |
| `csm_atlas_draw` | 1 | gfx1.csm |  | 0.347 / 0.342 / 0.369 | 0.361 / 0.359 / 0.391 |  |
| `dof_compute` | 1 | gfx1.dof |  |  | 3.356 / 3.351 / 3.369 |  |

### Transfers between GPUs

`sum` = src + dst (GPU work); `span` = start of src to end of dst on the CPU clock (includes waits). Bandwidth: payload / median.

| Transfer | Half | GPU | Queue | Run | Samples | Mean, ms | Median, ms | p95, ms | Payload | GB/s |
| --- | --- | --- | --- | --- | ---: | ---: | ---: | ---: | ---: | ---: |
| `csm_atlas` | src | 1 | gfx1.csm | 2 GPU: shadows | 17936 | 2.941 | 2.935 | 2.947 | 8.00 MiB | 2.86 |
| `csm_atlas` | src | 1 | gfx1.csm | 2 GPU: shadows + DOF | 7395 | 2.936 | 2.934 | 2.943 | 8.00 MiB | 2.86 |
| `csm_atlas` | dst | 0 | copy0.csm | 2 GPU: shadows | 17934 | 0.701 | 0.660 | 0.822 | 8.00 MiB | 12.72 |
| `csm_atlas` | dst | 0 | copy0.csm | 2 GPU: shadows + DOF | 7391 | 0.800 | 0.808 | 0.910 | 8.00 MiB | 10.38 |
| `csm_atlas` | sum | 1>0 | - | 2 GPU: shadows | 17934 | 3.642 | 3.600 | 3.761 | 8.00 MiB | 2.33 |
| `csm_atlas` | sum | 1>0 | - | 2 GPU: shadows + DOF | 7391 | 3.736 | 3.744 | 3.846 | 8.00 MiB | 2.24 |
| `csm_atlas` | span | 1>0 | - | 2 GPU: shadows | 17934 | 6.476 | 6.345 | 6.746 | 8.00 MiB |  |
| `csm_atlas` | span | 1>0 | - | 2 GPU: shadows + DOF | 7391 | 12.622 | 12.612 | 13.017 | 8.00 MiB |  |
| `dof_in` | src | 0 | gfx0.dof | 2 GPU: shadows + DOF | 7393 | 0.352 | 0.351 | 0.358 | 4.43 MiB | 13.25 |
| `dof_in` | dst | 1 | gfx1.dof | 2 GPU: shadows + DOF | 7395 | 0.288 | 0.284 | 0.314 | 4.43 MiB | 16.37 |
| `dof_in` | sum | 0>1 | - | 2 GPU: shadows + DOF | 7393 | 0.640 | 0.635 | 0.669 | 4.43 MiB | 7.32 |
| `dof_in` | span | 0>1 | - | 2 GPU: shadows + DOF | 7393 | 2.400 | 2.395 | 2.512 | 4.43 MiB |  |
| `dof_out` | src | 1 | gfx1.dof | 2 GPU: shadows + DOF | 7395 | 0.502 | 0.501 | 0.504 | 3.69 MiB | 7.73 |
| `dof_out` | dst | 0 | gfx0.dof | 2 GPU: shadows + DOF | 7393 | 0.305 | 0.305 | 0.308 | 3.69 MiB | 12.71 |
| `dof_out` | sum | 1>0 | - | 2 GPU: shadows + DOF | 7393 | 0.807 | 0.806 | 0.811 | 3.69 MiB | 4.80 |
| `dof_out` | span | 1>0 | - | 2 GPU: shadows + DOF | 7393 | 1.168 | 1.179 | 1.246 | 3.69 MiB |  |
