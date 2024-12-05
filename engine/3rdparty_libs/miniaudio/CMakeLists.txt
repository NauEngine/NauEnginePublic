cmake_minimum_required(VERSION 3.16)

project(miniaudio LANGUAGES CXX)

set(HEADERS
    miniaudio.h
)

set(TargetName miniaudio)

add_library(${TargetName} INTERFACE)
target_include_directories(${TargetName} INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/3rdPartyLibs/miniaudio>
)

nau_install(${TargetName} 3rdPartyLibs ${HEADERS})