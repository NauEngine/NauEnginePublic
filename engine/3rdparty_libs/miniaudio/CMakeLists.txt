cmake_minimum_required(VERSION 3.16)

project(miniaudio LANGUAGES CXX)

set(TargetName miniaudio)

add_library(${TargetName} INTERFACE)
target_include_directories(${TargetName} INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/)
