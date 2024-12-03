set(FolderName "${NauEngineFolder}/3rdparty")

set (3rdPartyLibDirs
  asio
  EABase
  EASTL
#  GameNetworkingSockets
  lua
  DirectX-Headers
  tiny-utf8
  fmt
  utfcpp
  ModifiedSonyMath
  JoltPhysics
  jsoncpp
  ska_hash_map
  xxHash
  wyhash
  brotli
  lzma-9.20
  zlib-ng
  zstd-1.4.5
  md5
  stb
  tinyimageformat
  fast_float
  ispc_texcomp
  tinydds
  binPack2D
  RenderDoc
  adl_sdk
  dxc
  imgui
  uriparser
  ozz
  libpng-libpng16
  miniaudio
  tinygltf
)

set (3rdPartyLibTargets
  asio
  EABase
  EASTL
  lua
  DirectX-Headers
  DirectX-Guids
  tinyutf8
  fmt
#  webrtc-lite
#  steamwebrtc
#  GameNetworkingSockets
#  GameNetworkingSockets_s
  utf8cpp
  vectormath
  Jolt
  jsoncpp
  ska_hash_map
  xxHash
  wyhash
  brotli
  lzma-9.20
  zlib-ng
  zstd-1.4.5
  md5
  stb
  tinyimageformat
  fast_float
  ispc_texcomp
  tinydds
  binPack2D
  RenderDoc
  adlSdk
  dxc
  imgui
  uriparser
  ozz_base
  ozz_animation
  ozz_animation_tools
  png
  miniaudio
  tinygltf
)

set(GAINPUT_SAMPLES OFF)
set(GAINPUT_TESTS OFF)
if (BUILD_SHARED_LIBS)
  set(GAINPUT_BUILD_STATIC OFF)
  list(APPEND 3rdPartyLibDirs gainput)
  list(APPEND 3rdPartyLibTargets gainput)
else()
  set(GAINPUT_BUILD_SHARED OFF)
  list(APPEND 3rdPartyLibDirs gainput)
  list(APPEND 3rdPartyLibTargets gainputstatic)
endif()

## option(TRACY_ON_DEMAND "" ON) # start profiling when connection with studio is established (no profiling of startup)

option(ozz_build_tests "" OFF) # conflict: duplication of gtest dependency
option(ozz_build_tools "" ON) # we need it for gltf2ozz
option(ozz_build_samples "" OFF)
option(ozz_build_howtos "" OFF)

#set (BUILD_TESTING OFF)

foreach (libDir ${3rdPartyLibDirs})
  add_subdirectory(${libDir})
endforeach()

nau_target_add_compile_options(TARGETS ${3rdPartyLibTargets})
set_target_properties ( ${3rdPartyLibTargets}
  PROPERTIES
    FOLDER "${FolderName}"
)

if (BUILD_SHARED_LIBS)
  target_compile_definitions(EASTL PUBLIC EA_DLL=1)
else()
  target_compile_definitions(EASTL PUBLIC EA_DLL=0 EASTL_USER_DEFINED_ALLOCATOR)
endif()

target_sources(EASTL PRIVATE EASTL/EASTL.natvis)


## test targets
if (NAU_CORE_TESTS)
  set(gtest_force_shared_crt OFF CACHE BOOL "" FORCE)
  set(BUILD_GTEST ON CACHE BOOL "" FORCE)
  set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)

  add_subdirectory(googletest)
  nau_target_add_compile_options(TARGETS gtest gmock)

  set_target_properties (
    gtest gmock gtest_main gmock_main
    PROPERTIES
      FOLDER "${FolderName}"
  )

  set(ThirdPartyTestTargets
    gainputtest
  )

  foreach (testTarget ${ThirdPartyTestTargets})
    if (TARGET ${testTarget})
        set_target_properties (${testTarget} PROPERTIES
          FOLDER "${NauEngineFolder}/tests"
        )
    endif()
  endforeach()

endif()


list(REMOVE_ITEM 3rdPartyLibTargets fmt )

if (NOT BUILD_SHARED_LIBS)
  add_library(gainput ALIAS gainputstatic)
endif()
