
set(TargetName test_usd_translator)

set(SOURCES
    main.cpp
    test_rigid_body_sizes.cpp
)

add_executable(${TargetName} ${SOURCES})

target_include_directories(${TargetName} PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}
)

# Advance the production world synchronizer directly, without a concurrent
# PhysicsService, so ray queries and pause/resume assertions are deterministic.
get_target_property(PhysicsSourceDir Physics SOURCE_DIR)
target_sources(${TargetName} PRIVATE "${PhysicsSourceDir}/src/physics_world_state.cpp")
target_include_directories(${TargetName} PRIVATE "${PhysicsSourceDir}/src")
target_compile_definitions(${TargetName} PRIVATE
  NAU_TEST_USD_PLUGINS="$<TARGET_FILE_DIR:NauPhysicsSchema>"
  NAU_TEST_NATIVE_SCENE="${CMAKE_CURRENT_SOURCE_DIR}/data/collider_sizes.nausd_scene"
)
add_dependencies(${TargetName} NauUsdFormat)

target_link_libraries(${TargetName} PRIVATE
  gtest
  gmock
  UsdTranslator
  NauFramework
  CoreAssets
  Physics
)

nau_add_compile_options(${TargetName})
nau_target_link_modules(${TargetName} CoreScene CoreAssets CoreAssetFormats PhysicsJolt)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${SOURCES})
set_target_properties (${TargetName} PROPERTIES
    FOLDER "Editor/tests"
)


if (WIN32)
    set(TestRuntimePath "$<TARGET_FILE_DIR:${TargetName}>;$<TARGET_FILE_DIR:NauPhysicsSchema>;${PXR_ENV_PATHS};$ENV{PATH}")
    set(TestPluginPath "$<TARGET_FILE_DIR:NauPhysicsSchema>;${PXR_CMAKE_DIR}/plugin/usd;${PXR_CMAKE_DIR}/plugin/$<IF:$<CONFIG:Debug>,debug,release>/usd")
    add_test(NAME UsdTranslator COMMAND ${CMAKE_COMMAND} -E env
        "PATH=${TestRuntimePath}"
        "PXR_PLUGINPATH_NAME=${TestPluginPath}"
        $<TARGET_FILE:${TargetName}>)
    set_target_properties(${TargetName} PROPERTIES VS_DEBUGGER_ENVIRONMENT
        "PATH=${TestRuntimePath}\nPXR_PLUGINPATH_NAME=${TestPluginPath}")
else()
    add_test(NAME UsdTranslator COMMAND ${TargetName})
endif()
set_tests_properties(UsdTranslator PROPERTIES TIMEOUT 180 RUN_SERIAL TRUE
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TargetName}>")
