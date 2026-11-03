set(TargetName CameraViewportPreviewDX12)
add_executable(${TargetName} camera_viewport_preview.cpp)
target_link_libraries(${TargetName} PRIVATE SampleCommonLib Graphics Animation)
get_target_property(sceneModules SceneBaseSample MODULES_LIST)
nau_target_link_modules(${TargetName} ${sceneModules})
nau_add_compile_options(${TargetName})
# SceneBaseSample owns the shader-cache build rules for the assets mounted below.
add_dependencies(${TargetName} SceneBaseSample)
set_target_properties(${TargetName} PROPERTIES FOLDER "${NauEngineFolder}/tests")

set(runDir "${CMAKE_CURRENT_BINARY_DIR}/run/$<CONFIG>")
set(projectDir "${runDir}/samples/sceneBase")
get_filename_component(sceneDir "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
# SampleAppDelegate locates the project's CMakeLists.txt relative to the working directory.
# Keep the DX12 cache and imgui settings in the test tree, and mount sample assets read-only.
file(GENERATE OUTPUT "${projectDir}/CMakeLists.txt" CONTENT "# Test runtime project\n")
file(GENERATE OUTPUT "${runDir}/cache/.keep" CONTENT "")
configure_file(app.json.in app.json @ONLY)
file(GENERATE OUTPUT "${projectDir}/config/app.json" INPUT "${CMAKE_CURRENT_BINARY_DIR}/app.json")

add_test(NAME ${TargetName} COMMAND ${TargetName})
set_tests_properties(${TargetName} PROPERTIES
    WORKING_DIRECTORY "${runDir}"
    LABELS "graphics;dx12"
    FAIL_REGULAR_EXPRESSION "\\[Critical\\]"
    TIMEOUT 180
    RUN_SERIAL TRUE
)
