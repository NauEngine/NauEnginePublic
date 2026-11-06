set(TargetName Mgpu)

nau_collect_files(Sources
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}

  MASK "*.cpp" "*.h"
)

nau_collect_files(PublicHeaders
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}

  MASK "*.h" "*.hpp"
)

nau_add_module(${TargetName}
  SOURCES ${Sources} ${PublicHeaders}
  FOLDER "${NauEngineFolder}/core/modules"
  EXPORT_AS_LIBRARY
)

# Third-party dependencies of the MGPU infrastructure (A: packages.config)
find_package(directxtex CONFIG REQUIRED)
find_package(directxtk12 CONFIG REQUIRED)

# WinPixEventRuntime is not a vcpkg port: fetch the same NuGet package that A uses.
set(MGPU_PIX_VERSION 1.0.240308001)
set(MGPU_PIX_DIR ${CMAKE_BINARY_DIR}/_deps/WinPixEventRuntime-${MGPU_PIX_VERSION})
if(NOT EXISTS ${MGPU_PIX_DIR}/Include/WinPixEventRuntime/pix3.h)
  set(MGPU_PIX_NUPKG ${MGPU_PIX_DIR}/WinPixEventRuntime.zip)
  file(DOWNLOAD
    https://api.nuget.org/v3-flatcontainer/winpixeventruntime/${MGPU_PIX_VERSION}/winpixeventruntime.${MGPU_PIX_VERSION}.nupkg
    ${MGPU_PIX_NUPKG}
    STATUS MGPU_PIX_DOWNLOAD_STATUS
    TLS_VERIFY ON
  )
  list(GET MGPU_PIX_DOWNLOAD_STATUS 0 MGPU_PIX_DOWNLOAD_CODE)
  if(NOT MGPU_PIX_DOWNLOAD_CODE EQUAL 0)
    message(FATAL_ERROR "Failed to download WinPixEventRuntime ${MGPU_PIX_VERSION}: ${MGPU_PIX_DOWNLOAD_STATUS}")
  endif()
  file(ARCHIVE_EXTRACT INPUT ${MGPU_PIX_NUPKG} DESTINATION ${MGPU_PIX_DIR})
endif()

add_library(WinPixEventRuntime SHARED IMPORTED GLOBAL)
set_target_properties(WinPixEventRuntime PROPERTIES
  IMPORTED_IMPLIB ${MGPU_PIX_DIR}/bin/x64/WinPixEventRuntime.lib
  IMPORTED_LOCATION ${MGPU_PIX_DIR}/bin/x64/WinPixEventRuntime.dll
  INTERFACE_INCLUDE_DIRECTORIES ${MGPU_PIX_DIR}/Include/WinPixEventRuntime
)

target_link_libraries(${TargetName} PUBLIC
  Microsoft::DirectXTex
  Microsoft::DirectXTK12
  WinPixEventRuntime
  d3d12
  dxgi
  d3dcompiler
  dxguid
)

# GPU timestamp profiler: the bridges time their own queues with D3D12TimestampSource. Its headers have no Nau or Mgpu
# types, so neither side leaks into the other.
target_link_libraries(${TargetName} PRIVATE GpuProfiler)

# A builds Graphics with ConformanceMode=false (code takes the address of temporaries,
# e.g. &CD3DX12_HEAP_PROPERTIES(...)); B defaults to /permissive-.
target_compile_options(${TargetName} PRIVATE /permissive)

# A builds all projects with CharacterSet=Unicode.
target_compile_definitions(${TargetName} PUBLIC NOMINMAX UNICODE _UNICODE)

# Tells nau/mgpu/bridge/mgpu_bridge.h to declare the real bridge instead of the inline stubs.
target_compile_definitions(${TargetName} PUBLIC NAU_ENABLE_MGPU=1)

# Replacement for Build/SampleResources.targets in A: the shaders of the Mgpu core (MipMapCS.hlsl for
# GTexture::GenerateMipMaps, Quad.hlsl) and the PIX runtime are copied next to the executable after build.
set(MGPU_SHARED_SHADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Common CACHE PATH "MGPU shared shaders")

function(mgpu_copy_runtime_resources Target)
  if(EXISTS ${MGPU_SHARED_SHADERS_DIR})
    add_custom_command(TARGET ${Target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory ${MGPU_SHARED_SHADERS_DIR} $<TARGET_FILE_DIR:${Target}>/Shaders)
  endif()
  add_custom_command(TARGET ${Target} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:WinPixEventRuntime> $<TARGET_FILE_DIR:${Target}>)
endfunction()

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/mgpu/include>
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Source FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX Headers FILES ${PublicHeaders})

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/mgpu
    FILES_MATCHING
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

nau_install(${TargetName} core)

# Bridge transfer check without Nau (needs two GPUs). Only for the static runtime: Mgpu has no DLL exports.
# Only with NAU_ENABLE_MGPU: the test links the vcpkg DirectX libraries, which match the static CRT of the engine only
# with the x64-windows-static triplet of that preset (see the root CMakeLists.txt).
if(NOT BUILD_SHARED_LIBS AND NAU_ENABLE_MGPU)
  add_subdirectory(samples/BridgeTransferTest)
endif()
