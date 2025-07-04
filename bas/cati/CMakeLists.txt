set(CATI_SOURCES
    cati.c
)

add_executable(cati ${CATI_SOURCES})

target_compile_options(cati PRIVATE
    $<$<CONFIG:Release>:-O3 -march=native -flto>
    $<$<CONFIG:Debug>:-O0 -DDEBUG>
)

target_link_options(cati PRIVATE
    $<$<CONFIG:Release>:-static -flto -s>
)

install(TARGETS cati DESTINATION ${PROJECT_SOURCE_DIR})