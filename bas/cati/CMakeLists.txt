set(CATI_SOURCES
    cati.c
)

add_executable(cati ${CATI_SOURCES})

target_compile_options(cati PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(cati PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)

install(TARGETS cati DESTINATION ${PROJECT_SOURCE_DIR})