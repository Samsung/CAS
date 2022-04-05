# Our previous CMakeLists was really bad, so we borrowed one from https://github.com/andreasfertig/cppinsights. Thanks guys!

cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

set(LLVM_CONFIG "llvm-config" CACHE STRING "LLVM config executable to use")

set(MIN_LLVM_MAJOR_VERSION 10)
set(MIN_LLVM_VERSION ${MIN_LLVM_MAJOR_VERSION}.0)

project(clang-proc CXX)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(IS_GNU On)
else()
    set(IS_GNU Off)
endif()

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(IS_CLANG On)
else()
    set(IS_CLANG Off)
endif()

function(check_compiler COMPILER version)
    if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "${COMPILER}")
        if (CMAKE_CXX_COMPILER_VERSION VERSION_LESS ${version})
            message(FATAL_ERROR "${COMPILER} version must be at least ${version}!")
        endif()

        set(HAVE_COMPILER On PARENT_SCOPE)
    endif()
endfunction(check_compiler)

check_compiler("GNU" 7.3)
check_compiler("Clang" 6.0)
check_compiler("MSVC" 19.25.0.0)

if(NOT HAVE_COMPILER)
    message(WARNING "You are using an unsupported compiler! Compilation has only been tested with Clang and GCC.")
endif()

# Find LLVM Config binary for LLVM
# If you're on debian, go to http://llvm.org/apt/ and install the development
# release of LLVM (all packages).
# If you're on Windows, build LLVM from scratch,
# see Readme_Windows.md instructions.
find_program(LLVM_CONFIG_PATH "${LLVM_CONFIG}")
if(NOT LLVM_CONFIG_PATH)
    message(FATAL_ERROR "llvm-config not found -- ${LLVM_CONFIG_PATH}: ${LLVM_CONFIG}")
endif()

# This function saves the output of the llvm-config command with the given
# switch to the variable named VARNAME.
#
# Example usage: llvm_config(LLVM_CXXFLAGS "--cxxflags")
function(llvm_config VARNAME switch)
    set(CONFIG_COMMAND "${LLVM_CONFIG_PATH}" "${switch}")

    execute_process(
        COMMAND ${CONFIG_COMMAND} ${LIB_TYPE}
        RESULT_VARIABLE HAD_ERROR
        OUTPUT_VARIABLE CONFIG_OUTPUT
    )

    if (HAD_ERROR)
        string(REPLACE ";" " " CONFIG_COMMAND_STR "${CONFIG_COMMAND}")
        message(STATUS "${CONFIG_COMMAND_STR}")
        message(FATAL_ERROR "llvm-config failed with status ${HAD_ERROR}")
    endif()

    # replace linebreaks with semicolon
    string(REGEX REPLACE
        "[ \t]*[\r\n]+[ \t]*" ";"
        CONFIG_OUTPUT ${CONFIG_OUTPUT})

    if(USE_SYSTEM_INCLUDES)
        # make all includes system include to prevent the compiler to warn about issues in LLVM/Clang
        string(REGEX REPLACE "-I" "-isystem" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
    endif()

    # remove certain options clang doesn't like
    if(IS_CLANG)
        string(REGEX REPLACE "-Wl,--no-keep-files-mapped" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Wl,--no-map-whole-files" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-fuse-ld=gold" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
    elseif(IS_GNU)
        string(REGEX REPLACE "-Wcovered-switch-default" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Wstring-conversion" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Werror=unguarded-availability-new" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Wno-unknown-warning-option" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Wno-unused-command-line-argument" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
    endif()

    # Since Clang 10 of ziglang C++14 is set
    string(REGEX REPLACE "-std[:=]+c\\+\\+[0-9][0-9]" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")

    # make result available outside
    set(${VARNAME} ${CONFIG_OUTPUT} PARENT_SCOPE)

    # Optionally output the configured value
    message(STATUS "llvm_config(${VARNAME})=>${CONFIG_OUTPUT}")

    # cleanup
    unset(CONFIG_COMMAND)
endfunction(llvm_config)

llvm_config(LLVM_CXXFLAGS "--cxxflags")
llvm_config(LLVM_LDFLAGS "--ldflags")

string(REGEX REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REGEX REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

llvm_config(LLVM_LIBS2 "--libs")
set(LLVM_LIBS ${LLVM_LIBS2})

llvm_config(LLVM_LIBDIR "--libdir")
llvm_config(LLVM_INCLUDE_DIR "--includedir")

llvm_config(LLVM_SYSTEM_LIBS2 "--system-libs")
set(LLVM_SYSTEM_LIBS ${LLVM_SYSTEM_LIBS2})

llvm_config(LLVM_PACKAGE_VERSION "--version")
string(REPLACE "git" "" LLVM_PACKAGE_VERSION_PLAIN "${LLVM_PACKAGE_VERSION}")

# copied from: llvm/tools/clang/cmake/modules/AddClang.cmake
macro(add_clang_tool name)
  add_executable( ${name} ${ARGN} )
endmacro(add_clang_tool)

# name the executable and all source files
add_clang_tool(clang-proc
    clang-proc/utils.cc
    clang-proc/main.cc
    clang-proc/sha.cpp
    clang-proc/base64.cpp
    clang-proc/dbjson.cpp
    clang-proc/notice.cpp
    clang-proc/visit.cpp
    clang-proc/lookup.cpp
    clang-proc/typehash.cpp
    clang-proc/fops.cpp
    clang-proc/taint.cpp
    clang-proc/PPCallbacksTracker.cpp
    clang-proc/DeclPrinter.cpp
    clang-proc/StmtPrinter.cpp
    clang-proc/TypePrinter.cpp
)

target_compile_options(clang-proc PRIVATE ${LLVM_CXXFLAGS})
target_compile_options(clang-proc PRIVATE -ggdb3)
target_compile_options(clang-proc PRIVATE $<$<CONFIG:Debug>:-O0>)

string(REPLACE "." ";" CLANG_VERSION_LIST ${LLVM_PACKAGE_VERSION})
list(GET CLANG_VERSION_LIST 0 CLANG_VERSION)

target_compile_options(clang-proc PRIVATE -std=c++14)
target_compile_options(clang-proc PRIVATE -fno-rtti)
target_compile_options(clang-proc PRIVATE -Wno-non-virtual-dtor)
target_compile_options(clang-proc PRIVATE -Wno-missing-field-initializers)
target_compile_options(clang-proc PRIVATE -Wno-comment)
target_compile_options(clang-proc PRIVATE -Wno-strict-aliasing)
target_compile_options(clang-proc PRIVATE "-DCLANG_VERSION=${CLANG_VERSION}")
if(IS_CLANG)
    target_compile_options(clang-proc PRIVATE -Wno-unused-private-field)
endif()

include_directories(${LLVM_INCLUDE_DIR} .)
link_directories(${LLVM_LIBDIR})

# additional libs required when building outside llvm
set(ADDITIONAL_LIBS
    ${LLVM_LDFLAGS}
    clangFrontend
    clangDriver
    clangSerialization
    clangParse
    clangSema
    clangAnalysis
    clangEdit
    clangAST
    clangLex
    clangBasic
    clangRewrite
    ${LLVM_LIBS}
    ${LLVM_SYSTEM_LIBS}
)

# minimum clang version
if (${LLVM_PACKAGE_VERSION} VERSION_LESS ${MIN_LLVM_VERSION})
    message(FATAL_ERROR "LLVM version ${MIN_LLVM_VERSION} or higher required. Current version is: ${LLVM_PACKAGE_VERSION}.")
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
  execute_process(
    COMMAND git log -1 --format=%H
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
  )
else()
  set(GIT_COMMIT_HASH "")
endif()

# Get the include dir
llvm_config(LLVM_INCLUDE_DIR "--includedir")

include_directories(${CMAKE_BINARY_DIR}/generated)

# general include also provided by clang-build
target_link_libraries(clang-proc
    PRIVATE
    clangTooling
    clangASTMatchers
    ${ADDITIONAL_LIBS}
)

target_include_directories(clang-proc PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/clang-proc)

install(TARGETS clang-proc RUNTIME DESTINATION .)

message(STATUS "")
message(STATUS "[ Build summary ]")
message(STATUS "CMAKE_GENERATOR       : ${CMAKE_GENERATOR}")
message(STATUS "LLVM_CXXFLAGS         : ${LLVM_CXXFLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_LINKER          : ${CMAKE_LINKER}")
message(STATUS "Compiler ID           : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version      : ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compiler path         : ${CMAKE_CXX_COMPILER}")
message(STATUS "llvm-config           : ${LLVM_CONFIG_PATH}")
message(STATUS "Min LLVM major version: ${MIN_LLVM_MAJOR_VERSION}")
message(STATUS "Install path          : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "Clang resource dir    : ${INSIGHTS_CLANG_RESOURCE_DIR}")
message(STATUS "CMAKE_SOURCE_DIR      : ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR      : ${CMAKE_BINARY_DIR}")
message(STATUS "Git commit hash       : ${GIT_COMMIT_HASH}")
message(STATUS "")

project(etrace_tools)
SET(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -ggdb3")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -ggdb3")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-unused-function")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-function")
if(IS_CLANG)
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedef")
else()
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-unused-local-typedefs")
    set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wno-address")
endif()

execute_process (
    COMMAND bash -c "gcc -print-file-name=plugin"
    OUTPUT_VARIABLE gcc_plugin_include_directory
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_executable(etrace_parser
    bas/main.cpp
    bas/parser.cpp
    bas/rbtree.c
    bas/utils.c
)
install(TARGETS etrace_parser DESTINATION .)

target_compile_options(etrace_parser PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
target_compile_options(etrace_parser PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-register>)
target_compile_options(etrace_parser PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-value>)
target_compile_options(etrace_parser PRIVATE $<$<CONFIG:Debug>:-O0>)

add_executable(cati
    bas/etrace_cat.c
)
target_compile_options(cati PRIVATE $<$<CONFIG:Debug>:-O0>)
install(TARGETS cati DESTINATION .)

add_library(etrace SHARED
        bas/strnstr.c
        bas/base64.c
        bas/compiler.c
        bas/compiler_gcc.c
        bas/compiler_clang.c
        bas/compiler_armcc.c
        bas/pyetrace.c
        bas/libflat.c
        bas/rbtree.c
        bas/interval_tree.c
        bas/maps.c
        bas/utils.c
        bas/filedeps.cpp
        bas/nfsdb_maps.cpp
)

target_compile_options(etrace PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
if(IS_CLANG)
    target_compile_options(etrace PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-designator>)
endif()
target_compile_options(etrace PRIVATE $<$<CONFIG:Debug>:-O0>)

add_library(ftdb SHARED
        bas/pyftdb.c
        bas/libflat.c
        bas/rbtree.c
        bas/interval_tree.c
        bas/maps.c
        bas/utils.c
        bas/ftdbmaps.cpp
)

target_compile_options(ftdb PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
if(IS_CLANG)
    target_compile_options(ftdb PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-designator>)
endif()
target_compile_options(ftdb PRIVATE $<$<CONFIG:Debug>:-O0>)

target_include_directories(etrace PRIVATE /usr/include/python3.8)
target_link_libraries(etrace PRIVATE "-lrt")
install(TARGETS etrace DESTINATION .)

target_include_directories(ftdb PRIVATE /usr/include/python3.8)
target_link_libraries(ftdb PRIVATE "-lrt")
install(TARGETS ftdb DESTINATION .)

add_library(gcc_input_name SHARED
    bas/input_name.cpp
)

target_compile_options(gcc_input_name PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-tags>)
target_include_directories(gcc_input_name PRIVATE ${gcc_plugin_include_directory}/include)
install(TARGETS gcc_input_name DESTINATION .)
