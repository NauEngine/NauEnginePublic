# Console check of the Mgpu <-> Nau transfer bridge without Nau: GPU2 -> shared heap -> GPU1 -> CPU.
# Needs two distinct hardware adapters. Exit code: 0 = every transfer matched, 1 = mismatch/error, 2 = fewer than two adapters.
set(TargetName MgpuBridgeTransferTest)

add_executable(${TargetName} main.cpp)

target_link_libraries(${TargetName} PRIVATE Mgpu)

nau_add_compile_options(${TargetName})

# Same as the Mgpu module (A builds with ConformanceMode=false).
target_compile_options(${TargetName} PRIVATE /permissive)

set_target_properties(${TargetName} PROPERTIES
  FOLDER "${NauEngineFolder}/core/modules"
  VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:${TargetName}>"
)

add_custom_command(TARGET ${TargetName} POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:WinPixEventRuntime> $<TARGET_FILE_DIR:${TargetName}>)
