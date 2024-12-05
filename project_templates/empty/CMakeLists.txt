# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.20)

project(%%(ProjectName:no_space)%%)


if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

if(NOT DEFINED NauEngineFolder)
  file(TO_CMAKE_PATH "$ENV{NAU_ENGINE_SDK_DIR}" ENV_NAU_ENGINE_SDK_DIR)
  set(NauEngineFolder "${ENV_NAU_ENGINE_SDK_DIR}")
endif()
cmake_path(SET NauEngineFolder NORMALIZE "${NauEngineFolder}")

message(NOTICE "SDK folder ${NauEngineFolder}")

if(NOT EXISTS ${NauEngineFolder}/cmake
  OR NOT EXISTS ${NauEngineFolder}/lib
  OR NOT EXISTS ${NauEngineFolder}/include
)
  message(FATAL_ERROR "SDK folder ${NauEngineFolder} does not contain required files!")
endif()

set(CMAKE_MODULE_PATH ${NauEngineFolder})


# Compiler config
set(CMAKE_CXX_STANDARD 20)
# TODO: define C standard

add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)


# Global config
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

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib/$<CONFIG>)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin/$<CONFIG>)


# Nau options
option(NAU_RTTI "Enable rtti support" OFF)
option(NAU_EXCEPTIONS "Enable exception support" OFF)
option(NAU_VERBOSE_LOG "Enable verbose messages for logger" OFF)
option(NAU_MATH_USE_DOUBLE_PRECISION "Enable double precision for math" OFF)

option(BUILD_SHARED_LIBS "Build shared libs" ON)


# Process options
if(BUILD_SHARED_LIBS)
  message(STATUS "Configure for dynamic (DLL) runtime")
else()
  message(STATUS "Configure for static (monolith) runtime")
endif()


include(NauEngineSDK)
include(NauCommon)
include(NauModule)
include(NauGenFunctions)
include(platformSpec/NauPlatformSetup)


include(source/game/%%(ProjectName:no_space)%%.cmake)

file(GLOB Sources
  source/main.cpp
)

set(TargetName %%(ProjectName:no_space)%%Main)

add_executable(${TargetName} ${Sources})

target_precompile_headers(${TargetName} PRIVATE source/pch.h)

target_link_libraries(${TargetName} PRIVATE
  MainGameModule
  PlatformAppApi
  Animation
  CoreInput
  NauFramework
  imgui
  Physics
  Graphics
  
  jsoncpp
)

if(CMAKE_GENERATOR MATCHES "Visual Studio")
  set(CURRENT_CONFIG $<CONFIG>)
  set(ENGINE_BIN_PATH "PATH=${NAU_BUILD_DIR}/bin/${CMAKE_BUILD_TYPE}/${CURRENT_CONFIG};%PATH%")
  set_target_properties(${TargetName} PROPERTIES VS_DEBUGGER_ENVIRONMENT "${ENGINE_BIN_PATH}")
  set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${TargetName})
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Sources})

nau_add_compile_options(${TargetName})

nau_target_link_modules(${TargetName}
  MainGameModule
  PlatformApp
  Animation
  Audio
  CoreScene
  CoreAssets
  GraphicsAssets
  DebugRenderer
  CoreAssetFormats
  CoreInput
  Graphics
  Physics
  PhysicsJolt
)

add_custom_command(TARGET ${TargetName} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${NauEngineFolder}/bin/$<CONFIG>"
  $<TARGET_FILE_DIR:${TargetName}>
)

install(TARGETS ${TargetName}
  DESTINATION bin
)