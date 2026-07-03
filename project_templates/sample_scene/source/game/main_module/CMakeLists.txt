set(TargetName MainGameModule)

nau_collect_files(Sources
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.cpp" "*.h"
)

nau_add_module(${TargetName}
  SOURCES ${Sources}
)

target_precompile_headers(${TargetName} PRIVATE pch.h)

target_link_libraries(${TargetName} PUBLIC 
    NauFramework 
    CoreInput
    Animation
    Physics
    Graphics
)

nau_target_link_modules(${TargetName}
    PlatformApp
    Animation
    CoreScene
    CoreAssets
    GraphicsAssets
    DebugRenderer
    CoreAssetFormats
    CoreInput
    Graphics
    Physics
    PhysicsJolt
)

target_include_directories(${TargetName} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(${TargetName} PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/public)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Sources})

if (TARGET NauKernel_Import AND TARGET NauFramework_Import)
  add_dependencies(${TargetName} NauKernel_Import NauFramework_Import)
endif()

install(TARGETS ${TargetName}
  DESTINATION bin
)