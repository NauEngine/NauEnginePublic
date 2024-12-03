set(TargetName "NauComponentSchema")

set(PLUG_INFO_LIBRARY_PATH "../${TargetName}.dll")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_ROOT "..")

set(Schema nau_component_schema.usd)

add_library(${TargetName} SHARED ${Schema})

nau_process_usd_schema(${TargetName} ${Schema} GenFiles)

target_sources(${TargetName} PRIVATE ${Schema} ${GenFiles})



cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH SOURCE_BASE_DIR)
target_compile_definitions(${TargetName} PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0>)
target_link_options(${TargetName} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(${TargetName} PRIVATE NAUCOMPONENTLIB_EXPORTS)
target_include_directories(${TargetName} PUBLIC
    $<BUILD_INTERFACE:${SOURCE_BASE_DIR}>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/plugins> 
)

set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug/plugins)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release/plugins)

target_link_libraries(${TargetName}
    usd
    usdGeom
    usdUtils
)

add_dependencies(${TargetName}
    usd-plugins
)


set(CONFIG_PLUGIN_PATH "$<$<CONFIG:DEBUG>:${CMAKE_BINARY_DIR}/bin/Debug>$<$<NOT:$<CONFIG:DEBUG>>:${CMAKE_BINARY_DIR}/bin/Release>")

add_custom_command(TARGET ${TargetName} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/generated_src/schema_plugins/nau/${TargetName}/plugInfo.json" "${CONFIG_PLUGIN_PATH}/plugins/${TargetName}/resources/plugInfo.json"
    COMMAND ${CMAKE_COMMAND} -E copy "${CMAKE_CURRENT_BINARY_DIR}/generated_src/schema_plugins/nau/${TargetName}/generatedSchema.usda" "${CONFIG_PLUGIN_PATH}/plugins/${TargetName}/resources/generatedSchema.usda"
)

nau_collect_files(GenFilesHeaders
    DIRECTORIES ${CMAKE_CURRENT_BINARY_DIR}/generated_src/schema_plugins/nau/${TargetName}
    ABSOLUTE ${CMAKE_CURRENT_BINARY_DIR}/generated_src/schema_plugins/nau/${TargetName}
    MASK "*.h"
)

nau_install(${TargetName} tools/plugins)
install(FILES ${GenFilesHeaders} DESTINATION "include/generated_src/schema_plugins/nau/${TargetName}")