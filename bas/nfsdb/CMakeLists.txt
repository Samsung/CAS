set(NFSDB_SOURCES
    base64.c
    pyetrace.c
    maps.c
    utils.c
    filedeps.cpp
    nfsdb_maps.cpp
)

add_library(etrace SHARED ${NFSDB_SOURCES})

if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(etrace PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-designator>)
    target_compile_options(etrace PRIVATE -Wno-unused-local-typedef)
elseif(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    target_compile_options(etrace PRIVATE -Wno-unused-local-typedefs)
    target_compile_options(etrace PRIVATE -Wno-address)
endif()
target_compile_options(etrace PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
target_compile_options(etrace PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(etrace PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
target_compile_options(etrace PRIVATE $<$<CONFIG:Debug>:-ggdb3>)
target_compile_options(etrace PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-register>)
target_compile_options(etrace PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unknown-attributes>)
target_compile_options(etrace PRIVATE -Wno-unused-function)
target_compile_options(etrace PRIVATE -Wall)

target_compile_options(etrace PRIVATE -Wno-unused-local-typedef)
target_compile_options(etrace PRIVATE "-DLIBRARY_BUILD=1")

target_include_directories(etrace PRIVATE ${Python3_INCLUDE_DIRS})

target_link_libraries(etrace PRIVATE uflat_static)
target_link_libraries(etrace PRIVATE unflatten_static)
target_link_libraries(etrace PUBLIC ${Python3_LIBRARIES})
target_link_libraries(etrace PRIVATE "-lrt")

install(TARGETS etrace DESTINATION ${PROJECT_SOURCE_DIR})
