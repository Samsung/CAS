set(GCC_INPUT_NAME_SOURCES
    input_name.cpp
)

execute_process (
    COMMAND bash -c "gcc -print-file-name=plugin"
    OUTPUT_VARIABLE gcc_plugin_include_directory
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_library(gcc_input_name SHARED ${GCC_INPUT_NAME_SOURCES})

target_compile_options(gcc_input_name PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-Wno-mismatched-tags>)

target_include_directories(gcc_input_name PRIVATE ${gcc_plugin_include_directory}/include)

install(TARGETS gcc_input_name DESTINATION ${PROJECT_SOURCE_DIR})
