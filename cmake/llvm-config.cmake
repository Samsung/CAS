find_package(LLVM REQUIRED CONFIG)

find_program(LLVM_CONFIG_PATH_BIN "llvm-config")
if(NOT LLVM_CONFIG_BIN AND LLVM_CONFIG_PATH_BIN)
    set(LLVM_CONFIG_BIN ${LLVM_CONFIG_PATH_BIN})
elseif(NOT LLVM_CONFIG_BIN AND NOT LLVM_CONFIG_PATH_BIN)
    set(LLVM_CONFIG_BIN "${LLVM_TOOLS_BINARY_DIR}/llvm-config")
endif()

# This function saves the output of the llvm-config command with the given
# switch to the variable named VARNAME.
#
# Example usage: llvm_config(LLVM_CXXFLAGS "--cxxflags")
function(llvm_config VARNAME switch)
    set(CONFIG_COMMAND "${LLVM_CONFIG_BIN}" "${switch}")

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
    if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        string(REGEX REPLACE "-Wl,--no-keep-files-mapped" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-Wl,--no-map-whole-files" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
        string(REGEX REPLACE "-fuse-ld=gold" "" CONFIG_OUTPUT "${CONFIG_OUTPUT}")
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
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

if(LLVM_CONFIG_BIN)
    llvm_config(LLVM_PACKAGE_VERSION "--version")
    llvm_config(LLVM_TOOLS_BINARY_DIR "--bindir")
endif()

# LLVM_VERSION_MAJOR - Major version number of installed LLVM
string(REPLACE "." ";" LLVM_VERSION_MAJOR_LIST ${LLVM_PACKAGE_VERSION})
list(GET LLVM_VERSION_MAJOR_LIST 0 LLVM_VERSION_MAJOR)

# LLVM_DEFINITIONS_LIST - LLVM compiler definitions
separate_arguments(LLVM_DEFINITIONS_LIST NATIVE_COMMAND ${LLVM_DEFINITIONS})

# LLVM_CXXFLAGS - C++ compiler flags for LLVM
llvm_config(LLVM_CXXFLAGS "--cxxflags")

# LLVM_LDFLAGS - Linker flags for LLVM
llvm_config(LLVM_LDFLAGS "--ldflags")

# LLVM_LIBS - Base set of libraries to link against LLVM
llvm_config(LLVM_LIBS "--libs")

# LLVM_LIBDIR - LLVM Library directory
llvm_config(LLVM_LIBDIR "--libdir")

# LLVM_INCLUDE_DIR - LLVM header directory
llvm_config(LLVM_INCLUDE_DIR "--includedir")

# LLVM_SYSTEM_LIBS - System libraries to link against LLVM
llvm_config(LLVM_SYSTEM_LIBS "--system-libs")
