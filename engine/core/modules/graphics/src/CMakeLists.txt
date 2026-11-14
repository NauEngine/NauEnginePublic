
set(TargetName Graphics)
#set(ApiTargetName ${TargetName}Api)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH moduleRoot)

nau_collect_files(Sources
    RELATIVE ${moduleRoot}/src
    DIRECTORIES ${moduleRoot}/src
    MASK "*.cpp" "*.h" "*.hlsli"
    EXCLUDE
    "/platform/.*"
)

nau_collect_files(Headers
    RELATIVE ${moduleRoot}
    DIRECTORIES ${moduleRoot}/include
    MASK "*.h"
    PREPEND "../"
)

# if (${Platform_Windows})
#   nau_collect_files(SOURCES
#     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
#     DIRECTORIES
#       ${CMAKE_CURRENT_SOURCE_DIR}/include/runtime/platform/windows
#       ${CMAKE_CURRENT_SOURCE_DIR}/platform/windows
#     MASK "*.cpp" "*.h"
#   )
# endif()

nau_add_module(${TargetName}
    SOURCES ${Sources} ${Headers}
    FOLDER "${NauEngineFolder}/core/modules"
    EXPORT_AS_LIBRARY
    #  INTERFACE_TARGET ${ApiTargetName}
)

target_precompile_headers(${TargetName} PRIVATE pch.h)
target_include_directories(${TargetName} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/internal
)

target_include_directories(${TargetName} PUBLIC
    $<BUILD_INTERFACE:${moduleRoot}/include>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/graphics/include>
)

target_link_libraries(${TargetName} PRIVATE
    Render
    PlatformAppApi
    CoreScene
    CoreInput
    NauKernel
    Animation
    D3DCompiler.lib
    imgui
    gainput
    DebugRenderer
    ui
    VFX
    # GPU timestamp profiler (engine/core/modules/gpu_profiler); its header has only plain C++ types, so the Mgpu
    # module links the same library.
    GpuProfiler
)

# Optional Mgpu transfer bridge (root option NAU_ENABLE_MGPU, switch: MGPU_TRANSFER_ENABLED in render_window/mgpu_nodes.cpp).
# Graphics may only include nau/mgpu/bridge/mgpu_bridge.h: the other Mgpu headers leak global using-directives and
# macros (PI). For the same reason Mgpu is linked LINK_ONLY, so its usage requirements (UNICODE, include directories,
# definitions) are not inherited; the include directory and NAU_ENABLE_MGPU are added by hand below.
if(NAU_ENABLE_MGPU)
    target_link_libraries(${TargetName} PRIVATE $<LINK_ONLY:Mgpu>)
    target_include_directories(${TargetName} PRIVATE ${moduleRoot}/../mgpu/include)
    target_compile_definitions(${TargetName} PRIVATE NAU_ENABLE_MGPU=1)

    # Mgpu links WinPixEventRuntime (import library of a DLL), so the DLL has to sit next to every executable that
    # links Graphics: it is copied into the common runtime output directory of the engine.
    get_property(isMultiConfigGenerator GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
    if(isMultiConfigGenerator)
        set(mgpuRuntimeDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/$<CONFIG>)
    else()
        set(mgpuRuntimeDir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
    endif()
    add_custom_command(TARGET ${TargetName} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory ${mgpuRuntimeDir}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:WinPixEventRuntime> ${mgpuRuntimeDir})
endif()

target_link_libraries(${TargetName} PUBLIC
    Render
    CoreInput
    GraphicsAssets
    CoreAssets
)

target_compile_definitions(${TargetName} PRIVATE
    NAU_GRAPHICS_BUILD=1
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} PREFIX Sources FILES ${Sources})
source_group(TREE ${moduleRoot}/include PREFIX Headers FILES ${Headers})

## Module API
# nau_collect_files(ApiHeaders
#   RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
#   DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
#   MASK "*.h"
# )

# target_sources(${ApiTargetName} INTERFACE ${ApiHeaders})
# target_include_directories(${ApiTargetName} INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# set_target_properties (${ApiTargetName} PROPERTIES
#     FOLDER "core/modules"
# )

install(DIRECTORY ${moduleRoot}/include
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/graphics
    FILES_MATCHING 
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

nau_install(${TargetName} core)