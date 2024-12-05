set(TargetName MaterialCreationTool)

nau_collect_files(Sources
    DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    MASK "*.cpp" "*.h"
)

add_executable(${TargetName} ${Sources})
target_link_libraries(${TargetName} PRIVATE
    NauFramework
)

nau_target_link_modules(${TargetName}
    CoreAssets
    GraphicsAssets
    CoreAssetFormats
)

nau_add_compile_options(${TargetName})

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Sources})
set_target_properties(${TargetName} PROPERTIES
    FOLDER "${NauEngineFolder}/tools"
)

nau_install(${TargetName} tools)