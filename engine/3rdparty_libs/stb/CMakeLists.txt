cmake_minimum_required(VERSION 3.16)

project(stb LANGUAGES CXX)

set(TargetName stb)

nau_collect_files(HEADERS
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.h" "*.hpp"
)

add_library(${TargetName} 
  INTERFACE
  ${HEADERS}
)

target_include_directories(${TargetName} INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/3rdPartyLibs/stb>
)

nau_install(${TargetName} 3rdPartyLibs ${HEADERS})