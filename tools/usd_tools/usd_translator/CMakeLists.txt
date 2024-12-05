set(TargetName UsdTranslator)

set(Sources
    animation/usd_animation_clip_adapter.cpp
    animation/usd_animation_clip_composer.cpp
    usd_audio_adapter.cpp
    usd_component_adapter.cpp
    usd_mesh_adapter.cpp
    usd_mesh_asset_adapter.cpp
    usd_mesh_container.cpp
    usd_prim_adapter.cpp
    usd_prim_translator.cpp
    usd_physics_adapter.cpp
    usd_physics_rigid_body_sphere.cpp
    usd_physics_rigid_body_cube.cpp
    usd_physics_rigid_body_cylinder.cpp
    usd_physics_rigid_body_capsule.cpp
    usd_physics_rigid_body_convex_hull.cpp
    usd_physics_rigid_body_mesh.cpp
    usd_scene_component_adapter.cpp
    usd_stage_translator.cpp
    usd_mesh_composer.cpp
    usd_vfx_adapter.cpp

    ui_translator/usd_ui_translator.cpp
    ui_translator/usd_ui_prim_adapter.cpp
    ui_translator/usd_ui_draw_node_adapter.cpp
    ui_translator/usd_ui_label_adapter.cpp
    ui_translator/usd_ui_button_adapter.cpp
    ui_translator/usd_ui_scroll_adapter.cpp

    ui_translator/usd_ui_layer_adapter.cpp
    ui_translator/usd_ui_sprite_adapter.cpp
    ui_translator/usd_ui_node_adapter.cpp
)

set(Headers
    animation/usd_animation_clip_adapter.h
    animation/usd_animation_clip_composer.h
    usd_audio_adapter.h
    usd_component_adapter.h
    usd_prim_adapter.h
    usd_prim_translator.h
    usd_physics_adapter.h
    usd_physics_rigid_body_sphere.h
    usd_physics_rigid_body_cube.h
    usd_physics_rigid_body_cylinder.h
    usd_physics_rigid_body_capsule.h
    usd_physics_rigid_body_convex_hull.h
    usd_physics_rigid_body_mesh.h
    usd_scene_component_adapter.h
    usd_stage_translator.h
    usd_mesh_adapter.h
    usd_mesh_asset_adapter.h
    usd_mesh_container.h
    usd_translator.h
    usd_translator_api.h
    usd_mesh_composer.h
    usd_vfx_adapter.h

    ui_translator/usd_ui_translator.h
    ui_translator/usd_ui_prim_adapter.h
    ui_translator/usd_ui_draw_node_adapter.h
    ui_translator/usd_ui_label_adapter.h
    ui_translator/usd_ui_button_adapter.h
    ui_translator/usd_ui_scroll_adapter.h
    ui_translator/usd_ui_layer_adapter.h
    ui_translator/usd_ui_sprite_adapter.h
    ui_translator/usd_ui_node_adapter.h
)

source_group("Sources" FILES ${Sources})
source_group("Headers" FILES ${Headers})

add_library(${TargetName} SHARED ${Sources} ${Headers})

cmake_path(GET CMAKE_CURRENT_SOURCE_DIR PARENT_PATH SOURCE_BASE_DIR)
target_compile_definitions(${TargetName}  PUBLIC NOMINMAX $<$<CONFIG:DEBUG>:TBB_USE_DEBUG=0> )
target_link_options(${TargetName}  PUBLIC $<$<CXX_COMPILER_ID:MSVC>:${PXR_NODEFAULTLIBS}>)
target_compile_definitions(${TargetName}  PRIVATE USD_TRANSLATOR_EXPORT)
target_include_directories(${TargetName}
    PUBLIC 
    $<BUILD_INTERFACE:${SOURCE_BASE_DIR}>
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools> 
    PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/../plugin/animation_clip/generated_src/schema_plugins"
)

target_link_libraries(${TargetName} 
    nlohmann_json::nlohmann_json
    usd
    usdUtils
    usdGeom
    usdSkel

    NauAnimationClipAsset

    Graphics
	Animation
    Audio
    NauKernel
    CoreScene
    CoreAssets
    AssetTool

    Physics
    PhysicsJolt

    ui

    VFX
 
    UsdProxy
    UsdMetaTools
    UsdHelpers

    NauAudioSchema
    NauVFXSchema
    NauGuiSchema
    NauPhysicsSchema
)

nau_add_compile_options(${TargetName})

set_target_properties (${TargetName} PROPERTIES
    FOLDER "Editor/Usd"
)

if (NAU_CORE_TESTS)
	add_subdirectory(tests)
endif()

install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/
    DESTINATION ${CMAKE_INSTALL_PREFIX}/include/tools/usd_translator
    FILES_MATCHING 
      PATTERN "*.h"
      PATTERN "*.hpp"
      PATTERN "*.ipp"
)

nau_install(${TargetName} tools)