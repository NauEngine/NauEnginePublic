function(nau_conditional_set name value)
    if (NOT DEFINED ${name})
        set("${name}" ${value} PARENT_SCOPE)
    endif()
endfunction()

function(nau_in value values result)
    if (NOT DEFINED ${value})
        set("${result}" 0 PARENT_SCOPE)
        return()
    endif()
    list(FIND values ${${value}} find_result)
    if (NOT find_result EQUAL -1)
        set("${result}" 1 PARENT_SCOPE)
    else()
        set("${result}" 0 PARENT_SCOPE)
    endif()
endfunction()

#    add_nau_compile_options( [TARGETNAME <target>]
#    )
#
# The add_nau_compile_options() function adds necessary compile flags to target.
# add_nau_compile_options(<target> [STRICT])
function(nau_add_compile_options target)
    if (NOT TARGET ${target})
        message(FATAL_ERROR "Not a valid target (${target})")
    endif()

    get_target_property(targetType ${target} TYPE)
    if(${targetType} STREQUAL "INTERFACE_LIBRARY")
        return()
    endif()

    # if(${targetType} STREQUAL "EXECUTABLE")
    #     target_sources(${target} PRIVATE
    #         ${DAGOR_ROOT}/tools/util/natvis/dagor_types.natvis
    #         ${DAGOR_ROOT}/tools/util/natvis/EASTL.natvis
    #     )
    # endif()

    set(optionsArgs STRICT ENABLE_RTTI)
    cmake_parse_arguments(PARSE_ARGV 1 OPTIONS "${optionsArgs}" "" "")

    if (OPTIONS_STRICT)
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:${_C_STRICT_OPTIONS}>$<$<COMPILE_LANGUAGE:CXX>:${_CPP_STRICT_OPTIONS}>)
    else()
        target_compile_options(${target} PRIVATE $<$<COMPILE_LANGUAGE:C>:${_C_OPTIONS}>$<$<COMPILE_LANGUAGE:CXX>:${_CPP_OPTIONS}>)
    endif()

    if (NOT OPTIONS_ENABLE_RTTI)
      # inside _RTTI_OPTIONS we desable rtti, it's enabled by default
      target_compile_options(${target} PRIVATE ${_RTTI_OPTIONS})
    endif()

    target_include_directories(${target} PUBLIC ${_CPP_BASE_INCLUDES})
    target_compile_definitions(${target} PUBLIC ${_DEF_C_CPP_DEFINITIONS} NAU_TARGET_NAME="${target}")

endfunction()

##
function (nau_target_add_compile_options)
    set(optionalValueArgs STRICT ENABLE_RTTI)
    set(oneValueArgs)
    set(multiValueArgs TARGETS)
    cmake_parse_arguments(OPTIONS "${optionalValueArgs}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    foreach (target ${OPTIONS_TARGETS})
        if (OPTIONS_STRICT)
            if (OPTIONS_ENABLE_RTTI)
              nau_add_compile_options(${target} STRICT ENABLE_RTTI)
            else()
              nau_add_compile_options(${target} STRICT)
            endif()
        else()
            if (OPTIONS_ENABLE_RTTI)
              nau_add_compile_options(${target} ENABLE_RTTI)
            else()
              nau_add_compile_options(${target})
            endif()
        endif()
    endforeach()

endfunction()


#    add_nau_folder_property( [TARGETNAME <target>]
#         [LIB|EXE|SHARED <type>]
#    )
#
# The add_nau_folder_property() function adds target
# to folder NAU${NAU_CURRENT_CUSTOM_TARGET}/[Libs|Exe]
# in Visual Studio solution and adds it as dependency
# to ${NAU_CURRENT_CUSTOM_TARGET}[Libs|Exe].
function(add_nau_custom_target targetName)
    set(NAU_CURRENT_CUSTOM_TARGET ${targetName} PARENT_SCOPE)
    add_custom_target(${targetName})
    set_target_properties (${targetName} PROPERTIES
        FOLDER "${targetName}Folder"
    )
endfunction()


###    add_nau_folder_property( [TARGETNAME <target>]
###         [LIB|EXE|SHARED <type>]
###         [LIST <subdirectories>]
###    )
### @param _inp1 A positional argument
### The add_nau_folder_property() function adds target
### to folder NAU${NAU_CURRENT_CUSTOM_TARGET}/[Libs|Exe]
### in Visual Studio solution and adds it as dependency
### to ${NAU_CURRENT_CUSTOM_TARGET}[Libs|Exe].
function(nau_add_folder_property target type)

    if(DEFINED NAU_CURRENT_CUSTOM_TARGET)
        if (${type} MATCHES "(LIB|EXE|SHARED)")
            if (${type} STREQUAL "LIB")
                set(folder_suffix "Libs")
            elseif(${type} STREQUAL "EXE")
                set(folder_suffix "Exe")
            else()
                set(folder_suffix "Shared")
            endif()
        else()
            message(FATAL_ERROR "Target <type> should be equal to LIB, EXE or SHARED.")
        endif()

        if (DEFINED ARGV)
            foreach(subfolder IN LISTS ARGN)
                STRING(APPEND folder_suffix "/${subfolder}")
            endforeach()
        endif()
        add_dependencies(${NAU_CURRENT_CUSTOM_TARGET} ${target})
        set_target_properties (${target} PROPERTIES
            FOLDER "${NAU_CURRENT_CUSTOM_TARGET}Folder/${folder_suffix}"
        )
    else()
        message(FATAL_ERROR "No {NAU_CURRENT_CUSTOM_TARGET} provided. Specify what you building.")
    endif()
endfunction()


###     nau_collect_files(<variable>
###         [DIRECTORIES <directories>]
###         [RELATIVE <relative-path>]
###         [MASK <globbing-expressions>]
###         [EXCLUDE <regex-to-exclude>]
###    )
###
###  Generate a list of files from <directories> (traverse all the subdirectories) that match the <globbing-expressions> and store it into the <variable>
###  If RELATIVE flag is specified, the results will be returned as relative paths to the given path.
###  If EXCLUDE is specified, all paths that matches any <regex-to-exclude> willbe removed from result
function (nau_collect_files VARIABLE) 
  set(oneValueArgs RELATIVE PREPEND)
  set(multiValueArgs DIRECTORIES EXCLUDE INCLUDE MASK)
  cmake_parse_arguments(COLLECT "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

  if (COLLECT_EXCLUDE AND COLLECT_INCLUDE)
    message(FATAL_ERROR "nau_collect_files must not specify both EXCLUDE and INCLUDE parameters")
  endif()

  set (allFiles)

  foreach (dir ${COLLECT_DIRECTORIES})
    foreach (msk ${COLLECT_MASK})
      set(globExpr "${dir}/${msk}")

      if (COLLECT_RELATIVE)
        file (GLOB_RECURSE files RELATIVE ${COLLECT_RELATIVE} ${globExpr} )
      else()
        file (GLOB_RECURSE files ${globExpr})
      endif()

      list(APPEND allFiles ${files})
    endforeach()
  endforeach()

  if (COLLECT_EXCLUDE)
    foreach (re ${COLLECT_EXCLUDE})
      list (FILTER allFiles EXCLUDE REGEX ${re})
    endforeach()
  endif(COLLECT_EXCLUDE) # COLLECT_EXCLUDE

  if (COLLECT_INCLUDE)
    foreach (re ${COLLECT_INCLUDE})
        list (FILTER allFiles INCLUDE REGEX ${re})
    endforeach()
  endif(COLLECT_INCLUDE) # COLLECT_INCLUDE

  if (COLLECT_PREPEND)
    list(TRANSFORM allFiles PREPEND "${COLLECT_PREPEND}")
  endif()


  if (${VARIABLE})
    list(APPEND ${VARIABLE} ${allFiles})
    set(${VARIABLE} ${${VARIABLE}} PARENT_SCOPE)
  else()
    set(${VARIABLE} ${allFiles} PARENT_SCOPE)
  endif()

endfunction()

##
##
function(nau_collect_cmake_subdirectories VARIABLE SCAN_DIR)

  file(GLOB children RELATIVE ${SCAN_DIR} ${SCAN_DIR}/*)
  set(result "")
  foreach(child ${children})
    
    if(IS_DIRECTORY ${SCAN_DIR}/${child} AND (EXISTS ${SCAN_DIR}/${child}/CMakeLists.txt))
      list(APPEND result ${child})
    endif()
  endforeach()

  set(${VARIABLE} ${result} PARENT_SCOPE)

endfunction()

function(nau_install target subPath)
    message(NOTICE "Installing target ${target} ${subPath}")
    install(TARGETS ${target}
        EXPORT ${target}     
        RUNTIME DESTINATION "bin/$<CONFIG>"   
        ARCHIVE DESTINATION "lib/$<CONFIG>" 
        LIBRARY DESTINATION "lib/$<CONFIG>"
        PUBLIC_HEADER DESTINATION "include/${subPath}/${target}"
    )
    get_target_property(Target_BINARY_DIR ${target} BINARY_DIR)
    get_target_property(Target_SOURCE_DIR ${target} SOURCE_DIR)
    foreach(include ${ARGN})
        get_filename_component(dir ${include} DIRECTORY)
        cmake_path(IS_ABSOLUTE dir isDirAbs)
        if(${isDirAbs})
            cmake_path(IS_PREFIX Target_BINARY_DIR ${dir} isBinDir)
            cmake_path(IS_PREFIX Target_SOURCE_DIR ${dir} isSourceDir)
            if(${isBinDir})
                cmake_path(RELATIVE_PATH dir BASE_DIRECTORY ${Target_BINARY_DIR})
                INSTALL(FILES ${include} DESTINATION "include/${subPath}/${target}/${dir}")
            elseif(${isSourceDir})
                cmake_path(RELATIVE_PATH dir BASE_DIRECTORY ${Target_SOURCE_DIR})
                INSTALL(FILES ${include} DESTINATION "include/${subPath}/${target}/${dir}")
            else()
                message(FATAL_ERROR "Unsupported install directory!")
            endif()
        else()
            INSTALL(FILES ${include} DESTINATION "include/${subPath}/${target}/${dir}")
        endif()
    endforeach(include)

    install(EXPORT ${target} 
        FILE ${target}-config.cmake 
        DESTINATION "cmake/${subPath}/${target}"
    )

    get_target_property(targetType ${target} TYPE)
    get_target_property(targetNoInstallPDB ${target} NO_INSTALL_PDB)
    if((MSVC) AND
        NOT(
            ${targetType} STREQUAL "INTERFACE_LIBRARY" OR 
            ${targetType} STREQUAL "STATIC_LIBRARY" OR
            ${targetNoInstallPDB} STREQUAL "ON"
            ))

	    install(FILES $<TARGET_PDB_FILE:${target}> DESTINATION bin/$<CONFIG> OPTIONAL)
    endif()

endfunction(nau_install)

function(nau_setup_exe_name target name)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "${name}$<$<CONFIG:Debug>:-dev>$<$<CONFIG:MemorySanitizer>:-asan>")
endfunction()

function(nau_setup_output_path target path)
    set_target_properties(${target}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_DEBUG "${path}"
        RUNTIME_OUTPUT_DIRECTORY_RELEASE "${path}"
        RUNTIME_OUTPUT_DIRECTORY_MEMORYSANITIZER "${path}"
        LIBRARY_OUTPUT_DIRECTORY_DEBUG "${path}"
        LIBRARY_OUTPUT_DIRECTORY_RELEASE "${path}"
        LIBRARY_OUTPUT_DIRECTORY_MEMORYSANITIZER "${path}"
    )
endfunction()

function(nau_set_version NAU_VERSION_STRING)
    string(REPLACE "." ";" NAU_VERSIONS_NUMBERS ${NAU_VERSION_STRING})

    LIST(GET NAU_VERSIONS_NUMBERS 0 VERSION_MAJOR)
    LIST(GET NAU_VERSIONS_NUMBERS 1 VERSION_MINOR)
    LIST(GET NAU_VERSIONS_NUMBERS 2 VERSION_PATCH)

    set(NAU_VERSION_MAJOR ${VERSION_MAJOR} PARENT_SCOPE)
    set(NAU_VERSION_MINOR ${VERSION_MINOR} PARENT_SCOPE)
    set(NAU_VERSION_PATCH ${VERSION_PATCH} PARENT_SCOPE)

    add_compile_definitions(NAU_VERSION_MAJOR=${VERSION_MAJOR})
    add_compile_definitions(NAU_VERSION_MINOR=${VERSION_MINOR})
    add_compile_definitions(NAU_VERSION_PATCH=${VERSION_PATCH})
    add_compile_definitions(NAU_VERSION="${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_PATCH}")
endfunction()


###     nau_add_default_shaders(
###         <target_name>
###         <content_dir>
###         <shader_dir>
###         [INCLUDES additional_includes ...]
###    )
###
###  Adds a custom command to generate `shader_cache.nsbc`.
###  <shader_dir> must be relative path.
###  Expects that <content_dir>/<shader_dir> contains a `src`, `meta` and `include` folders.
###  Puts `shader_cache.nsbc` in `<content_dir>/<shader_dir>/cache` folder.
macro(nau_add_default_shaders target_name content_dir shader_dir)
    set(oneValueArgs)
    set(multiValueArgs INCLUDES)
    cmake_parse_arguments("SHADERS" "" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})
    set(additionalIncludes)
    set(additionalIncludesCommand)
    if(DEFINED SHADERS_INCLUDES)
        foreach(includePath ${SHADERS_INCLUDES})
            list(APPEND additionalIncludes "${includePath}")
            string(APPEND additionalIncludesCommand " -i \"${includePath}\"")
        endforeach()
    endif()

    nau_collect_files(ShadersSources
        DIRECTORIES ${content_dir}/${shader_dir}/src
        MASK "*.hlsl"
        EXCLUDE
        "/platform/.*"
    )
    nau_collect_files(ShadersIncludes
        DIRECTORIES ${content_dir}/${shader_dir}/include
        MASK "*.hlsli"
        EXCLUDE
        "/platform/.*"
    )
    nau_collect_files(ShadersMeta
        DIRECTORIES ${content_dir}/${shader_dir}/meta
        MASK "*.blk"
        EXCLUDE
        "/platform/.*"
    )

    source_group(shaders/src FILES ${ShadersSources})
    source_group(shaders/include FILES ${ShadersIncludes})
    source_group(shaders/meta FILES ${ShadersMeta})

    set(AllShaderFiles
        ${SourcesMaterials}
        ${ShadersSources}
        ${ShadersIncludes}
        ${ShadersMeta}
    )

    set_source_files_properties(${ShadersSources} PROPERTIES
        HEADER_FILE_ONLY TRUE
    )

    target_include_directories(${target_name} PRIVATE
        ${content_dir}/${shader_dir}/include
        ${additionalIncludes}
    )

    cmake_path(SET contentPath "${content_dir}")
    cmake_path(IS_ABSOLUTE contentPath isAbsolute)
    if (${isAbsolute})
        cmake_path(SET workingDir NORMALIZE ${content_dir})
    else()
        cmake_path(SET workingDir NORMALIZE ${CMAKE_CURRENT_SOURCE_DIR}/${content_dir})
    endif()

    add_custom_command(OUTPUT ${workingDir}/${shader_dir}/cache/shader_cache.nsbc
        DEPENDS ${ShadersSources} ${ShadersMeta} ${ShadersIncludes} ShaderCompilerTool
        COMMAND ${CMAKE_COMMAND} -E env
        "PYTHONPATH=${PXR_CMAKE_DIR}/lib/release/python;"
        "PATH=${PXR_CMAKE_DIR}/bin;${PXR_CMAKE_DIR}/lib;${PXR_CMAKE_DIR}/lib/release;${PXR_CMAKE_DIR}/python;${PXR_CMAKE_DIR}/plugin/usd;${PROJECT_SOURCE_DIR}/engine/3rdparty_libs/dxil/bin/x64"        
        $<TARGET_FILE:ShaderCompilerTool>
        -c shader_cache.nsbc
        -o ${shader_dir}/cache
        -s ${shader_dir}/src
        -m ${shader_dir}/meta
        -i "${shader_dir}/include"
        ${additionalIncludesCommand}
        $<$<CONFIG:Debug>:-De>
        WORKING_DIRECTORY ${workingDir}
    )

    target_sources(${target_name} PRIVATE
        ${AllShaderFiles}
        ${workingDir}/${shader_dir}/cache/shader_cache.nsbc
    )
endmacro()

