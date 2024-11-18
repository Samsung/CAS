# Our previous CMakeLists was really bad and the one before it supposedly was too.
# So we rewrote it for the third time.
cmake_minimum_required(VERSION 3.12 FATAL_ERROR)

project(cas)

include(cmake/llvm-config.cmake)
include(cmake/git_hash.cmake)

set(CMAKE_INSTALL_PREFIX ${CMAKE_SOURCE_DIR})

find_package(Python3 COMPONENTS Interpreter Development REQUIRED)

if (NOT EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/kflat/CMakeLists.txt)
    message(STATUS "Initializing missing KFLAT submodule")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init
        WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
    )
else()
    execute_process(
        COMMAND sh -c "${GIT_EXECUTABLE} status | grep '(new commits)' -q"
        RESULT_VARIABLE GREP_NOT_MATCHED
    )

    if (NOT GREP_NOT_MATCHED)
        message(STATUS "Pulling in submodules updates")
        execute_process(
            COMMAND ${GIT_EXECUTABLE} submodule update
            WORKING_DIRECTORY ${PROJECT_SOURCE_DIR}
        )
        execute_process(
            COMMAND rm -r kflat
            WORKING_DIRECTORY ${PROJECT_BINARY_DIR}
        )
    endif()
endif()

message(STATUS "")
message(STATUS "[ Build summary ]")
message(STATUS "CMAKE_GENERATOR       : ${CMAKE_GENERATOR}")
message(STATUS "LLVM_CXXFLAGS         : ${LLVM_CXXFLAGS}")
message(STATUS "CMAKE_EXE_LINKER_FLAGS: ${CMAKE_EXE_LINKER_FLAGS}")
message(STATUS "CMAKE_LINKER          : ${CMAKE_LINKER}")
message(STATUS "Compiler ID           : ${CMAKE_CXX_COMPILER_ID}")
message(STATUS "Compiler version      : ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "Compiler path         : ${CMAKE_CXX_COMPILER}")
message(STATUS "LLVM tools dir        : ${LLVM_TOOLS_BINARY_DIR}")
message(STATUS "Install path          : ${CMAKE_INSTALL_PREFIX}")
message(STATUS "CMAKE_SOURCE_DIR      : ${CMAKE_SOURCE_DIR}")
message(STATUS "CMAKE_BINARY_DIR      : ${CMAKE_BINARY_DIR}")
message(STATUS "Git commit hash       : ${GIT_COMMIT_HASH}")
message(STATUS "")


add_subdirectory(kflat)
set_property(DIRECTORY kflat PROPERTY EXCLUDE_FROM_ALL YES)

add_subdirectory(ftdb)
add_subdirectory(clang-proc)
add_subdirectory(bas)
