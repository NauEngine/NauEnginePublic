set(TargetName Shared)

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

if(BUILD_SHARED_LIBS)
    add_library(${TargetName} SHARED ${Files})
    target_compile_definitions(${TargetName} PRIVATE SHARED_EXPORT)
else()
    add_library(${TargetName} STATIC ${Files})
    target_compile_definitions(${TargetName} PRIVATE NAU_STATIC_RUNTIME)
endif()


target_link_libraries(${TargetName} PRIVATE
    nlohmann_json::nlohmann_json
    NauKernel
    usd
)

nau_add_compile_options(${TargetName})

target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/Shared/include> 
)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Sources})
set_target_properties(${TargetName} PROPERTIES
    FOLDER "${NauEngineFolder}/tools"
)

nau_install(${TargetName} tools ${Headers})