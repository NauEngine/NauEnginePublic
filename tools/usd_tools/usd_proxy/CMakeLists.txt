set(Sources
    usd_property_proxy.cpp
    usd_prim_proxy.cpp
    usd_proxy_decorators_regestry.cpp
    usd_stage_watcher.cpp
)

set(Headers
    usd_property_proxy.h
    usd_prim_proxy.h
    usd_proxy_decorators_regestry.h
    usd_proxy_api.h
    usd_proxy.h
    usd_stage_watcher.h
)

source_group("Sources" FILES ${Sources})
source_group("Headers" FILES ${Headers})

add_library(UsdProxy SHARED ${Sources} ${Headers})

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH SOURCE_BASE_DIR)
target_compile_definitions(UsdProxy PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0> )
target_link_options(UsdProxy PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(UsdProxy PRIVATE USD_PROXY_EXPORT)
target_include_directories(UsdProxy PUBLIC 
    $<BUILD_INTERFACE:${SOURCE_BASE_DIR}>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools> 
)

target_link_libraries(UsdProxy
    usd
    usdUtils
)

nau_add_compile_options(UsdProxy)

if (NAU_CORE_TESTS)
	add_subdirectory(tests)
endif()

#add_custom_command(TARGET UsdProxy POST_BUILD
#    COMMAND ${CMAKE_COMMAND} -E copy_directory ${PXR_LIB_DIR}/$<CONFIG> ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
#    COMMAND ${CMAKE_COMMAND} -E copy ${PXR_CMAKE_DIR}/Python/python310.dll ${PXR_LIB_DIR}/boost_python310-vc143-mt-x64-1_85.dll ${PXR_LIB_DIR}/boost_python310-vc143-mt-gd-x64-1_85.dll ${PXR_CMAKE_DIR}/bin/tbb.dll ${CMAKE_BINARY_DIR}/bin/$<CONFIG>
#)

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/tools/usd_proxy
    FILES_MATCHING 
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

nau_install(UsdProxy tools)