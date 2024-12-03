set(TargetName SceneBaseSample)

nau_collect_files(Sources
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    MASK "*.cpp" "*.h"
)

add_executable(${TargetName}
    ${Sources}
)

target_link_libraries(${TargetName} PRIVATE
    NauFramework
    SampleCommonLib
    Animation
    Physics
    Graphics
    VFX
)

target_precompile_headers(${TargetName} PRIVATE pch.h)
target_include_directories(${TargetName} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/content/shaders/include)

if(NOT NauSDKLibraries)
  file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/content/shaders DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/resources)

  nau_add_default_shaders(${TargetName} content shaders
    INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/core/modules/render/include/nau/shaders
  )
  nau_add_default_shaders(${TargetName} resources shaders
    INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}/../../engine/core/modules/render/include/nau/shaders
  )
endif()

nau_collect_files(SourcesMaterials
    DIRECTORIES content/materials
    MASK  "*.json"
    EXCLUDE
    "/platform/.*"
)
source_group(shaders/materials FILES ${SourcesMaterials})
target_sources(${TargetName} PRIVATE ${SourcesMaterials})

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
    VFX
)

if(NauSDKLibraries)
  target_link_directories(inputDemo PUBLIC ${NauSDKLibraries})
endif()

nau_add_compile_options(${TargetName})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Sources})

set_target_properties(${TargetName} PROPERTIES
    FOLDER "${NauEngineFolder}/samples"
)
