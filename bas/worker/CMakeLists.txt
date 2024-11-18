set(WORKER_SOURCES
    worker.cpp
)

add_executable(worker ${WORKER_SOURCES})

target_compile_options(worker PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
target_compile_options(worker PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(worker PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)

install(TARGETS worker DESTINATION ${PROJECT_SOURCE_DIR})