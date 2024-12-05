cmake_minimum_required(VERSION 3.6)

set(lib_name edtaa3func)
set(target_name ext_${lib_name})

project(${lib_name})

add_library(${target_name} STATIC
  edtaa3func.cpp
)

target_include_directories(${target_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/ui/cocos2d-x/external/edtaa3func>
)

set_target_properties(${target_name}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    FOLDER "${CocosExternalsFolder}"
)

nau_add_compile_options(${target_name})
