# CMakeList.txt : Top-level CMake project file, do global configuration
# and include sub-projects here.
#
cmake_minimum_required(VERSION 3.20)

project(%%(ProjectName:no_space)%%)

# TODO: set enviroment variables for other platforms
if(WIN32)
  execute_process(COMMAND "${CMAKE_SOURCE_DIR}/cmake/platformSpec/set_env.bat")
endif(WIN32)

if(POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW)
endif()

if(NOT DEFINED ${NauEngineFolder})
  file(TO_CMAKE_PATH "$ENV{NAU_ENGINE_SDK_DIR}" ENV_NAU_ENGINE_SDK_DIR)
  set(NauEngineFolder "${ENV_NAU_ENGINE_SDK_DIR}")
endif(NOT DEFINED ${NauEngineFolder})

message(NOTICE "SDK folder ${NauEngineFolder}")

if(NOT EXISTS ${NauEngineFolder}/CMakeLists.txt
  OR NOT EXISTS ${NauEngineFolder}/cmake
  OR NOT EXISTS ${NauEngineFolder}/lib
  OR NOT EXISTS ${NauEngineFolder}/include
)
  message(FATAL_ERROR "SDK folder ${NauEngineFolder} does not contain required files!")
endif()

set(CMAKE_MODULE_PATH ${NauEngineFolder}
  ${NauEngineFolder}/cmake
  ${NauEngineFolder}/cmake/defaults
)

option(NAU_RTTI "Enable rtti support" OFF)
option(NAU_EXCEPTIONS "Enable exception support" OFF)
option(NAU_VERBOSE_LOG "Enable verbose messages for logger" OFF)
option(NAU_MATH_USE_DOUBLE_PRECISION "Enable double precision for math" OFF)

option(BUILD_SHARED_LIBS "Build shared libs" ON)

if(BUILD_SHARED_LIBS)
  message(STATUS "Configure for dynamic (DLL) runtime")
else()
  message(STATUS "Configure for static (monolith) runtime")
endif()

set(NauSDKIncludes ${NauEngineFolder}/include)
set(NauSDKLibraries ${NauEngineFolder}/lib)

set(CMAKE_CXX_STANDARD 20)

find_package(Python
  REQUIRED
)

include(NauCommon)
include(NauModule)
include(NauGenFunctions)
include(platformSpec/NauPlatformSetup)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)

# Enable Hot Reload for MSVC compilers if supported.
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

find_package(Threads REQUIRED)

function(try_inc cmakeName)
  if(EXISTS ${NauEngineFolder}/${cmakeName})
    include(${NauEngineFolder}/${cmakeName})
    message(STATUS "${NauEngineFolder}/${cmakeName}")
  endif()
endfunction()

function(try_add subdir)
  if(EXISTS ${NauEngineFolder}/${subdir})
    add_subdirectory(${NauEngineFolder}/${subdir})
    message(STATUS "${NauEngineFolder}/${subdir}")
  endif()
endfunction()

try_inc(cmake/3rdPartyLibs/asio/asio-config.cmake)
try_inc(cmake/3rdPartyLibs/brotli/brotli-config.cmake)
try_inc(cmake/3rdPartyLibs/EABase/EABase-config.cmake)
try_inc(cmake/3rdPartyLibs/EASTL/EASTL-config.cmake)
try_inc(cmake/3rdPartyLibs/fast_float/fast_float-config.cmake)
try_inc(cmake/3rdPartyLibs/jsoncpp/jsoncpp-config.cmake)
try_inc(cmake/3rdPartyLibs/lzma-9.20/lzma-9.20-config.cmake)
try_inc(cmake/3rdPartyLibs/md5/md5-config.cmake)
try_inc(cmake/3rdPartyLibs/vectormath/vectormath-config.cmake)
try_inc(cmake/3rdPartyLibs/wyhash/wyhash-config.cmake)
try_inc(cmake/3rdPartyLibs/zlib-ng/zlib-ng-config.cmake)
try_inc(cmake/3rdPartyLibs/zstd-1.4.5/zstd-1.4.5-config.cmake)
try_inc(cmake/3rdPartyLibs/fmt/fmt-config.cmake)
try_inc(cmake/3rdPartyLibs/tinyutf8/tinyutf8-config.cmake)
try_inc(cmake/3rdPartyLibs/utf8cpp/utf8cpp-config.cmake)
try_inc(cmake/3rdPartyLibs/tinyimageformat/tinyimageformat-config.cmake)
try_inc(cmake/3rdPartyLibs/gainputstatic/gainputstatic-config.cmake)
try_inc(cmake/3rdPartyLibs/imgui/imgui-config.cmake)
try_inc(cmake/3rdPartyLibs/RenderDoc/RenderDoc-config.cmake)
try_inc(cmake/3rdPartyLibs/adlSdk/adlSdk-config.cmake)
try_inc(cmake/3rdPartyLibs/dxc/dxc-config.cmake)
try_inc(cmake/3rdPartyLibs/ska_hash_map/ska_hash_map-config.cmake)
try_inc(cmake/3rdPartyLibs/xxHash/xxHash-config.cmake)
try_inc(cmake/3rdPartyLibs/ozz_base/ozz_base-config.cmake)
try_inc(cmake/3rdPartyLibs/ozz_animation/ozz_animation-config.cmake)
try_inc(cmake/3rdPartyLibs/stb/stb-config.cmake)
try_inc(cmake/3rdPartyLibs/ispc_texcomp/ispc_texcomp-config.cmake)
try_inc(cmake/3rdPartyLibs/tinydds/tinydds-config.cmake)
try_inc(cmake/3rdPartyLibs/Jolt/Jolt-config.cmake)
try_inc(cmake/3rdPartyLibs/uriparser/uriparser-config.cmake)

## try_inc(share/Tracy/TracyConfig.cmake)

try_inc(cmake/core/NauKernel/NauKernel-config.cmake)

try_inc(cmake/3rdPartyLibs/binPack2D/binPack2D-config.cmake)

try_inc(cmake/core/NauKernel/NauKernel-config.cmake)

try_inc(cmake/core/CoreAssets/CoreAssets-config.cmake)
try_inc(cmake/core/CoreScene/CoreScene-config.cmake)
try_inc(cmake/core/PlatformAppApi/PlatformAppApi-config.cmake)
try_inc(cmake/core/PlatformApp/PlatformApp-config.cmake)
try_inc(cmake/core/CoreInput/CoreInput-config.cmake)
try_inc(cmake/core/Render/Render-config.cmake)
try_inc(cmake/core/Render/Render-config.cmake)
try_inc(cmake/core/GraphicsAssets/GraphicsAssets-config.cmake)
try_inc(cmake/core/CoreInput/CoreInput-config.cmake)
try_inc(cmake/core/Animation/Animation-config.cmake)
try_inc(cmake/core/ext_tinyxml2/ext_tinyxml2-config.cmake)
try_inc(cmake/core/ext_clipper/ext_clipper-config.cmake)
try_inc(cmake/core/ext_edtaa3func/ext_edtaa3func-config.cmake)
try_inc(cmake/core/ext_convertUTF/ext_convertUTF-config.cmake)
try_inc(cmake/core/ext_poly2tri/ext_poly2tri-config.cmake)
try_inc(cmake/core/png/png-config.cmake)
try_inc(cmake/core/external/external-config.cmake)
try_inc(cmake/core/cocos2d/cocos2d-config.cmake)
try_inc(cmake/core/ui/ui-config.cmake)
try_inc(cmake/core/DebugRenderer/DebugRenderer-config.cmake)
try_inc(cmake/core/VFX/VFX-config.cmake)
try_inc(cmake/core/Graphics/Graphics-config.cmake)
try_inc(cmake/core/CoreAssetFormats/CoreAssetFormats-config.cmake)
try_inc(cmake/core/NauFramework/NauFramework-config.cmake)
try_inc(cmake/core/Physics/Physics-config.cmake)
try_inc(cmake/core/PhysicsJolt/PhysicsJolt-config.cmake)
try_inc(cmake/core/Network/Network-config.cmake)
try_inc(cmake/core/Audio/Audio-config.cmake)

try_inc(cmake/tools/ShaderCompilerTool/ShaderCompilerTool-config.cmake)

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

add_custom_command(TARGET ${TargetName} PRE_BUILD
  COMMAND ${CMAKE_COMMAND}
  -DFileToCheck=${NauEngineFolder}/lib/$<CONFIG>
  -P ${NauEngineFolder}/cmake/for_build/check.cmake
  COMMENT "Checking if ${NauEngineFolder}/lib/$<CONFIG> exists...")

add_custom_command(TARGET ${TargetName} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
  "${NauEngineFolder}/bin/$<CONFIG>"
  $<TARGET_FILE_DIR:${TargetName}>
)

## add_custom_command(TARGET ${TargetName} POST_BUILD
##   COMMAND ${CMAKE_COMMAND} -E copy
##   "${NauEngineFolder}/bin/TracyClient.dll"
##   $<TARGET_FILE_DIR:${TargetName}>
## )

install(TARGETS ${TargetName}
  DESTINATION bin
)