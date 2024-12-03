# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.20)

project(NauEngine)


# TODO: set enviroment variables for other platforms
if (WIN32)
  execute_process(COMMAND "${CMAKE_SOURCE_DIR}/cmake/platformSpec/set_env.bat")
endif()

if (POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW) 
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
#  ${CMAKE_SOURCE_DIR}/cmake/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/defaults
)


# Compiler config
set(CMAKE_CXX_STANDARD 20)
# TODO: define C standard

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()


# Required packages
find_package(Python REQUIRED)


# Global config
if (NOT DEFINED NauEngineFolder)
  set(NauEngineFolder "NauEngine")
endif()

if (CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/output CACHE PATH "" FORCE)
  message(STATUS "Use default install folder: ${CMAKE_INSTALL_PREFIX}")
else()
  message(STATUS "Defined install folder: ${CMAKE_INSTALL_PREFIX}")
endif()

if (DEFINED CMAKE_BUILD_TYPE)
  message(STATUS "Build configuration: ${CMAKE_BUILD_TYPE}")
else()
  if (DEFINED CMAKE_CONFIGURATION_TYPES)
    message(STATUS "Use multi-configuration. Allowed configurations at build time: ${CMAKE_CONFIGURATION_TYPES}")
  else()
    set(CMAKE_BUILD_TYPE "Debug")
    message(STATUS "Use default build configuration: ${CMAKE_BUILD_TYPE}")
  endif()
endif()

if (NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  message(FATAL_ERROR "Toolchain not selected! Please define -DCMAKE_TOOLCHAIN_FILE")
else()
  message(STATUS "Toolchain file: ${CMAKE_TOOLCHAIN_FILE}")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>)


# Nau options
option(NAU_ENGINE_KERNEL "Build kernel" ON)
option(NAU_ENGINE_MODULES "Build modules" ON)
option(NAU_ENGINE_FRAMEWORK "Build framework" ON)
option(NAU_CORE_TOOLS "Build core tools projects" ON)
option(NAU_CORE_SAMPLES "Build core samples projects" ON)
option(NAU_CORE_TESTS "Build core tests projects" ON)
option(NAU_RTTI "Enable rtti support" OFF)
option(NAU_EXCEPTIONS "Enable exception support" OFF)
option(NAU_VERBOSE_LOG "Enable verbose messages for logger" OFF)
option(NAU_FORCE_ENABLE_SHADER_COMPILER_TOOL "Enable build for ShaderCompilerTool even if NAU_CORE_TOOLS is OFF" OFF)
option(NAU_PACKAGE_BUILD "Enabled for packaged build" OFF)
option(NAU_MATH_USE_DOUBLE_PRECISION "Enable double precision for math" OFF)

# Global options
option(BUILD_SHARED_LIBS "Build shared libs" ON)


# Process options
if (BUILD_SHARED_LIBS)
  message(STATUS "Configure for dynamic (DLL) runtime")
else()
  message(STATUS "Configure for static (monolith) runtime")
endif()

if (NAU_PACKAGE_BUILD)
  add_definitions(-DNAU_PACKAGE_BUILD)
endif()


include(NauCommon)
include(NauModule)
include(NauGenFunctions)
include(platformSpec/NauPlatformSetup)
if (BUILD_SHARED_LIBS)
  message(STATUS "Adding UsdPackages")
  include(UsdPackages)
else()
  message(STATUS "Skipping UsdPackages")
endif()
include(CTest)
include(cmake/NauGitVersion.cmake)

nau_check_python()
git_check_version()

file (STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/.nauversion NAU_VERSION_STRING)
nau_set_version("${NAU_VERSION_STRING}")
message(STATUS "NAU core version: ${NAU_VERSION_MAJOR}.${NAU_VERSION_MINOR}.${NAU_VERSION_PATCH}")

enable_testing()

configure_file("run_tests.bat.in" "${CMAKE_SOURCE_DIR}/run_tests.bat")

add_subdirectory(engine)
add_subdirectory(tools)
add_subdirectory(samples)

nau_generate_es_targets()


# Install
foreach(_folder cmake/defaults cmake/platformSpec)
  install(DIRECTORY ${_folder}
  DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake
  FILES_MATCHING 
    PATTERN "*.cmake"
    PATTERN "*.bat"
  )
endforeach()
install(FILES
  cmake/NauCommon.cmake
  cmake/NauGenFunctions.cmake
  cmake/NauGitVersion.cmake
  cmake/NauModule.cmake
  DESTINATION ${CMAKE_INSTALL_PREFIX}/cmake
)

install(DIRECTORY cmake/for_build/
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)
install(DIRECTORY project_templates
  DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(DIRECTORY DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/$<CONFIG>/plugins)
install(DIRECTORY ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins/
  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/$<CONFIG>/plugins
  OPTIONAL
)
