# Console check of D3D12TimestampSource: for every hardware adapter, times work on a direct, a compute and a copy queue
# through the profiler and checks that the results arrive. Exit code: 0 = every supported queue was measured.
set(TargetName GpuProfilerD3D12Check)

add_executable(${TargetName} main.cpp)

# NauKernel provides the EASTL allocator the profiler library is built with.
target_link_libraries(${TargetName} PRIVATE GpuProfiler NauKernel d3d12 dxgi)

nau_add_compile_options(${TargetName})

set_target_properties(${TargetName} PROPERTIES
  FOLDER "${NauEngineFolder}/tests"
)
