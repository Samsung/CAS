set(LLVM_MINIMAL_MAJOR_VERSION 10)
if (${LLVM_PACKAGE_VERSION} VERSION_LESS ${LLVM_MINIMAL_MAJOR_VERSION})
    message(FATAL_ERROR "LLVM version ${LLVM_MINIMAL_MAJOR_VERSION} or higher required. Current version is: ${LLVM_PACKAGE_VERSION}.")
endif()

set(CLANG_PROC_SOURCES
    main.cc
    utils.cc
    sha.cpp
    base64.cpp
    dbjson.cpp
    notice.cpp
    visit.cpp
    lookup.cpp
    typehash.cpp
    taint.cpp
    MacroHandler.cpp
    ${LLVM_VERSION_MAJOR}/DeclPrinter.cpp
    ${LLVM_VERSION_MAJOR}/StmtPrinter.cpp
    ${LLVM_VERSION_MAJOR}/TypePrinter.cpp
)

set(CLANG_PROC_LIBS
    pthread
    clangTooling
    clangASTMatchers
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
    clangToolingSyntax
    ${LLVM_LIBS}
    ${LLVM_SYSTEM_LIBS}
)

add_executable(clang-proc ${CLANG_PROC_SOURCES})

if(CMAKE_C_COMPILER_ID STREQUAL "Clang")
    target_compile_options(clang-proc PRIVATE -Wno-unused-private-field)
endif()
target_compile_options(clang-proc PRIVATE SHELL:${LLVM_CXXFLAGS})
target_compile_options(clang-proc PRIVATE -ggdb3)
target_compile_options(clang-proc PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(clang-proc PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
target_compile_options(clang-proc PRIVATE -std=c++17)
target_compile_options(clang-proc PRIVATE -Wno-non-virtual-dtor)
target_compile_options(clang-proc PRIVATE -Wno-missing-field-initializers)
target_compile_options(clang-proc PRIVATE -Wno-comment)
target_compile_options(clang-proc PRIVATE -Wno-strict-aliasing)
target_compile_options(clang-proc PRIVATE -Wno-switch)
target_compile_options(clang-proc PRIVATE "-DCLANG_VERSION=${LLVM_VERSION_MAJOR}")
target_compile_options(clang-proc PRIVATE "-DCLANG_BUILTIN_INCLUDE_PATH=\"${LLVM_INCLUDE_DIRS}\"")
target_link_directories(clang-proc PRIVATE ${LLVM_LIBDIR})

target_link_libraries(clang-proc PRIVATE ${CLANG_PROC_LIBS})
if(LLVM_VERSION_MAJOR VERSION_GREATER_EQUAL 15)
    target_link_libraries(clang-proc PRIVATE clangSupport)
endif()

target_include_directories(clang-proc PRIVATE ${LLVM_INCLUDE_DIR})
target_include_directories(clang-proc PRIVATE .)

target_compile_definitions(clang-proc PRIVATE ${LLVM_DEFINITIONS_LIST})

# Change release build optimization level to O2
string(REGEX REPLACE "-O3" "-O2" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REGEX REPLACE "-O3" "-O2" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

string(REGEX REPLACE "-DNDEBUG" "" CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE}")
string(REGEX REPLACE "-DNDEBUG" "" CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE}")

install(TARGETS clang-proc RUNTIME DESTINATION ${PROJECT_SOURCE_DIR}/clang-proc)
