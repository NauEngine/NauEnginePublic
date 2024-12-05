#cmake_minimum_required(VERSION 3.6)
#project(external)

set (CocosExternalsFolder "${NauEngineFolder}/3rdparty/cocos_deps")

include(cmake/CocosExternalConfig.cmake)

option(BUILD_EXT_FREETYPE2 "Build with internal freetype2 support" ON)
option(BUILD_EXT_JPEG "Build with internal jpeg support" ON)
option(BUILD_EXT_TINYXML2 "Build with internal tinyxml2 support" ON)
option(BUILD_EXT_CLIPPER "Build with internal clipper support" ON)
option(BUILD_EXT_EDTAA3FUNC "Build with internal edtaa3func support" ON)
option(BUILD_EXT_CONVERTUTF "Build with internal ConvertUTF support" ON)
option(BUILD_EXT_POLY2TRI "Build with internal poly2tri support" ON)
option(BUILD_EXT_MD5 "Build with internal md5 support" ON)
option(BUILD_EXT_ZLIB "Build with internal zlib support" ON)
option(BUILD_EXT_UNZIP "Build with internal unzip support" ON)

add_library(external STATIC empty.cpp)

target_link_directories(external INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/lib>
    )

if(BUILD_EXT_FREETYPE2)
    add_subdirectory(freetype2)
    target_link_libraries(external ext_freetype)
    target_include_directories(external INTERFACE
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/freetype2/include/${platform_name}/freetype2>  
      $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/core/modules/ui/cocos2d-x/external/freetype2/include/${platform_name}/freetype2>
    )
endif(BUILD_EXT_FREETYPE2)
if(BUILD_EXT_JPEG)
    add_subdirectory(jpeg)
    target_link_libraries(external ext_jpeg)
endif(BUILD_EXT_JPEG)
if(BUILD_EXT_TINYXML2)
    add_subdirectory(tinyxml2)
    target_link_libraries(external ext_tinyxml2)
    list(APPEND ExportTargets ext_tinyxml2)
endif(BUILD_EXT_TINYXML2)
    target_link_libraries(external xxHash)
if(BUILD_EXT_CLIPPER)
    add_subdirectory(clipper)
    target_link_libraries(external ext_clipper)
    list(APPEND ExportTargets ext_clipper)
endif(BUILD_EXT_CLIPPER)
if(BUILD_EXT_EDTAA3FUNC)
    add_subdirectory(edtaa3func)
    target_link_libraries(external ext_edtaa3func)
    list(APPEND ExportTargets ext_edtaa3func)
endif(BUILD_EXT_EDTAA3FUNC)
if(BUILD_EXT_CONVERTUTF)
    add_subdirectory(ConvertUTF)
    target_link_libraries(external ext_convertUTF)
    list(APPEND ExportTargets ext_convertUTF)
endif(BUILD_EXT_CONVERTUTF)
if(BUILD_EXT_POLY2TRI)
    add_subdirectory(poly2tri)
    target_link_libraries(external ext_poly2tri)
    list(APPEND ExportTargets ext_poly2tri)
endif(BUILD_EXT_POLY2TRI)

if(BUILD_EXT_MD5)
    #add_subdirectory(md5)
    target_link_libraries(external md5)
endif(BUILD_EXT_MD5)


if(NOT LINUX)
    target_link_libraries(external png)
endif(NOT LINUX)
    
if(LINUX)
    add_subdirectory(linux-specific/fmod)
    target_link_libraries(external 
        ext_fmod
    )
endif()

if(ANDROID)
    add_subdirectory(android-specific/cpufeatures)
    add_subdirectory(android-specific/pvmp3dec)
    add_subdirectory(android-specific/tremolo)
    target_link_libraries(external 
        ext_cpufeatures 
        ext_pvmp3dec 
        ext_tremolo
    )
endif()


if(MACOSX OR ANDROID OR WINDOWS)
    if(BUILD_EXT_ZLIB)
        #add_subdirectory(zlib)
        target_link_libraries(external 
            zlib-ng
        )
    endif(BUILD_EXT_ZLIB)
endif()

# put "external" into External folder, too
set_target_properties(external
    PROPERTIES
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    FOLDER "${CocosExternalsFolder}"
)


if(MACOSX OR IOS)
    add_subdirectory(glsl-optimizer)
    target_link_libraries(external 
    ext_glcpp_library
    ext_glsl_optimizer
    ext_libmesa
    )
endif()

nau_add_compile_options(external)

#export(TARGETS
#    ${ExportTargets}
#    external
#  FILE ${CMAKE_BINARY_DIR}/cmake/NauCocos2dExternal.cmake)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/tinyxml2 DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/ui/cocos2d-x/external FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.ipp")
nau_install(ext_tinyxml2 core)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/clipper DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/ui/cocos2d-x/external FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.ipp")
nau_install(ext_clipper core)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/edtaa3func DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/ui/cocos2d-x/external FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.ipp")
nau_install(ext_edtaa3func core)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/ConvertUTF DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/ui/cocos2d-x/external FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.ipp")
nau_install(ext_convertUTF core)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/poly2tri DESTINATION ${CMAKE_INSTALL_PREFIX}/include/core/modules/ui/cocos2d-x/external FILES_MATCHING PATTERN "*.h" PATTERN "*.hpp" PATTERN "*.ipp")
nau_install(ext_poly2tri core)

nau_install(png core)

nau_install(external core)
