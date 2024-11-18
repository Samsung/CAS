set(FTDB_SOURCES
    pyftdb.c
    maps.c
    utils.c
    ftdbmaps.cpp
)

add_library(ftdb_shared SHARED ${FTDB_SOURCES})
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(ftdb_shared PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-c99-designator>)
endif()
target_compile_options(ftdb_shared PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
target_compile_options(ftdb_shared PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(ftdb_shared PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
target_include_directories(ftdb_shared PRIVATE ${Python3_INCLUDE_DIRS})
target_link_libraries(ftdb_shared PUBLIC ${Python3_LIBRARIES})
target_link_libraries(ftdb_shared PRIVATE uflat_static)
target_link_libraries(ftdb_shared PRIVATE unflatten_static)
set_target_properties(ftdb_shared PROPERTIES OUTPUT_NAME ftdb)
install(TARGETS ftdb_shared DESTINATION ${PROJECT_SOURCE_DIR})

set(FTDB_C_SOURCES
    ftdb.c
    maps.c
)

set(FTDB_C_INCLUDES
    ftdb.h
)

add_library(ftdb_c SHARED ${FTDB_C_SOURCES})
target_link_libraries(ftdb_c PUBLIC stdc++)
target_link_libraries(ftdb_c PRIVATE unflatten_static)
target_link_libraries(ftdb_c PRIVATE uflat_static)
target_include_directories(ftdb_c PUBLIC ${PROJECT_SOURCE_DIR}/ftdb)
target_compile_options(ftdb_c PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(ftdb_c PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
install(TARGETS ftdb_c DESTINATION ${PROJECT_SOURCE_DIR})

add_subdirectory(examples)