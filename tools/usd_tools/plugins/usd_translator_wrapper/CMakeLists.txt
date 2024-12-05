set(TargetName UsdTranslatorWrapper)

set(Headers 
    usd_translator_wrapper.h
    usd_translator_wrapper_api.h
)

set(Sources 
    usd_translator_wrapper.cpp
)

add_library(${TargetName} SHARED ${Sources} ${Headers})


set(PLUG_INFO_ROOT "..")
set(PLUG_INFO_RESOURCE_PATH "resources")
set(PLUG_INFO_LIBRARY_PATH "../${TargetName}.dll")


cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH SOURCE_BASE_DIR)
target_compile_definitions(${TargetName} PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0> )
target_link_options(${TargetName} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(${TargetName} PRIVATE USD_TRANSLATOR_WRAPPER_EXPORT)
target_include_directories(${TargetName} PUBLIC
    $<BUILD_INTERFACE:${SOURCE_BASE_DIR}>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/plugins> 
)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/Debug)
set_target_properties(${TargetName} PROPERTIES RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/Release)
target_compile_options(${TargetName} PRIVATE /Zc:preprocessor)
target_link_libraries(${TargetName}
    usd
    UsdTranslator
)

add_dependencies(${TargetName}
    usd-plugins
)

nau_install(${TargetName} tools/plugins ${Headers})