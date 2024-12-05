
# TODO: set enviroment variables for other platforms
if (WIN32)
  execute_process( COMMAND "${CMAKE_SOURCE_DIR}/cmake/platformSpec/set_env.bat")
endif(WIN32)

if (POLICY CMP0091)
  cmake_policy(SET CMP0091 NEW) 
endif()

if (NOT DEFINED NauEngineFolder)
  message(FATAL_ERROR "NauEngineSDK: NauEngineFolder not defined!")
endif()

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
  "${NauEngineFolder}/cmake"
  "${NauEngineFolder}/cmake/defaults"
  "${NauEngineFolder}/cmake/for_build"
)

# Enable Hot Reload for MSVC compilers if supported.
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

set(NauSDKIncludes ${NauEngineFolder}/include)
set(NauSDKLibraries ${NauEngineFolder}/lib)


function(nau_try_inc_lib target cmakeName)
  if (EXISTS ${NauEngineFolder}/${cmakeName})
    message(STATUS "include ${cmakeName} from NAU sdk")
    include(${NauEngineFolder}/${cmakeName})
    get_target_property(targetType ${target} TYPE)
    if (NOT ${targetType} STREQUAL "INTERFACE_LIBRARY")
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
  endif()
endfunction()

function(nau_try_inc_exe target cmakeName)
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

function(nau_try_add_sample target subdir)
  if (EXISTS ${NauEngineFolder}/${subdir})
    message(STATUS "add ${subdir} from NAU sdk")
    add_subdirectory(${NauEngineFolder}/${subdir} "samples/${target}")
  endif()
endfunction()

nau_try_inc_lib(adlSdk cmake/3rdPartyLibs/adlSdk/adlSdk-config.cmake)
nau_try_inc_lib(asio cmake/3rdPartyLibs/asio/asio-config.cmake)
nau_try_inc_lib(brotli cmake/3rdPartyLibs/brotli/brotli-config.cmake)
nau_try_inc_lib(dxc cmake/3rdPartyLibs/dxc/dxc-config.cmake)
nau_try_inc_lib(EABase cmake/3rdPartyLibs/EABase/EABase-config.cmake)
nau_try_inc_lib(EASTL cmake/3rdPartyLibs/EASTL/EASTL-config.cmake)
nau_try_inc_lib(fast_float cmake/3rdPartyLibs/fast_float/fast_float-config.cmake)
nau_try_inc_lib(fmt cmake/3rdPartyLibs/fmt/fmt-config.cmake)
nau_try_inc_lib(gainput cmake/3rdPartyLibs/gainput/gainput-config.cmake)
nau_try_inc_lib(imgui cmake/3rdPartyLibs/imgui/imgui-config.cmake)
nau_try_inc_lib(ispc_texcomp cmake/3rdPartyLibs/ispc_texcomp/ispc_texcomp-config.cmake)
nau_try_inc_lib(Jolt cmake/3rdPartyLibs/Jolt/Jolt-config.cmake)
nau_try_inc_lib(json cmake/3rdPartyLibs/json/json-config.cmake)
nau_try_inc_lib(jsoncpp cmake/3rdPartyLibs/jsoncpp/jsoncpp-config.cmake)
nau_try_inc_lib(lua cmake/3rdPartyLibs/lua/lua-config.cmake)
nau_try_inc_lib(lzma-9.20 cmake/3rdPartyLibs/lzma-9.20/lzma-9.20-config.cmake)
nau_try_inc_lib(md5 cmake/3rdPartyLibs/md5/md5-config.cmake)
nau_try_inc_lib(miniaudio cmake/3rdPartyLibs/miniaudio/miniaudio-config.cmake)
nau_try_inc_lib(ozz_base cmake/3rdPartyLibs/ozz_base/ozz_base-config.cmake)
nau_try_inc_lib(ozz_options cmake/3rdPartyLibs/ozz_options/ozz_options-config.cmake)
nau_try_inc_lib(ozz_animation cmake/3rdPartyLibs/ozz_animation/ozz_animation-config.cmake)
nau_try_inc_lib(ozz_animation_offline cmake/3rdPartyLibs/ozz_animation_offline/ozz_animation_offline-config.cmake)
nau_try_inc_lib(ozz_animation_tools cmake/3rdPartyLibs/ozz_animation_tools/ozz_animation_tools-config.cmake)
nau_try_inc_lib(ozz_geomenau_try cmake/3rdPartyLibs/ozz_geomenau_try/ozz_geomenau_try-config.cmake)
nau_try_inc_lib(RenderDoc cmake/3rdPartyLibs/RenderDoc/RenderDoc-config.cmake)
nau_try_inc_lib(ska_hash_map cmake/3rdPartyLibs/ska_hash_map/ska_hash_map-config.cmake)
nau_try_inc_lib(stb cmake/3rdPartyLibs/stb/stb-config.cmake)
nau_try_inc_lib(tinydds cmake/3rdPartyLibs/tinydds/tinydds-config.cmake)
nau_try_inc_lib(tinygltf cmake/3rdPartyLibs/tinygltf/tinygltf-config.cmake)
nau_try_inc_lib(tinyimageformat cmake/3rdPartyLibs/tinyimageformat/tinyimageformat-config.cmake)
nau_try_inc_lib(tinyutf8 cmake/3rdPartyLibs/tinyutf8/tinyutf8-config.cmake)
nau_try_inc_lib(uriparser cmake/3rdPartyLibs/uriparser/uriparser-config.cmake)
nau_try_inc_lib(utf8cpp cmake/3rdPartyLibs/utf8cpp/utf8cpp-config.cmake)
nau_try_inc_lib(vectormath cmake/3rdPartyLibs/vectormath/vectormath-config.cmake)
nau_try_inc_lib(wyhash cmake/3rdPartyLibs/wyhash/wyhash-config.cmake)
nau_try_inc_lib(xxHash cmake/3rdPartyLibs/xxHash/xxHash-config.cmake)
nau_try_inc_lib(zlib-ng cmake/3rdPartyLibs/zlib-ng/zlib-ng-config.cmake)
nau_try_inc_lib(zstd-1.4.5 cmake/3rdPartyLibs/zstd-1.4.5/zstd-1.4.5-config.cmake)

nau_try_inc_lib(NauKernel cmake/core/NauKernel/NauKernel-config.cmake)
nau_try_inc_lib(binPack2D cmake/3rdPartyLibs/binPack2D/binPack2D-config.cmake)

#nau_try_inc_lib(Tracy share/Tracy/TracyConfig.cmake)

nau_try_inc_lib(ext_clipper cmake/core/ext_clipper/ext_clipper-config.cmake)
nau_try_inc_lib(ext_convertUTF cmake/core/ext_convertUTF/ext_convertUTF-config.cmake)
nau_try_inc_lib(ext_edtaa3func cmake/core/ext_edtaa3func/ext_edtaa3func-config.cmake)
nau_try_inc_lib(ext_poly2tri cmake/core/ext_poly2tri/ext_poly2tri-config.cmake)
nau_try_inc_lib(ext_tinyxml2 cmake/core/ext_tinyxml2/ext_tinyxml2-config.cmake)
nau_try_inc_lib(png cmake/core/png/png-config.cmake)
nau_try_inc_lib(external cmake/core/external/external-config.cmake)
nau_try_inc_lib(PlatformAppApi cmake/core/PlatformAppApi/PlatformAppApi-config.cmake)
nau_try_inc_lib(PlatformApp cmake/core/PlatformApp/PlatformApp-config.cmake)
nau_try_inc_lib(CoreAssets cmake/core/CoreAssets/CoreAssets-config.cmake)
nau_try_inc_lib(CoreInput cmake/core/CoreInput/CoreInput-config.cmake)
nau_try_inc_lib(CoreScene cmake/core/CoreScene/CoreScene-config.cmake)
nau_try_inc_lib(Render cmake/core/Render/Render-config.cmake)
nau_try_inc_lib(Animation cmake/core/Animation/Animation-config.cmake)
nau_try_inc_lib(CoreAssetFormats cmake/core/CoreAssetFormats/CoreAssetFormats-config.cmake)
nau_try_inc_lib(GraphicsAssets cmake/core/GraphicsAssets/GraphicsAssets-config.cmake)
nau_try_inc_lib(Audio cmake/core/Audio/Audio-config.cmake)
nau_try_inc_lib(cocos2d cmake/core/cocos2d/cocos2d-config.cmake)
nau_try_inc_lib(ui cmake/core/ui/ui-config.cmake)
nau_try_inc_lib(VFX cmake/core/VFX/VFX-config.cmake)
nau_try_inc_lib(DebugRenderer cmake/core/DebugRenderer/DebugRenderer-config.cmake) 
nau_try_inc_lib(Graphics cmake/core/Graphics/Graphics-config.cmake) 
nau_try_inc_lib(LuaToolkit cmake/core/LuaToolkit/LuaToolkit-config.cmake) 
nau_try_inc_lib(Network cmake/core/Network/Network-config.cmake)
nau_try_inc_lib(Physics cmake/core/Physics/Physics-config.cmake)
nau_try_inc_lib(PhysicsJolt cmake/core/PhysicsJolt/PhysicsJolt-config.cmake)
nau_try_inc_lib(ScriptsLua cmake/core/ScriptsLua/ScriptsLua-config.cmake)
nau_try_inc_lib(NauFramework cmake/core/NauFramework/NauFramework-config.cmake)

nau_try_inc_lib(nlohmann_json cmake/tools/nlohmann_json/nlohmann_json-config.cmake)
nau_try_inc_lib(Shared cmake/tools/Shared/Shared-config.cmake)
nau_try_inc_lib(UsdHelpers cmake/tools/UsdHelpers/UsdHelpers-config.cmake)
nau_try_inc_lib(UsdProxy cmake/tools/UsdProxy/UsdProxy-config.cmake)
nau_try_inc_lib(UsdMetaTools cmake/tools/UsdMetaTools/UsdMetaTools-config.cmake)
nau_try_inc_lib(AssetPackTool cmake/tools/AssetPackTool/AssetPackTool-config.cmake)
nau_try_inc_lib(AssetTool cmake/tools/AssetTool/AssetTool-config.cmake)
nau_try_inc_lib(BuildTool cmake/tools/BuildTool/BuildTool-config.cmake)
nau_try_inc_exe(BuildToolCmd cmake/tools/BuildToolCmd/BuildToolCmd-config.cmake)
nau_try_inc_exe(MaterialCreationTool cmake/tools/MaterialCreationTool/MaterialCreationTool-config.cmake)
nau_try_inc_lib(ProjectTool cmake/tools/ProjectTool/ProjectTool-config.cmake)
nau_try_inc_exe(ProjectToolCmd cmake/tools/ProjectToolCmd/ProjectToolCmd-config.cmake)
nau_try_inc_exe(ShaderCompilerTool cmake/tools/ShaderCompilerTool/ShaderCompilerTool-config.cmake)

nau_try_inc_lib(NauAnimationClipAsset cmake/tools/plugins/NauAnimationClipAsset/NauAnimationClipAsset-config.cmake)
nau_try_inc_lib(NauAssetMetadata cmake/tools/plugins/NauAssetMetadata/NauAssetMetadata-config.cmake)
nau_try_inc_lib(NauAudioSchema cmake/tools/plugins/NauAudioSchema/NauAudioSchema-config.cmake)
nau_try_inc_lib(NauComponentSchema cmake/tools/plugins/NauComponentSchema/NauComponentSchema-config.cmake)
nau_try_inc_lib(NauGuiSchema cmake/tools/plugins/NauGuiSchema/NauGuiSchema-config.cmake)
nau_try_inc_lib(NauInputSchema cmake/tools/plugins/NauInputSchema/NauInputSchema-config.cmake)
nau_try_inc_lib(NauPhysicsSchema cmake/tools/plugins/NauPhysicsSchema/NauPhysicsSchema-config.cmake)
nau_try_inc_lib(NauUsdFormat cmake/tools/plugins/NauUsdFormat/NauUsdFormat-config.cmake)
nau_try_inc_lib(NauVFXSchema cmake/tools/plugins/NauVFXSchema/NauVFXSchema-config.cmake)
nau_try_inc_lib(UsdUidLookup cmake/tools/plugins/UsdUidLookup/UsdUidLookup-config.cmake)

nau_try_inc_lib(UsdTranslator cmake/tools/UsdTranslator/UsdTranslator-config.cmake)
nau_try_inc_lib(UsdTranslatorWrapper cmake/tools/plugins/UsdTranslatorWrapper/UsdTranslatorWrapper-config.cmake)

function(nau_add_samples)
    nau_try_add_sample(sample_common samples/sample_common)
    nau_try_add_sample(all3rdpartyBuild samples/all3rdpartyBuild)
    nau_try_add_sample(inputDemo samples/inputDemo)
    nau_try_add_sample(kernelBuild samples/kernelBuild)
    nau_try_add_sample(minimalApp samples/minimalApp)
    nau_try_add_sample(networkTransport samples/networkTransport)
    nau_try_add_sample(physics samples/physics)
    nau_try_add_sample(sceneBase samples/sceneBase)
    nau_try_add_sample(ui-cpp-tests samples/ui-cpp-tests)
    nau_try_add_sample(uiSampleApp samples/uiSampleApp)
endfunction()
