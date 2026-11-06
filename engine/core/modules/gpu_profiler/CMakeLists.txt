# GPU timestamp profiler (see include/nau/gpu_profiler/gpu_profiler.h).
# A plain static library, not a Nau module: Graphics and the Mgpu module both link it, and Mgpu must not see Nau's
# headers. The public header uses only plain C++ types; EASTL is an implementation detail.
set(TargetName GpuProfiler)

nau_collect_files(Sources
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.cpp" "*.h"
)

nau_collect_files(PublicHeaders
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.h"
)

# Timestamps of raw D3D12 queues (the Mgpu module): Windows only.
if(NOT WIN32)
  list(FILTER Sources EXCLUDE REGEX "d3d12_timestamp_source")
  list(FILTER PublicHeaders EXCLUDE REGEX "d3d12_timestamp_source")
endif()

add_library(${TargetName} STATIC ${Sources} ${PublicHeaders})
nau_add_compile_options(${TargetName})

set_target_properties(${TargetName} PROPERTIES FOLDER "${NauEngineFolder}/core/modules")

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/gpu_profiler/include>
)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${TargetName} PRIVATE EASTL)
if(WIN32)
  # Driver versions of the adapters in the run metadata (run_info.cpp).
  target_link_libraries(${TargetName} PRIVATE dxgi)
endif()

# Build metadata of the run JSON: commit, branch and local changes at build time, preset (the build directory name)
# and configuration.
set(buildInfoDir ${CMAKE_CURRENT_BINARY_DIR}/generated)
get_filename_component(presetName ${CMAKE_BINARY_DIR} NAME)
add_custom_target(GpuProfilerBuildInfo
  COMMAND ${CMAKE_COMMAND} -DSOURCE_DIR=${CMAKE_SOURCE_DIR} -DOUTPUT=${buildInfoDir}/gpu_profiler_build_info.h
          -DPRESET=${presetName} -P ${CMAKE_CURRENT_SOURCE_DIR}/build_info.cmake
  BYPRODUCTS ${buildInfoDir}/gpu_profiler_build_info.h
  COMMENT "GPU profiler: build info"
)
set_target_properties(GpuProfilerBuildInfo PROPERTIES FOLDER "${NauEngineFolder}/core/modules")
add_dependencies(${TargetName} GpuProfilerBuildInfo)
target_include_directories(${TargetName} PRIVATE ${buildInfoDir})
target_compile_definitions(${TargetName} PRIVATE NAU_GPU_PROFILER_CONFIG="$<CONFIG>")

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Source FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX Headers FILES ${PublicHeaders})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/gpu_profiler
  FILES_MATCHING PATTERN "*.h"
)

nau_install(${TargetName} core)

# Check of D3D12TimestampSource on every hardware adapter of the machine (needs real GPUs, so it is a console program
# and not a registered test).
if(NAU_CORE_TESTS AND WIN32)
  add_subdirectory(tests/d3d12_check)
endif()
