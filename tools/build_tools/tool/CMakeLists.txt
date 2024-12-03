set(TargetName "BuildTool")

nau_collect_files(Sources
  ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.cpp"
)

nau_collect_files(Headers
  ABSOLUTE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.h"
)

set(Files ${Sources} ${Headers})

if(BUILD_SHARED_LIBS)
  add_library(${TargetName} SHARED ${Files})
  target_compile_definitions(${TargetName} PRIVATE BUILD_TOOL_EXPORT)
else()
  add_library(${TargetName} STATIC ${Files})
  target_compile_definitions(${TargetName} PRIVATE NAU_STATIC_RUNTIME)
endif()

add_library(BuildTool::${TargetName} ALIAS ${TargetName})

set_property(TARGET ${TargetName} PROPERTY
  MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set_target_properties(
  ${TargetName}
  PROPERTIES
  FOLDER "${NauEngineFolder}/tools"
)

target_sources(${TargetName} PRIVATE ${Sources} ${Headers})

target_link_libraries(${TargetName} PRIVATE
  nlohmann_json::nlohmann_json
  AssetTool
  Shared
  NauKernel
  PlatformAppApi
  ProjectTool
  NauFramework
  AssetPackTool
  usd
  CoreAssets
  CoreScene
  stb
  tinyimageformat
  ispc_texcomp
  tinydds
  tinygltf
)

nau_target_link_modules(${TargetName}
  PlatformApp
  CoreAssets
  CoreScene
  CoreAssetFormats
)

nau_add_compile_options(${TargetName})

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/${TargetName}/include> 
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Sources})

set(PUBLIC_HEADERS
  "include/nau/build_tool/build_tool.h"
  "include/nau/build_tool/build_config.h"
)

set_target_properties(${TargetName} PROPERTIES PUBLIC_HEADER "${PUBLIC_HEADERS}")

nau_install(${TargetName} tools ${PUBLIC_HEADERS})