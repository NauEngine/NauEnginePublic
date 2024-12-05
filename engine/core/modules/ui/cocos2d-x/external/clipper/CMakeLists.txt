cmake_minimum_required(VERSION 3.6)

set(lib_name clipper)
set(target_name ext_${lib_name})

project(${lib_name})

add_library(${target_name} STATIC
  clipper.cpp
)

target_include_directories(${target_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/ui/cocos2d-x/external/clipper>
)

set_target_properties(${target_name}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    FOLDER "${CocosExternalsFolder}"
)

nau_add_compile_options(${target_name})
