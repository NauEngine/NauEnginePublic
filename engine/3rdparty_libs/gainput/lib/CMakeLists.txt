project(gainput)
message(STATUS "GAINPUT version ${GAINPUT_VERSION}")

set(CMAKE_MACOSX_RPATH 1)

if(CMAKE_COMPILER_IS_GNUCXX)
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++98 -Wall -Wextra -pedantic -Wshadow -Wno-variadic-macros")
endif()

include_directories (include/)

file(GLOB_RECURSE sources source/*.cpp source/*.h include/*.h)

if(APPLE)
	file(GLOB_RECURSE mmsources source/*.mm)
endif()

## build STATIC *or* SHARED
if (GAINPUT_BUILD_SHARED)
  message(STATUS "..Building shared libraries (-DGAINPUT_BUILD_SHARED=OFF to disable)")
  add_library(gainput SHARED ${sources} ${mmsources})

  target_include_directories(gainput PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/3rdPartyLibs/gainput/include>
  )

  set_target_properties(gainput PROPERTIES
    OUTPUT_NAME gainput
    DEBUG_POSTFIX -d
    VERSION ${GAINPUT_VERSION}
    SOVERSION ${GAINPUT_MAJOR_VERSION}
    FOLDER "${NauEngineFolder}/gainput"
  )
  set(install_libs ${install_libs} gainput)
endif (GAINPUT_BUILD_SHARED)

if (GAINPUT_BUILD_STATIC)
  message(STATUS "..Building static libraries (-DGAINPUT_BUILD_STATIC=OFF to disable)")
  add_library(gainputstatic STATIC ${sources} ${mmsources})

  target_include_directories(gainputstatic PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>  
    $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/3rdPartyLibs/gainput/include>
  )

  set_target_properties(gainputstatic PROPERTIES 
     DEBUG_POSTFIX -d 
     FOLDER "${NauEngineFolder}/gainput"
  )
  set(install_libs ${install_libs} gainputstatic)
endif (GAINPUT_BUILD_STATIC)

if(WIN32)
  if (GAINPUT_BUILD_SHARED)
  	target_link_libraries(gainput ${XINPUT} ws2_32)
	add_definitions(-DGAINPUT_LIB_DYNAMIC=1)
  endif (GAINPUT_BUILD_SHARED)
  if (GAINPUT_BUILD_STATIC)
	target_link_libraries(gainputstatic ${XINPUT} ws2_32)
  endif (GAINPUT_BUILD_STATIC)
elseif(ANDROID)
	target_link_libraries(gainputstatic native_app_glue log android)
	target_link_libraries(gainput native_app_glue log android)
elseif(APPLE)
	find_library(FOUNDATION Foundation)
	find_library(IOKIT IOKit)
  find_library(GAME_CONTROLLER GameController)
	target_link_libraries(gainput ${FOUNDATION} ${IOKIT} ${GAME_CONTROLLER})
  if(IOS)
      find_library(UIKIT UIKit)
      find_library(COREMOTION CoreMotion)
      find_library(QUARTZCORE QuartzCore)
      target_link_libraries(gainput ${UIKIT} ${COREMOTION})
  else()
      find_library(APPKIT AppKit)
      target_link_libraries(gainput ${APPKIT})
  endif()
endif()

# Library installation directory
if(NOT DEFINED CMAKE_INSTALL_LIBDIR)
    set(CMAKE_INSTALL_LIBDIR lib)
endif(NOT DEFINED CMAKE_INSTALL_LIBDIR)
set(libdir ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR})

nau_collect_files(HEADERS
  RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
  DIRECTORIES
    ${CMAKE_CURRENT_SOURCE_DIR}/include
  MASK "*.h" "*.hpp" "*.ipp"
)

if (GAINPUT_BUILD_SHARED)
  nau_install(gainput 3rdPartyLibs ${HEADERS})
endif (GAINPUT_BUILD_SHARED)

if (GAINPUT_BUILD_STATIC)
  nau_install(gainputstatic 3rdPartyLibs ${HEADERS})
endif (GAINPUT_BUILD_STATIC)
