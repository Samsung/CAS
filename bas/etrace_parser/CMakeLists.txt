set(ETRACE_PARSER_SOURCES
    main.cpp
    parser.cpp
    error.cpp
)

add_executable(etrace_parser ${ETRACE_PARSER_SOURCES})

target_compile_options(etrace_parser PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)
target_compile_options(etrace_parser PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-deprecated-register>)
target_compile_options(etrace_parser PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unknown-attributes>)
target_compile_options(etrace_parser PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-Wno-unused-value>)
target_compile_options(etrace_parser PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-march=native>)
target_compile_options(etrace_parser PRIVATE -Wall)
target_compile_options(etrace_parser PRIVATE -Wextra)
target_compile_options(etrace_parser PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(etrace_parser PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)

target_include_directories(etrace_parser PRIVATE .)

target_link_libraries(etrace_parser pthread)

install(TARGETS etrace_parser DESTINATION ${PROJECT_SOURCE_DIR})