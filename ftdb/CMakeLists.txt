set(FTDB_C_SOURCES
    ftdb.c
    maps.c
)


add_library(ftdb_c SHARED ${FTDB_C_SOURCES})
target_link_libraries(ftdb_c PUBLIC stdc++)
target_link_libraries(ftdb_c PRIVATE unflatten_static)
target_link_libraries(ftdb_c PRIVATE uflat_static)
target_include_directories(ftdb_c PUBLIC ${PROJECT_SOURCE_DIR}/ftdb)
target_compile_options(ftdb_c PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(ftdb_c PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
install(TARGETS ftdb_c DESTINATION ${PROJECT_SOURCE_DIR})

add_subdirectory(python)
add_subdirectory(examples)
