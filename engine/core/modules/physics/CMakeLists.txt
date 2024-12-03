
set(TargetName Physics)
cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH ModuleRoot)

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

nau_add_module(${TargetName}
  SOURCES ${Sources} ${PublicHeaders}
  FOLDER "${NauEngineFolder}/core/modules"
  EXPORT_AS_LIBRARY
)

target_precompile_headers(${TargetName} PRIVATE src/pch.h)

target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/physics/include>
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Source FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX Headers FILES ${PublicHeaders})

target_compile_definitions(${TargetName} PRIVATE NAU_PHYSICS_BUILD)

## Module API
nau_collect_files(ApiHeaders
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
  MASK "*.h"
)

target_link_libraries(${TargetName} PUBLIC
  CoreScene
  DebugRenderer
)

nau_target_link_modules(${TargetName}
  CoreScene
  DebugRenderer
)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/physics
    FILES_MATCHING 
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

nau_install(${TargetName} core)