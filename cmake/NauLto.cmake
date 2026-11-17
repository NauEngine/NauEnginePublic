# Link-time optimization (MSVC: /GL + /LTCG) for engine's own targets.
# Stage 1: Release configuration, Windows + MSVC (cl.exe) only. Third-party libraries are not affected.

set(NAU_LTO_ACTIVE OFF)

if(NAU_ENABLE_LTO)
    if(NOT Compiler_MSVC)
        message(WARNING "NAU_ENABLE_LTO: only MSVC (cl.exe) is supported for now, compiler: (${CMAKE_CXX_COMPILER_ID}). LTO disabled.")
    else()
        include(CheckIPOSupported)
        check_ipo_supported(RESULT _nauIpoSupported OUTPUT _nauIpoOutput LANGUAGES C CXX)
        if(_nauIpoSupported)
            set(NAU_LTO_ACTIVE ON)
            message(STATUS "NAU_ENABLE_LTO: link-time optimization enabled for Release")
        else()
            message(WARNING "NAU_ENABLE_LTO: not supported by toolchain: ${_nauIpoOutput}. LTO disabled.")
        endif()
    endif()
endif()

set(_NAU_LTO_EXCLUDE_DIRS
    "${CMAKE_SOURCE_DIR}/engine/3rdparty_libs"
    "${CMAKE_SOURCE_DIR}/engine/core/modules/ui/cocos2d-x"
)

function(nau_target_enable_lto target)
    get_target_property(targetSourceDir ${target} SOURCE_DIR)
    foreach(excludeDir ${_NAU_LTO_EXCLUDE_DIRS})
        cmake_path(IS_PREFIX excludeDir "${targetSourceDir}" NORMALIZE isThirdParty)
        if(isThirdParty)
            return()
        endif()
    endforeach()

    set_target_properties(${target} PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
endfunction()
