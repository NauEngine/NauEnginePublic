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

set(CMAKE_MODULE_PATH ${NauEngineFolder}
  ${NauEngineFolder}/cmake
  ${NauEngineFolder}/cmake/defaults
)

# TODO: set enviroment variables for other platforms
if(WIN32)
  execute_process(COMMAND "${CMAKE_SOURCE_DIR}/cmake/platformSpec/set_env.bat")
endif(WIN32)


# Compiler config
set(CMAKE_CXX_STANDARD 20)
# TODO: define C standard

# Enable Hot Reload for MSVC compilers if supported.
if(POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

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

set(NauSDKIncludes ${NauEngineFolder}/include)
set(NauSDKLibraries ${NauEngineFolder}/lib)


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


include(NauCommon)
include(NauModule)
include(NauGenFunctions)
include(platformSpec/NauPlatformSetup)


function(try_inc_lib target cmakeName)
  if (EXISTS ${NauEngineFolder}/${cmakeName})
    message(STATUS "include ${cmakeName} from NAU sdk")
    include(${NauEngineFolder}/${cmakeName})
    if (BUILD_SHARED_LIBS)
      set_target_properties(${target} PROPERTIES
        IMPORTED_LOCATION_Debug "${NauEngineFolder}/bin/Debug/${target}.dll"
        IMPORTED_LOCATION_Release "${NauEngineFolder}/bin/Release/${target}.dll"
        IMPORTED_IMPLIB_Debug "${NauEngineFolder}/lib/Debug/${target}.lib"
        IMPORTED_IMPLIB_Release "${NauEngineFolder}/lib/Release/${target}.lib"
        IMPORTED_GLOBAL TRUE
      )
    else()
      set_target_properties(${target} PROPERTIES
        IMPORTED_LOCATION_Debug "${NauEngineFolder}/bin/Debug/${target}.lib"
        IMPORTED_LOCATION_Release "${NauEngineFolder}/bin/Release/${target}.lib"
        IMPORTED_GLOBAL TRUE
      )
    endif()
  endif()
endfunction()

function(try_inc_exe target cmakeName)
  if (EXISTS ${NauEngineFolder}/${cmakeName})
    message(STATUS "include ${cmakeName} from NAU sdk")
    include(${NauEngineFolder}/${cmakeName})
    set_target_properties(${target} PROPERTIES
      IMPORTED_LOCATION_Debug "${NauEngineFolder}/bin/Debug/${target}.exe"
      IMPORTED_LOCATION_Release "${NauEngineFolder}/bin/Release/${target}.exe"
      IMPORTED_GLOBAL TRUE
    )
  endif()
endfunction()

function(try_add subdir)
  if (EXISTS ${NauEngineFolder}/${subdir})
    message(STATUS "add ${subdir} from NAU sdk")
    add_subdirectory(${NauEngineFolder}/${subdir})
  endif()
endfunction()

try_inc_lib(adlSdk cmake/3rdPartyLibs/adlSdk/adlSdk-config.cmake)
try_inc_lib(asio cmake/3rdPartyLibs/asio/asio-config.cmake)
try_inc_lib(brotli cmake/3rdPartyLibs/brotli/brotli-config.cmake)
try_inc_lib(dxc cmake/3rdPartyLibs/dxc/dxc-config.cmake)
try_inc_lib(EABase cmake/3rdPartyLibs/EABase/EABase-config.cmake)
try_inc_lib(EASTL cmake/3rdPartyLibs/EASTL/EASTL-config.cmake)
try_inc_lib(fast_float cmake/3rdPartyLibs/fast_float/fast_float-config.cmake)
try_inc_lib(fmt cmake/3rdPartyLibs/fmt/fmt-config.cmake)
try_inc_lib(gainput cmake/3rdPartyLibs/gainput/gainput-config.cmake)
try_inc_lib(imgui cmake/3rdPartyLibs/imgui/imgui-config.cmake)
try_inc_lib(ispc_texcomp cmake/3rdPartyLibs/ispc_texcomp/ispc_texcomp-config.cmake)
try_inc_lib(Jolt cmake/3rdPartyLibs/Jolt/Jolt-config.cmake)
try_inc_lib(json cmake/3rdPartyLibs/json/json-config.cmake)
try_inc_lib(jsoncpp cmake/3rdPartyLibs/jsoncpp/jsoncpp-config.cmake)
try_inc_lib(lua cmake/3rdPartyLibs/lua/lua-config.cmake)
try_inc_lib(lzma-9.20 cmake/3rdPartyLibs/lzma-9.20/lzma-9.20-config.cmake)
try_inc_lib(md5 cmake/3rdPartyLibs/md5/md5-config.cmake)
try_inc_lib(ozz_base cmake/3rdPartyLibs/ozz_base/ozz_base-config.cmake)
try_inc_lib(ozz_options cmake/3rdPartyLibs/ozz_options/ozz_options-config.cmake)
try_inc_lib(ozz_animation cmake/3rdPartyLibs/ozz_animation/ozz_animation-config.cmake)
try_inc_lib(ozz_animation_offline cmake/3rdPartyLibs/ozz_animation_offline/ozz_animation_offline-config.cmake)
try_inc_lib(ozz_animation_tools cmake/3rdPartyLibs/ozz_animation_tools/ozz_animation_tools-config.cmake)
try_inc_lib(ozz_geometry cmake/3rdPartyLibs/ozz_geometry/ozz_geometry-config.cmake)
try_inc_lib(RenderDoc cmake/3rdPartyLibs/RenderDoc/RenderDoc-config.cmake)
try_inc_lib(ska_hash_map cmake/3rdPartyLibs/ska_hash_map/ska_hash_map-config.cmake)
try_inc_lib(stb cmake/3rdPartyLibs/stb/stb-config.cmake)
try_inc_lib(tinydds cmake/3rdPartyLibs/tinydds/tinydds-config.cmake)
try_inc_lib(tinygltf cmake/3rdPartyLibs/tinygltf/tinygltf-config.cmake)
try_inc_lib(tinyimageformat cmake/3rdPartyLibs/tinyimageformat/tinyimageformat-config.cmake)
try_inc_lib(tinyutf8 cmake/3rdPartyLibs/tinyutf8/tinyutf8-config.cmake)
try_inc_lib(uriparser cmake/3rdPartyLibs/uriparser/uriparser-config.cmake)
try_inc_lib(utf8cpp cmake/3rdPartyLibs/utf8cpp/utf8cpp-config.cmake)
try_inc_lib(vectormath cmake/3rdPartyLibs/vectormath/vectormath-config.cmake)
try_inc_lib(wyhash cmake/3rdPartyLibs/wyhash/wyhash-config.cmake)
try_inc_lib(xxHash cmake/3rdPartyLibs/xxHash/xxHash-config.cmake)
try_inc_lib(zlib-ng cmake/3rdPartyLibs/zlib-ng/zlib-ng-config.cmake)
try_inc_lib(zstd-1.4.5 cmake/3rdPartyLibs/zstd-1.4.5/zstd-1.4.5-config.cmake)

try_inc_lib(NauKernel cmake/core/NauKernel/NauKernel-config.cmake)
try_inc_lib(binPack2D cmake/3rdPartyLibs/binPack2D/binPack2D-config.cmake)

#try_inc_lib(Tracy share/Tracy/TracyConfig.cmake)

try_inc_lib(ext_clipper cmake/core/ext_clipper/ext_clipper-config.cmake)
try_inc_lib(ext_convertUTF cmake/core/ext_convertUTF/ext_convertUTF-config.cmake)
try_inc_lib(ext_edtaa3func cmake/core/ext_edtaa3func/ext_edtaa3func-config.cmake)
try_inc_lib(ext_poly2tri cmake/core/ext_poly2tri/ext_poly2tri-config.cmake)
try_inc_lib(ext_tinyxml2 cmake/core/ext_tinyxml2/ext_tinyxml2-config.cmake)
try_inc_lib(png cmake/core/png/png-config.cmake)
try_inc_lib(external cmake/core/external/external-config.cmake)
try_inc_lib(CoreAssetFormats cmake/core/CoreAssetFormats/CoreAssetFormats-config.cmake)
try_inc_lib(CoreAssets cmake/core/CoreAssets/CoreAssets-config.cmake)
try_inc_lib(CoreInput cmake/core/CoreInput/CoreInput-config.cmake)
try_inc_lib(CoreScene cmake/core/CoreScene/CoreScene-config.cmake)
try_inc_lib(Animation cmake/core/Animation/Animation-config.cmake)
try_inc_lib(PlatformAppApi cmake/core/PlatformAppApi/PlatformAppApi-config.cmake)
try_inc_lib(PlatformApp cmake/core/PlatformApp/PlatformApp-config.cmake)
try_inc_lib(Render cmake/core/Render/Render-config.cmake)
try_inc_lib(GraphicsAssets cmake/core/GraphicsAssets/GraphicsAssets-config.cmake)
try_inc_lib(Audio cmake/core/Audio/Audio-config.cmake)
try_inc_lib(cocos2d cmake/core/cocos2d/cocos2d-config.cmake)
try_inc_lib(DebugRenderer cmake/core/DebugRenderer/DebugRenderer-config.cmake) 
try_inc_lib(Graphics cmake/core/Graphics/Graphics-config.cmake) 
try_inc_lib(LuaToolkit cmake/core/LuaToolkit/LuaToolkit-config.cmake) 
try_inc_lib(Network cmake/core/Network/Network-config.cmake)
try_inc_lib(Physics cmake/core/Physics/Physics-config.cmake)
try_inc_lib(PhysicsJolt cmake/core/PhysicsJolt/PhysicsJolt-config.cmake)
try_inc_lib(ScriptsLua cmake/core/ScriptsLua/ScriptsLua-config.cmake)
try_inc_lib(ui cmake/core/ui/ui-config.cmake)
try_inc_lib(VFX cmake/core/VFX/VFX-config.cmake)
try_inc_lib(NauFramework cmake/core/NauFramework/NauFramework-config.cmake)

try_inc_lib(nlohmann_json cmake/tools/nlohmann_json/nlohmann_json-config.cmake)
try_inc_lib(Shared cmake/tools/Shared/Shared-config.cmake)
try_inc_lib(UsdHelpers cmake/tools/UsdHelpers/UsdHelpers-config.cmake)
try_inc_lib(UsdProxy cmake/tools/UsdProxy/UsdProxy-config.cmake)
try_inc_lib(UsdMetaTools cmake/tools/UsdMetaTools/UsdMetaTools-config.cmake)
try_inc_lib(AssetPackTool cmake/tools/AssetPackTool/AssetPackTool-config.cmake)
try_inc_lib(AssetTool cmake/tools/AssetTool/AssetTool-config.cmake)
try_inc_lib(BuildTool cmake/tools/BuildTool/BuildTool-config.cmake)
try_inc_exe(BuildToolCmd cmake/tools/BuildToolCmd/BuildToolCmd-config.cmake)
try_inc_exe(MaterialCreationTool cmake/tools/MaterialCreationTool/MaterialCreationTool-config.cmake)
try_inc_lib(ProjectTool cmake/tools/ProjectTool/ProjectTool-config.cmake)
try_inc_exe(ProjectToolCmd cmake/tools/ProjectToolCmd/ProjectToolCmd-config.cmake)
try_inc_exe(ShaderCompilerTool cmake/tools/ShaderCompilerTool/ShaderCompilerTool-config.cmake)

try_inc_lib(NauAnimationClipAsset cmake/tools/plugins/NauAnimationClipAsset/NauAnimationClipAsset-config.cmake)
try_inc_lib(NauAssetMetadata cmake/tools/plugins/NauAssetMetadata/NauAssetMetadata-config.cmake)
try_inc_lib(NauAudioSchema cmake/tools/plugins/NauAudioSchema/NauAudioSchema-config.cmake)
try_inc_lib(NauComponentSchema cmake/tools/plugins/NauComponentSchema/NauComponentSchema-config.cmake)
try_inc_lib(NauGuiSchema cmake/tools/plugins/NauGuiSchema/NauGuiSchema-config.cmake)
try_inc_lib(NauInputSchema cmake/tools/plugins/NauInputSchema/NauInputSchema-config.cmake)
try_inc_lib(NauPhysicsSchema cmake/tools/plugins/NauPhysicsSchema/NauPhysicsSchema-config.cmake)
try_inc_lib(NauUsdFormat cmake/tools/plugins/NauUsdFormat/NauUsdFormat-config.cmake)
try_inc_lib(NauVFXSchema cmake/tools/plugins/NauVFXSchema/NauVFXSchema-config.cmake)
try_inc_lib(UsdUidLookup cmake/tools/plugins/UsdUidLookup/UsdUidLookup-config.cmake)

try_inc_lib(UsdTranslator cmake/tools/UsdTranslator/UsdTranslator-config.cmake)
try_inc_lib(UsdTranslatorWrapper cmake/tools/plugins/UsdTranslatorWrapper/UsdTranslatorWrapper-config.cmake)


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