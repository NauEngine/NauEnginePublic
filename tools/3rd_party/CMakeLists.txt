# CMakeList.txt : CMake project for project_tool, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

set(CMAKE_CXX_STANDARD 20)

option(JSON_Install "" OFF)

add_subdirectory(json)
add_subdirectory(argparse)

nau_collect_files(nlohmann_json_Headers
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/json/include/nlohmann
    MASK "*.h" "*.hpp"
)
nau_install(nlohmann_json tools ${nlohmann_json_Headers})