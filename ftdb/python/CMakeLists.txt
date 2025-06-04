set(PYFTDB_SOURCES
    pyftdb.c
    utils.c
    maps.c

    ftdbmaps.cpp

    generic_collection.c

    funcs.c
    funcs_entry.c
    funcs_iter.c

    funccallinfo.c
    funcifinfo.c
    funcoffsetrefinfo.c
    funcswitchinfo.c
    funcderefinfo.c
    funclocalinfo.c

    fops.c
    fops_entry.c
    fops_iter.c

    globals.c
    globals_entry.c
    globals_iter.c

    types.c
    types_entry.c
    types_iter.c

    funcdecls.c
    funcdecls_entry.c
    funcdecls_iter.c

    unresolvedfuncs.c
    unresolvedfuncs_entry.c
    unresolvedfuncs_iter.c

    modules.c
    modules_iter.c

    sources.c
    sources_iter.c
)

add_library(pyftdb SHARED ${PYFTDB_SOURCES})
target_compile_options(pyftdb PUBLIC $<$<COMPILE_LANGUAGE:CXX>:-std=c++14>)
target_compile_options(pyftdb PUBLIC $<$<COMPILE_LANGUAGE:C>:-std=c11>)
target_compile_options(pyftdb PRIVATE $<$<CONFIG:Debug>:-O0>)
target_compile_options(pyftdb PRIVATE $<$<CONFIG:Debug>:-DDEBUG>)
target_include_directories(pyftdb PRIVATE ${Python3_INCLUDE_DIRS})
target_include_directories(pyftdb PRIVATE ${PROJECT_SOURCE_DIR}/ftdb)
target_link_libraries(pyftdb PRIVATE uflat_static)
target_link_libraries(pyftdb PRIVATE unflatten_static)
target_link_directories(pyftdb PRIVATE ${Python3_LIBRARIES})
set_target_properties(pyftdb PROPERTIES OUTPUT_NAME ftdb)
install(TARGETS pyftdb DESTINATION ${PROJECT_SOURCE_DIR})
