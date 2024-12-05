# Setup for Visual C/C++ compiler for Win64
# Compiler_Cl:      cl, clang-cl
# Compiler_MSVC:    cl only
# Compiler_ClangCl: clang-cl only
# Compiler_Clang:   currently not supported

set(Platform_Windows ON)

if(${Host_Arch} STREQUAL x64)
    set(Platform_Win64 ON)
elseif(${Host_Arch} STREQUAL x86)
    set(Platform_Win32 ON)
else()
    message(FATAL_ERROR "Unknown microsoft target platform:(${Platform})")
endif()

if(MSVC) # MSVC: cl.exe, clang-cl.exe
    set(Compiler_Cl ON)

    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(Compiler_MSVC ON)
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(Compiler_ClangCl ON)
    else()
        message(FATAL_ERROR "Unsupported compiler:(${CMAKE_CXX_COMPILER_ID}) on (${CMAKE_SYSTEM_NAME})")
    endif()
else()
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(Compiler_Clang ON)
    endif()
    #message(FATAL_ERROR "Unsupported compiler:(${CMAKE_CXX_COMPILER_ID}). Currently supported only msvc like compilers")
endif()

#execute_process(
#    COMMAND ${CMAKE_COMMAND} --help-variable-list
#    OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/cmake_var_full_list.txt"
#)

#file(STRINGS "${CMAKE_CURRENT_BINARY_DIR}/cmake_var_full_list.txt" VAR_FULL_LIST)
#foreach(var ${VAR_FULL_LIST})
#    if("${var}" MATCHES "<CONFIG>")
#        if("${var}" MATCHES "<LANG>")
#            foreach(lang C CXX)
#                # (supported languages list from https://cmake.org/cmake/help/latest/command/project.html)
#                string(REPLACE "<LANG>" "${lang}" lang_var "${var}")
#                list(APPEND CONFIG_VAR_LIST "${lang_var}")
#            endforeach()
#        else()
#            list(APPEND CONFIG_VAR_LIST "${var}")
#        endif()
#    endif()
#endforeach()
#unset(VAR_FULL_LIST)

#function(copy_configuration_type config_from config_to)
#    string(TOUPPER "${config_from}" config_from)
#    string(TOUPPER "${config_to}" config_to)
#    foreach(config_var ${CONFIG_VAR_LIST})
#        string(REPLACE "<CONFIG>" "${config_from}" config_var_from "${config_var}")
#        string(REPLACE "<CONFIG>" "${config_to}"   config_var_to   "${config_var}")
#        set("${config_var_to}" "${${config_var_from}}" PARENT_SCOPE)
#    endforeach()
#endfunction()

#copy_configuration_type(RELEASE RELWITHDEBINFO)

set(CMAKE_INSTALL_DEBUG_LIBRARIES TRUE)
set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_PREFIX}/bin/$<CONFIG>)
include(InstallRequiredSystemLibraries)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)


set(_DEF_C_CPP_DEFINITIONS_DEBUG)
set(_CONFIG_OPTIONS_RELWITHDEBINFO
    _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
    NDEBUG=1
)
set(_DEF_C_CPP_DEFINITIONS_RELEASE
    NDEBUG=1
    _SECURE_SCL=0
    _DISABLE_CONSTEXPR_MUTEX_CONSTRUCTOR
)

set(_DEF_C_CPP_DEFINITIONS
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _CRT_SECURE_NO_WARNINGS
    $<$<CONFIG:Debug>:${_DEF_C_CPP_DEFINITIONS_DEBUG}>
    $<$<CONFIG:Release>:${_DEF_C_CPP_DEFINITIONS_RELEASE}>
    $<$<CONFIG:RelWithDebInfo>:${_CONFIG_OPTIONS_RELWITHDEBINFO}>
)

if(MSVC)
    if(NOT BUILD_SHARED_LIBS)
        set(CrtRuntime_DEBUG /MTd)
        set(CrtRuntime_RELEASE /MT)
        set(CrtRuntime_RELWITHDEBINFO /MT)
    else()
        set(CrtRuntime_DEBUG /MDd)
        set(CrtRuntime_RELEASE /MD)
        set(CrtRuntime_RELWITHDEBINFO /MD)
    endif()

    set(_DEF_C_CPP_OPTIONS
        /c
        /nologo
        /Zc:forScope
        /Zc:inline
        /Zc:wchar_t
        /Zp8
        /J
        /JMC
        /bigobj
        $<$<CONFIG:Debug>:${CrtRuntime_DEBUG}>
        $<$<CONFIG:Release>:${CrtRuntime_RELEASE}>
        $<$<CONFIG:RelWithDebInfo>:${CrtRuntime_RELWITHDEBINFO}>
    )

    if(BUILD_DEBUG_WITH_ASAN)
        list(APPEND _DEF_C_CPP_OPTIONS /fsanitize=address)
    endif()
endif()

if(Compiler_MSVC)
    list(APPEND _DEF_C_CPP_OPTIONS
        /Zc:preprocessor # preprocessor conformance mode (https://learn.microsoft.com/en-us/cpp/preprocessor/preprocessor-experimental-overview?view=msvc-170)
        /MP  #enable multi processor compilation (which is used only for cl)
    )
endif()


STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG}")
STRING(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")


#if ((DEFINED StrictCompile) AND (${StrictCompile}))
#  suppressed warnigs list (marked with x are not suppressed now):
#   C4265  'XXX' : class has virtual functions, but destructor is not virtual
#   C4127  conditional expression is constant
# x C4100  'XXX' : unreferenced formal parameter
# x   981  operands are evaluated in unspecified order
# x   383  value copied to temporary, reference to temporary used
# x   310  old-style parameter list (anachronism)
# x   174  expression has no effect
# x   111  statement is unreachable
# x   271  trailing comma is nonstandard
#   C4514  'XXX' : unreferenced inline function has been removed
#   C4061  enumerator 'XXX' in switch of enum 'YYY' is not explicitly handled by a case label
#   C4062  enumerator 'XXX' in switch of enum 'YYY' is not handled
#   C4820  'XXX' : 'N' bytes padding added after data member 'XXX::YYY'
#   C4324  'XXX' : structure was padded due to __declspec(align())
#   C4668  'XXX' is not defined as a preprocessor macro, replacing with '0' for '#if/#elif'
#   C4619  pragma warning : there is no warning number 'NNN'
#   C4365  'XXX' : conversion from 'TYPE1' to 'TYPE2', signed/unsigned mismatch
#   C4302  'type cast' : truncation from 'TYPE1' to 'TYPE2'
#   C4244  '=' : conversion from 'TYPE1' to 'TYPE2', possible loss of data
#   C4242  'argument' : conversion from 'TYPE1' to 'TYPE2', possible loss of data
#   C4245  'argument' : conversion from 'TYPE1' to 'TYPE2', signed/unsigned mismatch
#   C4101  'XXX' : unreferenced local variable
#   C4201  nonstandard extension used : nameless struct/union
#   C4625  'XXX' : copy constructor could not be generated because a base class copy constructor is inaccessible
#   C4626  'XXX' : assignment operator could not be generated because a base class assignment operator is inaccessible
#   C4800  'int' : forcing value to bool 'true' or 'false' (performance warning)
#   C4018  '<' : signed/unsigned mismatch
#   C4710  'XXX' : function not inlined
#   C4291  'void *operator new(size_t,IMemAlloc *)' : no matching operator delete found; memory will not be freed if initialization throws an exception
#   C4389  '!=' : signed/unsigned mismatch
#   C4200  nonstandard extension used : zero-sized array in struct/union
#   C4255  'XXX' : no function prototype given: converting '()' to '(void)'
#   C4711  function 'XXX' selected for automatic inline expansion
#   C4355  'this' : used in base member initializer list
#   C4640  'XXX' : construction of local static object is not thread-safe
# x C4714  function 'XXX' marked as __forceinline not inlined
#   C4305  'initializing' : truncation from 'double' to 'real'
#   C4511  'XXX' : copy constructor could not be generated
#   C4512  'XXX' : assignment operator could not be generated
#   C4305  'XXX' : truncation from 'double' to 'float'
#   C4738  storing 32-bit float result in memory, possible loss of performance
#   C4996  'stricmp' was declared deprecated
#   C4740  macro redefinition
#   C4702  unreachable code
#   C4826: Conversion from 'const void *' to 'void * __ptr64' is sign-extended. This may cause unexpected runtime behavior.
#   C4503  decorated name length exceeded, name was truncated
#   C4748: /GS can not protect parameters and local variables from local buffer overrun because optimizations are disabled in function
#   C4987: nonstandard extension used: 'throw (...)'
#   C4574: '_SECURE_SCL' is defined to be '0': did you mean to use '#if _SECURE_SCL'?
#   C4554: '>>' : check operator precedence for possible error; use parentheses to clarif
#   C4456: declaration of 'XXX' hides previous local declaration
#   C4471: 'bar' : a forward declaration of an unscoped enumeration must have an underlying type (int assumed)
#   C4350: behavior change: 'std::...' called instead of 'std::...'
#   C4370  layout of class has changed from a previous version of the compiler due to better packing
#   C4371  layout of class may have changed from a previous version of the compiler due to better packing of member
#   C4316  object allocated on the heap may not be aligned
#   C4091  'keyword' : ignored on left of 'type' when no variable is declared - fails in win.sdk.81/um/DbgHelp.h
#   C4302  'type cast': truncation from 'T1' to 'T2' // optimizations on int* -> ulong in crypto
#   C4312  'type cast': conversion from 'T1' to 'T2' of greater size
#   C4334  'operator' : result of 32-bit shift implicitly converted to 64 bits (was 64-bit shift intended?)
#   C4388  signed/unsigned mismatch
#   C4458   declaration of 'parameter' hides class member
#   C4774  '<function>' : format string expected in argument <position> is not a string literal
#   C5039  pointer or reference to potentially throwing function passed to extern C function
#   C5045  Compiler will insert Spectre mitigation for memory load if /Qspectre switch specified
#   C4623  default constructor was implicitly defined as deleted
#   C5038  (some) data member will be initialized after (another) data member
#   C4768  __declspec attributes before linkage specification are ignored
#   C5052  Keyword 'char8_t' was introduced in C++20 and requires use of the '/std:c++latest' command-line option
#   C5204 non-virtual destructor for class with virtual functions
#   C4577 'noexcept' used with no exception handling mode specified
#   C5220 a non-static data member with a volatile qualified type no longer implies that compiler generated copy/move constructors and copy/move assignment operators are non trivial
#   C5219 implicit conversion. That is GOOD warning, we would rather fix it everywhere!
#   C5245 'function': unreferenced function with internal linkage has been removed
#   C5246 'member': the initialization of a subobject should be wrapped in braces
#   C5264 'variable-name': 'const' variable is not used

set(_STRICT_DIAGNOSTIC_OPTIONS -Wall -WX)
if(MSVC)
    list(APPEND _STRICT_DIAGNOSTIC_OPTIONS
        /wd4514 /wd4061 /wd4820 /wd4668 /wd4619
        /wd4365 /wd4127 /wd4302 /wd4242 /wd4244 /wd4265
        /wd4101 /wd4201 /wd4625 /wd4626 /wd4800 /wd4018
        /wd4710 /wd4245 /wd4291 /wd4389 /wd4200 /wd4255
        /wd4711 /wd4062 /wd4355 /wd4640 /wd4305 /wd4324
        /wd4511 /wd4512 /wd4305 /wd4738 /wd4996 /wd4005
        /wd4740 /wd4702 /wd4826 /wd4503 /wd4748 /wd4987
        /wd4574 /wd4554 /wd4471 /wd4350 /wd4370 /wd4371
        /wd4316 /wd4388 /wd4091 /wd5026 /wd5027 /wd4774
        /wd4312 /wd4302 /wd4334 /wd5220 /wd5219
        /wd4464 /wd4463 /wd4589 /wd4595 /wd4435 /wd4319 /wd4311 /wd4267 /wd4477 /wd4777
        /wd4548 /wd5039 /wd5045 /wd4623 /wd5038 /wd4768
        /wd4456 # very convenient for casting to the same var within one function (event handler)
        #/wd444 /wd279 /wd981 /wd383 /wd310 /wd174 /wd111 /wd271 /wd4714
        /wd5052
        /wd5204 /wd4577
        /wd4866 # warning C4866: compiler may not enforce left-to-right evaluation order for call to operator_name
        /wd5245 # Some functions' overloads aren't used and the compiler warns to remove then
        /wd5246 # the MSC has the CWG defect #1270 and offers the old initialization behaviour, https://stackoverflow.com/a/70127399
        /wd5264 # falsely identifies static const variables unused
        /wd4275 # non dll-interface struct 'XXX' used as base for dll-interface class 'YYY'
        /wd4250 # 'Class_XXX': inherits 'Method_YYY' via dominance
        /permissive-
    )
endif()

if(MSVC)
    list(APPEND _DIAGNOSTIC_OPTIONS
        -W3 /wd4244 /wd4101 /wd4800 /wd4018 /wd4291 /wd4200 /wd4355 /wd4305
        /wd4996 /wd4005 /wd4740 /wd4748 /wd4324 /wd4503 /wd4574 /wd4554 /wd4316
        /wd4388 /wd4091 /wd5026 /wd5027 /wd4334
        /wd4595 /wd4838 /wd4312 /wd4302 /wd4311 /wd4319 /wd4477 /wd5039 /wd5045 /wd4623 /wd5038 /wd4768
        /wd5204 /wd4577 /wd4267
        /wd4723 #warning C4723: potential divide by 0
        /wd4866 # warning C4866: compiler may not enforce left-to-right evaluation order for call to operator_name
        #warning C4263: 'void B::f(int)' : member function does not override any base class virtual member function
        #/w14263
        #warning C4264: 'void A::f(void)' : no override available for virtual member function from base 'A'; function is hidden
        #/w14264
        /wd4251 #  Warning	C4251	'': class '' needs to have dll-interface to be used by clients of struct ''
        /wd4275 # non dll-interface struct 'XXX' used as base for dll-interface class 'YYY'
        /wd4250 # 'Class_XXX': inherits 'Method_YYY' via dominance
    )
endif()

if(Compiler_ClangCl OR Compiler_Clang)
    #Temporary disabled warnings from clang
    set(CLANG_DISABLED_WARNINGS
        -Wno-deprecated-builtins
        -Wno-reorder-ctor
        -Wno-invalid-offsetof
        -Wno-deprecated-enum-enum-conversion
        -Wno-deprecated-volatile
    )
    list(APPEND
        _STRICT_DIAGNOSTIC_OPTIONS
        ${CLANG_DISABLED_WARNINGS}
    )
    list(APPEND
        _DIAGNOSTIC_OPTIONS
        ${CLANG_DISABLED_WARNINGS}
    )

endif()

if(MSVC)
    set(_CONFIG_OPTIONS_DEBUG /GF /Gy /Gw /Oi /Oy /Od)
    set(_CONFIG_OPTIONS_RELEASE /Ox /GF /Gy /Gw /Oi /Ot /Oy)
    set(_CONFIG_OPTIONS_RELWITHDEBINFO /Od /GS- /GF /Gy /Gw)
endif()

set(_CONFIG_OPTIONS
    $<$<CONFIG:Debug>:${_CONFIG_OPTIONS_DEBUG}>
    $<$<CONFIG:Release>:${_CONFIG_OPTIONS_RELEASE}>
    $<$<CONFIG:RelWithDebInfo>:${_CONFIG_OPTIONS_RELWITHDEBINFO}>
)

if(${NAU_RTTI})
else()
endif()

if(${NAU_EXCEPTIONS})
else()
endif()

#nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} /EHsc -DDAGOR_EXCEPTIONS_ENABLED=1 )

list(APPEND _RTTI_OPTIONS /GR-)

# if (NOT ${Config} STREQUAL dbg)
#     nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} ${_VC_CRT_TYPE} )
# else()
#     nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} ${_VC_CRT_TYPE}d )
# endif()

# if (${DriverLinkage} STREQUAL static)
#  	nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} -D_TARGET_STATIC_LIB=1)
# endif()
# if (${StarForce})
#  	nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} -DSTARFORCE_PROTECT)
# endif()
# if (${UseWholeOpt})
#  	nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} /GL)
# endif()

# if (${Analyze})
#  	nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} /analyze)
# endif()

if(MSVC)
    list(APPEND _CONFIG_OPTIONS
        /FS
    )
endif()

if(Compiler_ClangCl)
    list(APPEND _CONFIG_OPTIONS
        /clang:-mavx2
        /clang:-mfma
    )
endif()

if(Compiler_Clang)
    list(APPEND _CONFIG_OPTIONS
        -mavx2
        -mfma
    )
endif()


#nau_set_list(_CONFIG_OPTIONS ${_CONFIG_OPTIONS} /FS )         # Force Synchronous PDB Writes.

#nau_set_list(_CPP_OPT  ${_CONFIG_OPTIONS} ${_SUPPRESS_OPT} ${CPPopt} ${GlobalCPPopt})

#AddForceFastcall _CPP_OPT ;
#AddDirectXInclude pre_opt : post_opt ;
#AddWtlInclude pre_opt : vc8 ;

#if $(QtVer) { AddQtCompileOpt ; }

set(_CPP_OPTIONS
    ${_DEF_C_CPP_OPTIONS}
    ${_DIAGNOSTIC_OPTIONS}
    ${_CONFIG_OPTIONS}
)

set(_CPP_STRICT_OPTIONS
    ${_DEF_C_CPP_OPTIONS}
    ${_STRICT_DIAGNOSTIC_OPTIONS}
    ${_CONFIG_OPTIONS}
)

set(_C_OPTIONS ${_DEF_C_CPP_OPTIONS} ${_DIAGNOSTIC_OPTIONS} ${_CONFIG_OPTIONS})
set(_C_STRICT_OPTIONS ${_DEF_C_CPP_OPTIONS} ${_STRICT_DIAGNOSTIC_OPTIONS} ${_CONFIG_OPTIONS})

if(MSVC)
    list(APPEND _C_OPTIONS /TC)
    list(APPEND _C_STRICT_OPTIONS /TC)
endif()


#enable_language(ASM_NASM)
#enable_language(ASM_MASM)

#set(CMAKE_ASM_NASM_COMPILE_OBJECT "<CMAKE_ASM_NASM_COMPILER> <INCLUDES> <FLAGS> -o <OBJECT> <SOURCE>")

#set(CMAKE_ASM_MASM_CREATE_STATIC_LIBRARY "<CMAKE_AR> /OUT:<TARGET> <LINK_FLAGS> <OBJECTS>")

# Create a compile option that operates on ASM_NASM files
# If the target has a property NASM_OBJ_FORMAT, use it, otherwise
# use the environment variable CMAKE_ASM_NASM_OBJECT_FORMAT
# add_compile_options(
#     "$<$<COMPILE_LANGUAGE:ASM_NASM>:-f $<IF:$<BOOL:$<TARGET_PROPERTY:NASM_OBJ_FORMAT>>, \
#     $<TARGET_PROPERTY:NASM_OBJ_FORMAT>, ${CMAKE_ASM_NASM_OBJECT_FORMAT}>>"
# )

# nau_set_list(_DEF_ASM_CMDLINE
#     -f
#     win64
# )



