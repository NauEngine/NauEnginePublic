cmake_minimum_required(VERSION 3.6)

set(lib_name tinyxml2)
set(target_name ext_${lib_name})

project(${lib_name})

set(${target_name}_src
  tinyxml2.cpp
)

add_library(${target_name} STATIC
  ${${target_name}_src}
)

target_include_directories(${target_name} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/ui/cocos2d-x/external/tinyxml2>
)

include(../cmake/CocosExternalConfig.cmake)
if(WINDOWS)
  if(BUILD_SHARED_LIBS)
    target_compile_definitions(${target_name} PRIVATE _USRDLL)
  else()
    target_compile_definitions(${target_name} PRIVATE CC_STATIC)
  endif()
endif()

set_target_properties(${target_name}
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    FOLDER "${CocosExternalsFolder}"
)

nau_add_compile_options(${target_name})

