define_property(TARGET
    PROPERTY NAU_MODULES_LINKED_TARGETS
    BRIEF_DOCS "Linked modules"
    FULL_DOCS "All modules (tagets) that must be linked in"
)

define_property(TARGET
    PROPERTY NAU_MODULES_LIST
    BRIEF_DOCS "Modules list"
    FULL_DOCS "All used modules names"
)

define_property(TARGET
    PROPERTY NAU_MODULE_API_TARGET
    BRIEF_DOCS "Module API Target"
    FULL_DOCS "Module API Target"
)

##
##
macro(nau_write_static_modules_initialization resVariable)
    if (${BUILD_SHARED_LIBS})
        message(FATAL_ERROR "Static runtime configuration expected")
    endif ()

    set(GEN_PATH ${CMAKE_CURRENT_BINARY_DIR}/generated_static_modules_initialization.cpp)

    set(entryContent)
    string(APPEND entryContent "// clang-format off\n")
    string(APPEND entryContent "// automatically generated code, do not manually modify\n")
    string(APPEND entryContent "//\n")
    string(APPEND entryContent "// Copyright (c) N-GINN LLC., 2023-2025. All rights reserved.\n")
    string(APPEND entryContent "//\n\n")

    string(APPEND entryContent "#include \"nau/module/module_manager.h\"\n\n")
    string(APPEND entryContent "namespace nau {\n")
    string(APPEND entryContent "  struct IModule\;\n")
    string(APPEND entryContent "} //namespace nau\n\n")

    foreach (module ${ARGN})
        string(APPEND entryContent "extern eastl::shared_ptr<nau::IModule> createModule_${module}()\;\n")
    endforeach ()

    string(APPEND entryContent "\n\nnamespace nau::module_detail\n")
    string(APPEND entryContent "{\n")
    string(APPEND entryContent "  void initializeAllStaticModules(nau::IModuleManager* manager)\n  {\n")

    foreach (module ${ARGN})
        string(APPEND entryContent "    eastl::shared_ptr<nau::IModule> module_${module} = createModule_${module}()\;\n")
        string(APPEND entryContent "    manager->registerModule(\"${module}\", module_${module})\;\n")
    endforeach ()

    string(APPEND entryContent "  }\n\n")
    string(APPEND entryContent "} //namespace nau::module_detail \n\n")

    string(APPEND entryContent "// clang-format on\n")

    file(WRITE ${GEN_PATH}.tmp ${entryContent})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GEN_PATH}.tmp ${GEN_PATH})

    set(${resVariable} ${GEN_PATH})
endmacro()


##
##
macro(nau_generate_module_config_file targetName)
    message(NOTICE "nau_generate_module_config_file ${targetName}")
    string(TOUPPER ${targetName} moduleUpper)

    set(GEN_PATH ${CMAKE_CURRENT_BINARY_DIR}/generated_${targetName}_config.h)

    set(entryContent)
    string(APPEND entryContent "// clang-format off\n")
    string(APPEND entryContent "// automatically generated code, do not manually modify\n")
    string(APPEND entryContent "//\n")
    string(APPEND entryContent "// Copyright (c) N-GINN LLC., 2023-2025. All rights reserved.\n")
    string(APPEND entryContent "//\n\n")
    string(APPEND entryContent "#pragma once\n\n")

    string(APPEND entryContent "#ifdef NAU_${moduleUpper}_EXPORT\n")
    string(APPEND entryContent "#undef NAU_${moduleUpper}_EXPORT\n")
    string(APPEND entryContent "#endif\n\n")

    string(APPEND entryContent "#if !defined(NAU_STATIC_RUNTIME)\n")
    string(APPEND entryContent "    #ifdef _MSC_VER\n")
    string(APPEND entryContent "        #ifdef NAU_${moduleUpper}_BUILD\n")
    string(APPEND entryContent "            #define NAU_${moduleUpper}_EXPORT __declspec(dllexport)\n")
    string(APPEND entryContent "        #else\n")
    string(APPEND entryContent "            #define NAU_${moduleUpper}_EXPORT __declspec(dllimport)\n")
    string(APPEND entryContent "        #endif\n\n")
    string(APPEND entryContent "    #else\n")
    string(APPEND entryContent "        #error Unknown Compiler/OS\n")
    string(APPEND entryContent "    #endif\n\n")
    string(APPEND entryContent "#else\n")
    string(APPEND entryContent "    #define NAU_${moduleUpper}_EXPORT\n")
    string(APPEND entryContent "#endif\n\n")

    string(APPEND entryContent "// clang-format on\n\n")

    #set(GEN_PATH_TMP ${GEN_PATH}.tmp)
    #file(WRITE ${GEN_PATH_TMP} ${entryContent})
    #execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GEN_PATH_TMP} ${GEN_PATH})
    #file(REMOVE ${GEN_PATH_TMP})

    file(WRITE ${GEN_PATH}.tmp ${entryContent})
    execute_process(COMMAND ${CMAKE_COMMAND} -E copy_if_different ${GEN_PATH}.tmp ${GEN_PATH})
    target_sources(${targetName} PRIVATE ${GEN_PATH})

    target_precompile_headers(${targetName} PUBLIC
        $<BUILD_INTERFACE:${GEN_PATH}>
        $<INSTALL_INTERFACE:generated_${targetName}_config.h>
    )
   
    target_include_directories(${targetName} PUBLIC
        $<INSTALL_INTERFACE:$<INSTALL_PREFIX>/include/generated/${targetName}>
    )

    target_compile_definitions(${targetName} PUBLIC
        "NAU_${moduleUpper}_EXPORT="
    )

    install(FILES ${GEN_PATH} DESTINATION "include/generated/${targetName}")

    message(NOTICE "nau_generate_module_config_file finished")
endmacro()


##
##
macro(nau_check_linked_modules_target)
    if (NOT TARGET NauLinkedModules)
        add_library(NauLinkedModules INTERFACE)
        set_property(TARGET NauLinkedModules APPEND PROPERTY EXPORT_PROPERTIES
            NAU_MODULES_LIST
            NAU_MODULES_LINKED_TARGETS
        )

        install(TARGETS NauLinkedModules
            EXPORT NauLinkedModulesTargets
        )

        export(EXPORT NauLinkedModulesTargets
            FILE ${CMAKE_BINARY_DIR}/cmake/NauLinkedModulesTargets.cmake
        )
    endif ()
endmacro()

macro(nau_check_modules_api_target)

    if (NOT TARGET NauModulesApi)
        add_library(NauModulesApi INTERFACE)

        install(TARGETS NauModulesApi
            EXPORT NauModulesApiTargets
        )

        export(EXPORT NauModulesApiTargets
            FILE ${CMAKE_BINARY_DIR}/cmake/NauModulesApiTargets.cmake
        )
    endif ()

endmacro()


##
##
function(nau_add_module TargetName)
    message(NOTICE "nau_add_module ${TargetName}")
    set(optionalValueArgs PRIVATE EXPORT_AS_LIBRARY ENABLE_RTTI)
    set(multiValueArgs SOURCES)
    set(singleValueArgs FOLDER INTERFACE_TARGET)

    cmake_parse_arguments(MODULE "${optionalValueArgs}" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    add_library(${TargetName} ${MODULE_SOURCES})

    target_compile_definitions(${TargetName} PRIVATE
        NAU_MODULE_NAME=${TargetName}
        NAU_MODULE_BUILD
    )

#    target_include_directories(${TargetName}
#        INTERFACE
#        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/${TargetName}/generated>
#    )

    if (MODULE_ENABLE_RTTI)
        nau_add_compile_options(${TargetName} ENABLE_RTTI)
    else()
        nau_add_compile_options(${TargetName})
    endif()

    if (NOT DEFINED MODULE_FOLDER)
        set(MODULE_FOLDER "modules")
    endif ()

    set_target_properties(
        ${TargetName}
        PROPERTIES
        FOLDER ${MODULE_FOLDER}
    )

    if (${MODULE_EXPORT_AS_LIBRARY})
        target_link_libraries(${TargetName} PUBLIC NauKernel)

        message(STATUS "Module (${TargetName}) will export as API library")

        if (MODULE_INTERFACE_TARGET)
            message(AUTHOR_WARNING "module: (${TargetName}) specify both 'EXPORT_AS_LIBRARY' and 'INTERFACE_TARGET'")
        endif ()

        nau_generate_module_config_file(${TargetName})
    else ()
        target_link_libraries(${TargetName} PRIVATE NauKernel)
    endif () #MODULE_EXPORT_AS_LIBRARY


    if (MODULE_INTERFACE_TARGET)
        add_library(${MODULE_INTERFACE_TARGET} INTERFACE)
        set_target_properties(
            ${MODULE_INTERFACE_TARGET}
            PROPERTIES
            FOLDER ${MODULE_FOLDER}
            NAU_MODULE_ASSOCIATION ${TargetName}
        )

        target_link_libraries(${TargetName} PRIVATE ${MODULE_INTERFACE_TARGET})
        set_property(TARGET ${TargetName}
            PROPERTY NAU_MODULE_API_TARGET ${MODULE_INTERFACE_TARGET}
        )

        set_property(TARGET ${TargetName} APPEND PROPERTY EXPORT_PROPERTIES NAU_MODULE_API_TARGET)

        if (NOT MODULE_PRIVATE)
            nau_check_modules_api_target()

#            install(TARGETS ${MODULE_INTERFACE_TARGET} EXPORT NauModulesApiTargets )
        endif ()
    endif () # MODULE_INTERFACE_TARGET

    if (NOT ${MODULE_PRIVATE})
        nau_check_linked_modules_target()

        get_target_property(IsImported NauLinkedModules IMPORTED)

        if (NOT BUILD_SHARED_LIBS OR MODULE_EXPORT_AS_LIBRARY)
            if (NOT IsImported)
#                install(TARGETS ${TargetName} EXPORT NauLinkedModulesTargets )
            endif ()

            set_property(TARGET NauLinkedModules
                APPEND PROPERTY NAU_MODULES_LINKED_TARGETS
                ${TargetName}
            )

        endif () # (static linkage) or (export as library)

        set_property(TARGET NauLinkedModules
            APPEND PROPERTY NAU_MODULES_LIST
            ${TargetName}
        )
    endif ()
    message(NOTICE "nau_add_module finished")
endfunction()


##
##
function(nau_target_link_modules TargetName)
    foreach (module ${ARGN})
        if (NOT TARGET ${module})
            message(AUTHOR_WARNING "Module (${module}) expected to be target")
        else ()
            get_target_property(moduleApiTarget ${module} NAU_MODULE_API_TARGET)
            if (moduleApiTarget AND TARGET ${moduleApiTarget})
                target_link_libraries(${TargetName} PRIVATE ${moduleApiTarget})
            endif ()

            set(moduleApiTarget)
        endif ()
    endforeach ()

    set(allModules ${ARGN})
    if (${BUILD_SHARED_LIBS})
        add_dependencies(${TargetName} ${allModules})
    else ()

        nau_write_static_modules_initialization(modulesInitCppPath ${allModules})
        target_sources(${TargetName} PRIVATE ${modulesInitCppPath})
        target_link_libraries(${TargetName} PRIVATE ${allModules})
    endif ()

    list(JOIN allModules "," commaSeparatedModulesList)
    target_compile_definitions(${TargetName} PRIVATE
        -DNAU_MODULES_LIST="${commaSeparatedModulesList}"
    )
    set_target_properties(${TargetName}
        PROPERTIES
        MODULES_LIST "${allModules}"
    )

    #set(GEN_PATH ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/generated_modules_list_${TargetName})
    #file(WRITE ${GEN_PATH} ${linkedModules})
endfunction()


##
##
function(nau_target_link_public_modules TargetName)
    if (NOT TARGET NauLinkedModules)
        message(STATUS "No modules to link")
        return()
    endif ()

    # Modules list are specified all public modules.
    # But not all modules must be linked to the target:
    # only modules that are 'exported as library' or
    # if not BUILD_SHARED_LIBS (in that case all modules must be directly linked with target).
    # All modules are accessible through NAU_MODULES_LIST property
    get_property(modulesList
        TARGET NauLinkedModules
        PROPERTY NAU_MODULES_LIST
    )

    list(JOIN modulesList "," commaSeparatedModulesList)
    target_compile_definitions(${TargetName} PRIVATE
        -DNAU_MODULES_LIST="${commaSeparatedModulesList}"
    )

    # Also automatically link all 'module API' targets, that can be
    # associated with module (if INTERFACE_TARGET was specied for nau_add_module).
    foreach (module ${modulesList})
        if (TARGET ${module})
            get_property(moduleApiTarget TARGET ${module} PROPERTY NAU_MODULE_API_TARGET)
            if (moduleApiTarget)
                if (TARGET ${moduleApiTarget})
                    target_link_libraries(${TargetName} PRIVATE ${moduleApiTarget})
                else ()
                    message(AUTHOR_WARNING "Module (${module}) are specified additional api through (${moduleApiTarget}) that is not a target")
                endif ()

                set(moduleApiTarget)
            endif ()
        endif ()
    endforeach ()


    # Even if BUILD_SHARED_LIBS there is can be modules
    # that are exported as libraries and should to be linked with target (or other modules).
    # Such modules are accessible through NAU_MODULES_LINKED_TARGETS property
    get_property(modulesLinkedTargets
        TARGET NauLinkedModules
        PROPERTY NAU_MODULES_LINKED_TARGETS
    )

    foreach (linkedModuleTarget ${modulesLinkedTargets})
        if (NOT TARGET ${linkedModuleTarget})
            message(AUTHOR_WARNING "Module (${linkedModuleTarget}) expected to be target")
        else ()
            target_link_libraries(${TargetName} PRIVATE ${linkedModuleTarget})
            message(STATUS "Link with module (${linkedModuleTarget}) library.")
        endif ()
    endforeach ()

    if (NOT BUILD_SHARED_LIBS)
        nau_write_static_modules_initialization(modulesInitCppPath ${modulesList})
        target_sources(${TargetName} PRIVATE ${modulesInitCppPath})
    endif ()
endfunction()
