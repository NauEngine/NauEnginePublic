set(HEADERS
  dist/json/json-forwards.h
  dist/json/json.h
)

add_library(json
  dist/jsoncpp.cpp
  ${HEADERS})

set_target_properties(json
  PROPERTIES FOLDER "NauEngine/3rdparty/ozz/extern")

target_compile_definitions(json
  PUBLIC $<$<BOOL:${BUILD_SHARED_LIBS}>:JSON_DLL>
  PRIVATE $<$<BOOL:${BUILD_SHARED_LIBS}>:JSON_DLL_BUILD>)

set_target_properties(json
  PROPERTIES COMPILE_OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/wd4702>)
set_target_properties(json
  PROPERTIES COMPILE_OPTIONS $<$<CXX_COMPILER_ID:MSVC>:/wd4275>)

target_compile_options(json
  PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd4702>
  PUBLIC $<$<CXX_COMPILER_ID:MSVC>:/wd4275>)

target_include_directories(json PUBLIC
  $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/extern/jsoncpp/dist>
  $<INSTALL_INTERFACE:extern/jsoncpp/dist>)

nau_install(json 3rdPartyLibs ${HEADERS})