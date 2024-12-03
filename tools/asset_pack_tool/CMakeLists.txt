set(TargetName AssetPackTool)

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
  FOLDER "${NauEngineFolder}/tools"
  EXPORT_AS_LIBRARY
)

target_compile_definitions(${TargetName} PRIVATE NAU_ASSETPACKTOOL_BUILD)

target_precompile_headers(${TargetName} PRIVATE src/pch.h)

target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/${TargetName}/include> 
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Source FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX Headers FILES ${PublicHeaders})

nau_install(${TargetName} tools ${PublicHeaders})