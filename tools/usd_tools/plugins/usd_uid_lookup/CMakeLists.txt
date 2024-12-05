set(TargetName UsdUidLookup)

set(Headers
    usd_uid_lookup.h
    usd_uid_lookup_api.h
)

set(Sources
    usd_uid_lookup.cpp
)

add_library(${TargetName} SHARED ${Sources} ${Headers})

set(PLUG_INFO_ROOT "..")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_LIBRARY_PATH "../${TargetName}.dll")

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH SOURCE_BASE_DIR)
target_compile_definitions(${TargetName} PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0>)
target_link_options(${TargetName} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(${TargetName} PRIVATE USD_UID_LOOKUP_EXPORT)
target_include_directories(${TargetName} PUBLIC
    $<BUILD_INTERFACE:${SOURCE_BASE_DIR}>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/plugins> 
)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug/plugins)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release/plugins)

target_link_libraries(${TargetName}
    usd
    NauKernel
    PlatformAppApi
    NauFramework
    CoreAssets
)

add_dependencies(${TargetName}
    usd-plugins
    PlatformApp
    CoreAssets
    CoreScene
    CoreAssetFormats
)

set(CONFIG_PLUGIN_PATH "$<$<CONFIG:DEBUG>:${CMAKE_BINARY_DIR}/bin/Debug>$<$<NOT:$<CONFIG:DEBUG>>:${CMAKE_BINARY_DIR}/bin/Release>")

add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_SOURCE_DIR}/plugInfo.json" "${CONFIG_PLUGIN_PATH}/plugins/${TargetName}/resources/plugInfo.json"
)

if (NAU_CORE_TESTS)
    #add_subdirectory(tests)
endif()

nau_install(${TargetName} tools/plugins ${Headers})