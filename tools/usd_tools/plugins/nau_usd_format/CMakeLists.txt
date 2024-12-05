set(TargetName NauUsdFormat)

set(Headers
    nau_api.h
    nau_usd_file_format.h
    nau_scene_file_format.h
    nau_uid_resolver.h
    nau_file_resolver.h
    nau_asset_resolver.h
)

set(Sources 
    nau_usd_file_format.cpp
    nau_scene_file_format.cpp
    nau_uid_resolver.cpp
    nau_file_resolver.cpp
    nau_asset_resolver.cpp
    plugInfo.json
)

add_library(${TargetName} SHARED ${Sources} ${Headers})


set(PLUG_INFO_ROOT "..")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_LIBRARY_PATH "../${TargetName}.dll")


cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH SOURCE_BASE_DIR)
target_compile_definitions(${TargetName} PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0> )
target_link_options(${TargetName} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(${TargetName} PRIVATE USD_FORMAT_EXPORT)
target_include_directories(${TargetName} PUBLIC
    $<BUILD_INTERFACE:${SOURCE_BASE_DIR}>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/plugins> 
)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug/plugins)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release/plugins)

target_link_libraries(${TargetName}
    usd
    AssetTool
    CoreAssets
)

add_dependencies(${TargetName}
    usd-plugins
)
 
set(CONFIG_PLUGIN_PATH "$<$<CONFIG:DEBUG>:${CMAKE_BINARY_DIR}/bin/Debug>$<$<NOT:$<CONFIG:DEBUG>>:${CMAKE_BINARY_DIR}/bin/Release>")

add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json" "${CONFIG_PLUGIN_PATH}/plugins/${TargetName}/resources/plugInfo.json"
)

nau_install(${TargetName} tools/plugins ${Headers})