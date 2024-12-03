set(TargetName "ProjectTool")

nau_collect_files(Sources
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.cpp"
)

nau_collect_files(Public
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}
  MASK "*.h"
)

set(Files ${Sources} ${Public})
source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${Files})

if(BUILD_SHARED_LIBS)
  add_library(${TargetName} SHARED ${Files})
  target_compile_definitions(${TargetName} PRIVATE PROJECT_TOOL_EXPORT)
else()
  add_library(${TargetName} STATIC ${Files})
  target_compile_definitions(${TargetName} PRIVATE NAU_STATIC_RUNTIME)
endif()

set_target_properties(
  ${TargetName}
  PROPERTIES
  FOLDER "${NauEngineFolder}/tools"
)

target_sources(${TargetName} PRIVATE ${Sources} ${Public})

target_link_libraries(${TargetName} PRIVATE nlohmann_json::nlohmann_json NauKernel Shared)
target_include_directories(${TargetName} PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
  $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/tools/${TargetName}/include> 
)

nau_add_compile_options(${TargetName})

nau_install(${TargetName} tools ${Public})