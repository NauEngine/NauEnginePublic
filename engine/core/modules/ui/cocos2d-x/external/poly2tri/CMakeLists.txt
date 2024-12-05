cmake_minimum_required(VERSION 3.6)

set(lib_name poly2tri)
set(target_name ext_${lib_name})

project(${lib_name})

set(${target_name}_src
  common/shapes.cc
  sweep/sweep.cc
  sweep/sweep_context.cc
  sweep/cdt.cc
  sweep/advancing_front.cc
)

add_library(${target_name} STATIC
  ${${target_name}_src}
)

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH PARENT_DIR)
target_include_directories(${target_name} PUBLIC
    $<BUILD_INTERFACE:${PARENT_DIR}>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/ui/cocos2d-x/external>
)

set_target_properties(${target_name}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    FOLDER "${CocosExternalsFolder}"
)

nau_add_compile_options(${target_name})
