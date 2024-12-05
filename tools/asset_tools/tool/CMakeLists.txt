set(TargetName AssetTool)

nau_collect_files(Sources
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
    MASK "*.cpp"
)

nau_collect_files(Headers
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
    MASK "*.h"
)

set(Files ${Sources} ${Headers})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Files})

if(BUILD_SHARED_LIBS)
    add_library(${TargetName} SHARED ${Files})
    target_compile_definitions(${TargetName} PRIVATE ASSET_TOOL_EXPORT)
else()
    add_library(${TargetName} STATIC ${Files})
    target_compile_definitions(${TargetName} PRIVATE NAU_STATIC_RUNTIME)
endif()

add_library(AssetTool::${TargetName} ALIAS ${TargetName})

set_target_properties(${TargetName}
    PROPERTIES
        FOLDER "${NauEngineFolder}/tools"
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)

target_link_libraries(${TargetName} 
    PRIVATE
        Animation
        NauKernel
        PlatformAppApi
        NauAnimationClipAsset
        NauFramework
        CoreAssets
        CoreScene
        stb
        tinyimageformat
        ispc_texcomp
        tinydds
        tinygltf
        ozz_animation_tools
        usdSkel
    PUBLIC
        nlohmann_json::nlohmann_json
        UsdMetaTools
)

nau_target_link_modules(${TargetName}
    Animation
    PlatformApp
    CoreAssets
    CoreScene
    CoreAssetFormats
)

add_dependencies(${TargetName} 
        NauAssetMetadata
)

nau_add_compile_options(${TargetName})

target_include_directories(${TargetName} PUBLIC   
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/${TargetName}/include>
)

nau_install(${TargetName} tools ${Headers})