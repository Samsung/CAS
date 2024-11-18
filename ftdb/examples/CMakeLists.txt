set(FTDB_SHA256_TEST_SOURCES
    ftdb_sha256_test.c
)

add_executable(ftdb_sha256_test ${FTDB_SHA256_TEST_SOURCES})
target_compile_options(ftdb_sha256_test PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(ftdb_sha256_test PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
target_include_directories(ftdb_sha256_test PRIVATE ${FTDB_C_INCLUDES})
target_link_libraries(ftdb_sha256_test PRIVATE ftdb_c)
target_link_libraries(ftdb_sha256_test PRIVATE uflat_static)
target_link_libraries(ftdb_sha256_test PUBLIC crypto)