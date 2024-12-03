set(TargetName UsdHelpers)

nau_collect_files(Sources
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/src
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}

    MASK "*.cpp" "*.h"
)

nau_collect_files(PublicHeaders
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}

    MASK "*.h"
)

nau_add_module(${TargetName}
    SOURCES ${Sources} ${PublicHeaders}
    FOLDER "${NauEngineFolder}/tools/usd"
    EXPORT_AS_LIBRARY
)

target_compile_definitions(${TargetName} PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0>)
target_link_options(${TargetName} PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(${TargetName} PRIVATE USD_HELPERS_BUILD)
target_include_directories(${TargetName} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/${TargetName}/include> 
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/src PREFIX Source FILES ${Sources})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR}/include PREFIX Headers FILES ${PublicHeaders})

target_link_libraries(${TargetName} PRIVATE
    usd
    usdUtils
    usdGeom
    
    NauKernel
)

if (NAU_CORE_TESTS)
  add_subdirectory(tests)
endif()

nau_install(${TargetName} tools ${PublicHeaders})