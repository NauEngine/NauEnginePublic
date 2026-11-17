# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required (VERSION 3.20)

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)

project(NauEngine)

# TODO: set enviroment variables for other platforms
if (WIN32)
  execute_process( COMMAND "${CMAKE_SOURCE_DIR}/cmake/platformSpec/set_env.bat")
endif(WIN32)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW) 
endif()

if(NOT DEFINED ${NauEngineFolder})
  set(NauEngineFolder "NauEngine")
endif(NOT DEFINED ${NauEngineFolder})

set(CMAKE_INSTALL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/output CACHE PATH "" FORCE)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
#  ${CMAKE_SOURCE_DIR}/cmake/modules
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake
  ${CMAKE_CURRENT_SOURCE_DIR}/cmake/defaults
)

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
option(NAU_ENABLE_LTO "Enable link-time optimization for engine targets (Release, MSVC only)" OFF)

option(BUILD_SHARED_LIBS "Build shared libs" ON)

if (BUILD_SHARED_LIBS)
  message (STATUS "Configure for dynamic (DLL) runtime")
else()
  message (STATUS "Configure for static (monolith) runtime")
endif()

if (NAU_PACKAGE_BUILD)
  add_definitions(-DNAU_PACKAGE_BUILD)
endif()

set(CMAKE_CXX_STANDARD 20)

find_package(Python
    REQUIRED
)


include(NauCommon)
include(NauModule)
include(NauGenFunctions)
include(platformSpec/NauPlatformSetup)
include(NauLto)
if(BUILD_SHARED_LIBS)
  message (STATUS "Adding UsdPackages")
  include(UsdPackages)
else()
  message (STATUS "Skipping UsdPackages")
endif()
include(CTest)
nau_check_python()

enable_testing()


# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(cmake/NauGitVersion.cmake)
git_check_version()

file (STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/.nauversion NAU_VERSION_STRING)
nau_set_version("${NAU_VERSION_STRING}")

add_subdirectory(engine)
add_subdirectory(tools)
add_subdirectory(samples)

nau_generate_es_targets()

configure_file("run_tests.bat.in" "${CMAKE_SOURCE_DIR}/run_tests.bat")

install(DIRECTORY cmake
    DESTINATION ${CMAKE_INSTALL_PREFIX}
    FILES_MATCHING 
      PATTERN "*.cmake"
      PATTERN "*.bat"
)

install(DIRECTORY cmake/for_build/
    DESTINATION ${CMAKE_INSTALL_PREFIX}
)

install(CODE "
  message(STATUS \"Installing USD plugins from ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins to ${CMAKE_INSTALL_PREFIX}/bin/$<CONFIG>/plugins\")
  execute_process(COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_BINARY_DIR}/bin/$<CONFIG>/plugins ${CMAKE_INSTALL_PREFIX}/bin/$<CONFIG>/plugins)
")

