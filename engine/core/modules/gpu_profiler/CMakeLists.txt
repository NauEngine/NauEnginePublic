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

add_library(${TargetName} STATIC ${Sources} ${PublicHeaders})
nau_add_compile_options(${TargetName})

set_target_properties(${TargetName} PROPERTIES FOLDER "${NauEngineFolder}/core/modules")

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/gpu_profiler/include>
)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${TargetName} PRIVATE EASTL)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Source FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX Headers FILES ${PublicHeaders})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
  DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/gpu_profiler
  FILES_MATCHING PATTERN "*.h"
)

nau_install(${TargetName} core)
