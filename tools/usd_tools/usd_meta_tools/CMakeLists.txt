set(TargetName UsdMetaTools)

nau_collect_files(Sources
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
    MASK "*.cpp"
)

nau_collect_files(Headers
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
    MASK "*.h"
)

set(Files ${Sources} ${Headers})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Files})

if(BUILD_SHARED_LIBS)
    add_library(${TargetName} SHARED ${Files})
    target_compile_definitions(${TargetName} PRIVATE USD_META_TOOLS_EXPORT)
else()
    add_library(${TargetName} STATIC ${Files})
    target_compile_definitions(${TargetName} PRIVATE NAU_STATIC_RUNTIME)
endif()

set_target_properties(${TargetName}
    PROPERTIES
        MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>"
)
nau_add_compile_options(${TargetName})

target_link_libraries(${TargetName}
    PUBLIC
        usd
        UsdProxy
        Shared
        NauKernel
)
target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/${TargetName}/include> 
)

nau_install(${TargetName} tools ${Headers})