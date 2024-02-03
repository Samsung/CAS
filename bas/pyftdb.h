#ifndef __PYFTDB_H__
#define __PYFTDB_H__

#define PY_SSIZE_T_CLEAN
#ifdef __cplusplus
extern "C" {
#endif
#include <Python.h>
#include <structmember.h>
#include <signal.h>
#include <fnmatch.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "rbtree.h"
#include "utils.h"
#include "ftdb.h"
#include "ftdb_entry.h"

#include <unflatten.hpp>

int ftdb_maps(struct ftdb* ftdb, int show_stats);

#ifdef __cplusplus
}
#endif

PyObject * libftdb_create_ftdb(PyObject *self, PyObject *args, PyObject *kwargs);
PyObject * libftdb_parse_c_fmt_string(PyObject *self, PyObject *args);
extern PyObject *libftdb_ftdbError;

static PyMethodDef libftdb_methods[] = {
    {"create_ftdb",  (PyCFunction)libftdb_create_ftdb, METH_VARARGS | METH_KEYWORDS, "Create cached version of Function/Type database file"},
    {"parse_c_fmt_string",  libftdb_parse_c_fmt_string, METH_VARARGS, "Parse C format string and returns a list of types of detected parameters"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

#ifdef __cplusplus
__attribute__((unused))
#endif
static struct PyModuleDef libftdb_module = {
    PyModuleDef_HEAD_INIT,
    "libftdb",   								/* name of module */
    "Function/Type database Python tooling",	/* module documentation, may be NULL */
    -1,											/* size of per-interpreter state of the module,
                                                                   or -1 if the module keeps state in global variables. */
    libftdb_methods,
};

typedef struct {
    PyObject_HEAD
    int verbose;
    int debug;
    int init_done;
    const struct ftdb* ftdb;
    struct stringRefMap_node* ftdb_image_map_node;
    
    CUnflatten unflatten;
} libftdb_ftdb_object;

void libftdb_ftdb_dealloc(libftdb_ftdb_object* self);
PyObject* libftdb_ftdb_repr(PyObject* self);
PyObject* libftdb_ftdb_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
int libftdb_ftdb_init(libftdb_ftdb_object* self, PyObject* args, PyObject* kwds);
int libftdb_ftdb_bool(PyObject* self);
PyObject* libftdb_ftdb_load(libftdb_ftdb_object* self, PyObject* args, PyObject* kwargs);
PyObject* libftdb_ftdb_get_refcount(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_version(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_module(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_directory(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_release(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_sources(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_sources_as_dict(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_sources_as_list(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_modules(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_modules_as_dict(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_modules_as_list(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcdecls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_unresolvedfuncs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_unresolvedfuncs_as_list(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_globals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_types(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_fops(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_calls_no_asm(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_calls_no_known(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_func_calls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_func_refs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_funrefs_no_asm(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_funrefs_no_known(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_globs_tree_globalrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_types_tree_refs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_types_tree_usedrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_static_funcs_map(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_init_data(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_known_data(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_BAS(PyObject* self, void* closure);
PyObject* libftdb_ftdb_get_func_fptrs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdb_methods[] = {
    {"load",(PyCFunction)libftdb_ftdb_load,METH_VARARGS|METH_KEYWORDS,"Load the database cache file"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMappingMethods libftdb_ftdb_mapping_methods = {
        .mp_subscript = libftdb_ftdb_mp_subscript,
};

static PyGetSetDef libftdb_ftdb_getset[] = {
    {"__refcount__",libftdb_ftdb_get_refcount,0,"ftdb object reference count",0},
    {"version",libftdb_ftdb_get_version,0,"ftdb object version",0},
    {"module",libftdb_ftdb_get_module,0,"ftdb object module",0},
    {"directory",libftdb_ftdb_get_directory,0,"ftdb object directory",0},
    {"release",libftdb_ftdb_get_release,0,"ftdb object release",0},
    {"sources",libftdb_ftdb_get_sources,0,"ftdb sources object",0},
    {"source_info",libftdb_ftdb_get_sources_as_dict,0,"ftdb source info object",0},
    {"modules",libftdb_ftdb_get_modules,0,"ftdb modules object",0},
    {"module_info",libftdb_ftdb_get_modules_as_dict,0,"ftdb source info object",0},
    {"funcs",libftdb_ftdb_get_funcs,0,"ftdb funcs object",0},
    {"funcdecls",libftdb_ftdb_get_funcdecls,0,"ftdb funcdecls object",0},
    {"unresolvedfuncs",libftdb_ftdb_get_unresolvedfuncs,0,"ftdb unresolvedfuncs object",0},
    {"globals",libftdb_ftdb_get_globals,0,"ftdb globals object",0},
    {"types",libftdb_ftdb_get_types,0,"ftdb types object",0},
    {"fops",libftdb_ftdb_get_fops,0,"ftdb fops object",0},
    {"funcs_tree_calls_no_asm",libftdb_ftdb_get_funcs_tree_calls_no_asm,0,"ftdb funcs_tree_calls_no_asm object",0},
    {"funcs_tree_calls_no_known",libftdb_ftdb_get_funcs_tree_calls_no_known,0,"ftdb funcs_tree_calls_no_known object",0},
    {"funcs_tree_calls_no_known_no_asm",libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm,0,"ftdb funcs_tree_calls_no_known_no_asm object",0},
    {"funcs_tree_func_calls",libftdb_ftdb_get_funcs_tree_func_calls,0,"ftdb funcs_tree_func_calls object",0},
    {"funcs_tree_func_refs",libftdb_ftdb_get_funcs_tree_func_refs,0,"ftdb funcs_tree_func_refs object",0},
    {"funcs_tree_funrefs_no_asm",libftdb_ftdb_get_funcs_tree_funrefs_no_asm,0,"ftdb funcs_tree_funrefs_no_asm object",0},
    {"funcs_tree_funrefs_no_known",libftdb_ftdb_get_funcs_tree_funrefs_no_known,0,"ftdb funcs_tree_funrefs_no_known object",0},
    {"funcs_tree_funrefs_no_known_no_asm",libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm,0,"ftdb funcs_tree_funrefs_no_known_no_asm object",0},
    {"globs_tree_globalrefs",libftdb_ftdb_get_globs_tree_globalrefs,0,"ftdb globs_tree_globalrefs object",0},
    {"types_tree_refs",libftdb_ftdb_get_types_tree_refs,0,"ftdb types_tree_refs object",0},
    {"types_tree_usedrefs",libftdb_ftdb_get_types_tree_usedrefs,0,"ftdb types_tree_usedrefs object",0},
    {"static_funcs_map",libftdb_ftdb_get_static_funcs_map,0,"ftdb static_funcs_map object",0},
    {"init_data",libftdb_ftdb_get_init_data,0,"ftdb init_data object",0},
    {"known_data",libftdb_ftdb_get_known_data,0,"ftdb known_data object",0},
    {"BAS",libftdb_ftdb_get_BAS,0,"ftdb BAS object",0},
    {"func_fptrs",libftdb_ftdb_get_func_fptrs,0,"ftdb func_fptrs object",0},
    {0,0,0,0,0},
};

static PyMemberDef libftdb_ftdb_members[] = {
    {0}  /* Sentinel */
};

static PyNumberMethods libftdb_ftdb_number_methods = {
        .nb_bool = libftdb_ftdb_bool,
};

static PySequenceMethods libftdb_ftdb_sequence_methods = {
    .sq_contains = libftdb_ftdb_sq_contains
};

#ifdef __cplusplus
__attribute__((unused))
#endif
static PyTypeObject libftdb_ftdbType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    "libftdb.ftdb",		               			/* tp_name */
    sizeof(libftdb_ftdb_object), 				/* tp_basicsize */
    0,                              			/* tp_itemsize */
    /* Methods to implement standard operations */
    (destructor)libftdb_ftdb_dealloc,    	  	/* tp_dealloc */
    0,                              			/* tp_vectorcall_offset */
    0,                              			/* tp_getattr */
    0,                              			/* tp_setattr */
    0,                              			/* tp_as_async */
    (reprfunc)libftdb_ftdb_repr,    			/* tp_repr */
    /* Method suites for standard classes */
    &libftdb_ftdb_number_methods,       	 	/* tp_as_number */
    &libftdb_ftdb_sequence_methods,   			/* tp_as_sequence */
    &libftdb_ftdb_mapping_methods,	  			/* tp_as_mapping */
    /* More standard operations (here for binary compatibility) */
    0,                              			/* tp_hash */
    0,                              			/* tp_call */
    0,                              			/* tp_str */
    0,                              			/* tp_getattro */
    0,                              			/* tp_setattro */
    /* Functions to access object as input/output buffer */
    0,                              			/* tp_as_buffer */
    /* Flags to define presence of optional/expanded features */
    Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
    "libftdb ftdb object",                 		/* tp_doc */
    /* Assigned meaning in release 2.0 */
    /* call function for all accessible objects */
    0,                              			/* tp_traverse */
    /* delete references to contained objects */
    0,                              			/* tp_clear */
    /* Assigned meaning in release 2.1 */
    /* rich comparisons */
    0,                              			/* tp_richcompare */
    /* weak reference enabler */
    0,                              			/* tp_weaklistoffset */
    /* Iterators */
    0,						           			/* tp_iter */
    0,                              			/* tp_iternext */
    /* Attribute descriptor and subclassing stuff */
    libftdb_ftdb_methods,           			/* tp_methods */
    libftdb_ftdb_members,           			/* tp_members */
    libftdb_ftdb_getset,  	     				/* tp_getset */
    0,                              			/* tp_base */
    0,                              			/* tp_dict */
    0,                              			/* tp_descr_get */
    0,                              			/* tp_descr_set */
    0,                              			/* tp_dictoffset */
    (initproc)libftdb_ftdb_init,    			/* tp_init */
    0,                              			/* tp_alloc */
    libftdb_ftdb_new,            				/* tp_new */
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_modules_object;

void libftdb_ftdb_modules_dealloc(libftdb_ftdb_modules_object* self);
PyObject* libftdb_ftdb_modules_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_modules_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_modules_sq_length(PyObject* self);
PyObject* libftdb_ftdb_modules_getiter(PyObject* self);
PyObject* libftdb_ftdb_modules_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_modules_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbModules_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbModules_sequence_methods = {
    .sq_length = libftdb_ftdb_modules_sq_length,
    .sq_contains = libftdb_ftdb_modules_sq_contains
};

static PyMemberDef libftdb_ftdbModules_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbModules_getset[] = {
    {0,0,0,0,0},
};

static PyMappingMethods libftdb_ftdbModules_mapping_methods = {
        .mp_subscript = libftdb_ftdb_modules_mp_subscript,
};

static PyTypeObject libftdb_ftdbModulesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbModules",
    .tp_basicsize = sizeof(libftdb_ftdbModulesType),
    .tp_dealloc = (destructor)libftdb_ftdb_modules_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_modules_repr,
    .tp_as_sequence = &libftdb_ftdbModules_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbModules_mapping_methods,
    .tp_doc = "libftdb ftdbModules object",
    .tp_iter = libftdb_ftdb_modules_getiter,
    .tp_methods = libftdb_ftdbModules_methods,
    .tp_members = libftdb_ftdbModules_members,
    .tp_getset = &libftdb_ftdbModules_getset[0],
    .tp_new = libftdb_ftdb_modules_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_modules_object* modules;
} libftdb_ftdb_modules_iter_object;

void libftdb_ftdb_modules_iter_dealloc(libftdb_ftdb_modules_iter_object* self);
PyObject* libftdb_ftdb_modules_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_modules_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_modules_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbModulesIter_sequence_methods = {
        .sq_length = libftdb_ftdb_modules_iter_sq_length,
};

static PyTypeObject libftdb_ftdbModulesIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbModulesIter",
    .tp_basicsize = sizeof(libftdb_ftdbModulesIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_modules_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbModulesIter_sequence_methods,
    .tp_doc = "libftdb ftdb modules iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_modules_iter_next,
    .tp_new = libftdb_ftdb_modules_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_sources_object;

void libftdb_ftdb_sources_dealloc(libftdb_ftdb_sources_object* self);
PyObject* libftdb_ftdb_sources_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_sources_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_sources_sq_length(PyObject* self);
PyObject* libftdb_ftdb_sources_getiter(PyObject* self);
PyObject* libftdb_ftdb_sources_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_sources_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbSources_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbSources_sequence_methods = {
    .sq_length = libftdb_ftdb_sources_sq_length,
    .sq_contains = libftdb_ftdb_sources_sq_contains
};

static PyMemberDef libftdb_ftdbSources_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbSources_getset[] = {
    {0,0,0,0,0},
};

static PyMappingMethods libftdb_ftdbSources_mapping_methods = {
        .mp_subscript = libftdb_ftdb_sources_mp_subscript,
};

static PyTypeObject libftdb_ftdbSourcesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbSources",
    .tp_basicsize = sizeof(libftdb_ftdbSourcesType),
    .tp_dealloc = (destructor)libftdb_ftdb_sources_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_sources_repr,
    .tp_as_sequence = &libftdb_ftdbSources_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbSources_mapping_methods,
    .tp_doc = "libftdb ftdbSources object",
    .tp_iter = libftdb_ftdb_sources_getiter,
    .tp_methods = libftdb_ftdbSources_methods,
    .tp_members = libftdb_ftdbSources_members,
    .tp_getset = &libftdb_ftdbSources_getset[0],
    .tp_new = libftdb_ftdb_sources_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_sources_object* sources;
} libftdb_ftdb_sources_iter_object;

void libftdb_ftdb_sources_iter_dealloc(libftdb_ftdb_sources_iter_object* self);
PyObject* libftdb_ftdb_sources_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_sources_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_sources_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbSourcesIter_sequence_methods = {
        .sq_length = libftdb_ftdb_sources_iter_sq_length,
};

static PyTypeObject libftdb_ftdbSourcesIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbSourcesIter",
    .tp_basicsize = sizeof(libftdb_ftdbSourcesIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_sources_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbSourcesIter_sequence_methods,
    .tp_doc = "libftdb ftdb sources iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_sources_iter_next,
    .tp_new = libftdb_ftdb_sources_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_funcs_object;

void libftdb_ftdb_funcs_dealloc(libftdb_ftdb_funcs_object* self);
PyObject* libftdb_ftdb_funcs_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_funcs_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_funcs_sq_length(PyObject* self);
PyObject* libftdb_ftdb_funcs_getiter(PyObject* self);
PyObject* libftdb_ftdb_funcs_mp_subscript(PyObject* self, PyObject* slice);
PyObject* libftdb_ftdb_funcs_entry_by_id(libftdb_ftdb_funcs_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcs_entry_by_hash(libftdb_ftdb_funcs_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcs_entry_by_name(libftdb_ftdb_funcs_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcs_contains_id(libftdb_ftdb_funcs_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcs_contains_hash(libftdb_ftdb_funcs_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcs_contains_name(libftdb_ftdb_funcs_object *self, PyObject *args);
int libftdb_ftdb_funcs_sq_contains(PyObject* self, PyObject* key);
PyObject* libftdb_ftdb_funcs_get_refcount(PyObject* self, void* closure);

static PyMethodDef libftdb_ftdbFuncs_methods[] = {
    {"entry_by_id",(PyCFunction)libftdb_ftdb_funcs_entry_by_id,METH_VARARGS,"Returns the ftdb func entry with a given id"},
    {"contains_id",(PyCFunction)libftdb_ftdb_funcs_contains_id,METH_VARARGS,"Check whether there is a func entry with a given id"},
    {"entry_by_hash",(PyCFunction)libftdb_ftdb_funcs_entry_by_hash,METH_VARARGS,"Returns the ftdb func entry with a given hash value"},
    {"contains_hash",(PyCFunction)libftdb_ftdb_funcs_contains_hash,METH_VARARGS,"Check whether there is a func entry with a given hash"},
    {"entry_by_name",(PyCFunction)libftdb_ftdb_funcs_entry_by_name,METH_VARARGS,"Returns the list of ftdb func entries with a given name"},
    {"contains_name",(PyCFunction)libftdb_ftdb_funcs_contains_name,METH_VARARGS,"Check whether there is a func entry with a given name"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncs_sequence_methods = {
    .sq_length = libftdb_ftdb_funcs_sq_length,
    .sq_contains = libftdb_ftdb_funcs_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncs_mapping_methods = {
        .mp_subscript = libftdb_ftdb_funcs_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncs_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncs_getset[] = {
    {"__refcount__",libftdb_ftdb_funcs_get_refcount,0,"ftdb funcs object reference count",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncs",
    .tp_basicsize = sizeof(libftdb_ftdbFuncsType),
    .tp_dealloc = (destructor)libftdb_ftdb_funcs_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_funcs_repr,
    .tp_as_sequence = &libftdb_ftdbFuncs_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncs_mapping_methods,
    .tp_doc = "libftdb ftdbFuncs object",
    .tp_iter = libftdb_ftdb_funcs_getiter,
    .tp_methods = libftdb_ftdbFuncs_methods,
    .tp_members = libftdb_ftdbFuncs_members,
    .tp_getset = &libftdb_ftdbFuncs_getset[0],
    .tp_new = libftdb_ftdb_funcs_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_funcs_object* funcs;
} libftdb_ftdb_funcs_iter_object;

void libftdb_ftdb_funcs_iter_dealloc(libftdb_ftdb_funcs_iter_object* self);
PyObject* libftdb_ftdb_funcs_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_funcs_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_funcs_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbFuncsIter_sequence_methods = {
        .sq_length = libftdb_ftdb_funcs_iter_sq_length,
};

static PyTypeObject libftdb_ftdbFuncsIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncsIter",
    .tp_basicsize = sizeof(libftdb_ftdbFuncsIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_funcs_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbFuncsIter_sequence_methods,
    .tp_doc = "libftdb ftdb funcs iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_funcs_iter_next,
    .tp_new = libftdb_ftdb_funcs_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb_func_entry* entry;
    unsigned long index;
    const libftdb_ftdb_funcs_object* funcs;
} libftdb_ftdb_func_entry_object;

void libftdb_ftdb_func_entry_dealloc(libftdb_ftdb_func_entry_object* self);
PyObject* libftdb_ftdb_func_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_entry_json(libftdb_ftdb_func_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_entry_get_object_refcount(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_id(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_hash(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_name(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_has_namespace(libftdb_ftdb_func_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_entry_has_classOuterFn(libftdb_ftdb_func_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_entry_has_class(libftdb_ftdb_func_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_entry_get_namespace(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_fid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_fids(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_mids(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_nargs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_variadic(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_firstNonDeclStmt(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_inline(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_template(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_classOuterFn(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_linkage(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_linkage_string(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_member(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_class(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_classid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_attributes(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_cshash(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_template_parameters(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_body(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_unpreprocessed_body(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_declbody(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_signature(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_declhash(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_location(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_start_loc(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_end_loc(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_refcount(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_declcount(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_integer_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_character_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_floating_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_string_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_taint(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_calls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_call_info(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_callrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_refcalls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_refcall_info(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_refcallrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_switches(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_csmap(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_macro_expansions(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_locals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_derefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_ifs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_asms(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_globalrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_globalrefInfo(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_funrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_refs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_decls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_get_types(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_entry_sq_contains(PyObject* self, PyObject* key);
PyObject* libftdb_ftdb_func_entry_deepcopy(PyObject* self, PyObject* memo);
Py_hash_t libftdb_ftdb_func_entry_hash(PyObject *o);
PyObject* libftdb_ftdb_func_entry_richcompare(PyObject *self, PyObject *other, int op);


static PyMethodDef libftdb_ftdbFuncEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_func_entry_json,METH_VARARGS,"Returns the json representation of the ftdb func entry"},
    {"has_namespace",(PyCFunction)libftdb_ftdb_func_entry_has_namespace,METH_VARARGS,"Returns True if func entry contains namespace information"},
    {"has_classOuterFn",(PyCFunction)libftdb_ftdb_func_entry_has_classOuterFn,METH_VARARGS,"Returns True if func entry contains outer function class information"},
    {"has_class",(PyCFunction)libftdb_ftdb_func_entry_has_class,METH_VARARGS,"Returns True if func entry is contained within a class"},
    {"__deepcopy__",(PyCFunction)libftdb_ftdb_func_entry_deepcopy,METH_O,"Deep copy of an object"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMemberDef libftdb_ftdbFuncEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncEntry_getset[] = {
    {"__refcount__",libftdb_ftdb_func_entry_get_object_refcount,0,"ftdb func entry object reference count",0},
    {"id",libftdb_ftdb_func_entry_get_id,0,"ftdb func entry id value",0},
    {"hash",libftdb_ftdb_func_entry_get_hash,0,"ftdb func entry hash value",0},
    {"name",libftdb_ftdb_func_entry_get_name,0,"ftdb func entry name value",0},
    {"namespace",libftdb_ftdb_func_entry_get_namespace,0,"ftdb func entry namespace value",0},
    {"fid",libftdb_ftdb_func_entry_get_fid,0,"ftdb func entry fid value",0},
    {"fids",libftdb_ftdb_func_entry_get_fids,0,"ftdb func entry fids value",0},
    {"mids",libftdb_ftdb_func_entry_get_mids,0,"ftdb func entry mids value",0},
    {"nargs",libftdb_ftdb_func_entry_get_nargs,0,"ftdb func entry nargs value",0},
    {"variadic",libftdb_ftdb_func_entry_get_variadic,0,"ftdb func entry is variadic value",0},
    {"firstNonDeclStmt",libftdb_ftdb_func_entry_get_firstNonDeclStmt,0,"ftdb func entry firstNonDeclStmt value",0},
    {"inline",libftdb_ftdb_func_entry_get_inline,0,"ftdb func entry is inline value",0},
    {"template",libftdb_ftdb_func_entry_get_template,0,"ftdb func entry is template value",0},
    {"classOuterFn",libftdb_ftdb_func_entry_get_classOuterFn,0,"ftdb func entry classOuterFn value",0},
    {"linkage_string",libftdb_ftdb_func_entry_get_linkage_string,0,"ftdb func entry linkage string value",0},
    {"linkage",libftdb_ftdb_func_entry_get_linkage,0,"ftdb func entry linkage value",0},
    {"member",libftdb_ftdb_func_entry_get_member,0,"ftdb func entry is member value",0},
    {"class",libftdb_ftdb_func_entry_get_class,0,"ftdb func entry class value",0},
    {"classid",libftdb_ftdb_func_entry_get_classid,0,"ftdb func entry classid value",0},
    {"attributes",libftdb_ftdb_func_entry_get_attributes,0,"ftdb func entry attributes value",0},
    {"cshash",libftdb_ftdb_func_entry_get_cshash,0,"ftdb func entry cshash value",0},
    {"template_parameters",libftdb_ftdb_func_entry_get_template_parameters,0,"ftdb func entry template_parameters value",0},
    {"body",libftdb_ftdb_func_entry_get_body,0,"ftdb func entry body value",0},
    {"unpreprocessed_body",libftdb_ftdb_func_entry_get_unpreprocessed_body,0,"ftdb func entry unpreprocessed_body value",0},
    {"declbody",libftdb_ftdb_func_entry_get_declbody,0,"ftdb func entry declbody value",0},
    {"signature",libftdb_ftdb_func_entry_get_signature,0,"ftdb func entry signature value",0},
    {"declhash",libftdb_ftdb_func_entry_get_declhash,0,"ftdb func entry declhash value",0},
    {"location",libftdb_ftdb_func_entry_get_location,0,"ftdb func entry location value",0},
    {"start_loc",libftdb_ftdb_func_entry_get_start_loc,0,"ftdb func entry start_loc value",0},
    {"end_loc",libftdb_ftdb_func_entry_get_end_loc,0,"ftdb func entry end_loc value",0},
    {"refcount",libftdb_ftdb_func_entry_get_refcount,0,"ftdb func entry refcount value",0},
    {"declcount",libftdb_ftdb_func_entry_get_declcount,0,"ftdb func entry declcount value",0},
    {"literals",libftdb_ftdb_func_entry_get_literals,0,"ftdb func entry literals values",0},
    {"integer_literals",libftdb_ftdb_func_entry_get_integer_literals,0,"ftdb func entry integer_literals values",0},
    {"character_literals",libftdb_ftdb_func_entry_get_character_literals,0,"ftdb func entry character_literals values",0},
    {"floating_literals",libftdb_ftdb_func_entry_get_floating_literals,0,"ftdb func entry floating_literals values",0},
    {"string_literals",libftdb_ftdb_func_entry_get_string_literals,0,"ftdb func entry string_literals values",0},
    {"taint",libftdb_ftdb_func_entry_get_taint,0,"ftdb func entry taint data",0},
    {"calls",libftdb_ftdb_func_entry_get_calls,0,"ftdb func entry calls data",0},
    {"call_info",libftdb_ftdb_func_entry_get_call_info,0,"ftdb func entry call_info data",0},
    {"callrefs",libftdb_ftdb_func_entry_get_callrefs,0,"ftdb func entry callrefs data",0},
    {"refcalls",libftdb_ftdb_func_entry_get_refcalls,0,"ftdb func entry refcalls data",0},
    {"refcall_info",libftdb_ftdb_func_entry_get_refcall_info,0,"ftdb func entry refcall_info data",0},
    {"refcallrefs",libftdb_ftdb_func_entry_get_refcallrefs,0,"ftdb func entry refcallrefs data",0},
    {"switches",libftdb_ftdb_func_entry_get_switches,0,"ftdb func entry switches data",0},
    {"csmap",libftdb_ftdb_func_entry_get_csmap,0,"ftdb func entry csmap data",0},
    {"macro_expansions",libftdb_ftdb_func_entry_get_macro_expansions,0,"ftdb func entry macro_expansions data",0},
    {"locals",libftdb_ftdb_func_entry_get_locals,0,"ftdb func entry locals data",0},
    {"derefs",libftdb_ftdb_func_entry_get_derefs,0,"ftdb func entry derefs data",0},
    {"ifs",libftdb_ftdb_func_entry_get_ifs,0,"ftdb func entry ifs data",0},
    {"asms",libftdb_ftdb_func_entry_get_asms,0,"ftdb func entry asms data",0},
    {"globalrefs",libftdb_ftdb_func_entry_get_globalrefs,0,"ftdb func entry globalrefs data",0},
    {"globalrefInfo",libftdb_ftdb_func_entry_get_globalrefInfo,0,"ftdb func entry globalrefInfo data",0},
    {"funrefs",libftdb_ftdb_func_entry_get_funrefs,0,"ftdb func entry funrefs list",0},
    {"refs",libftdb_ftdb_func_entry_get_refs,0,"ftdb func entry refs list",0},
    {"decls",libftdb_ftdb_func_entry_get_decls,0,"ftdb func entry decls list",0},
    {"types",libftdb_ftdb_func_entry_get_types,0,"ftdb func entry types list",0},
    {0,0,0,0,0},
};

static PyMappingMethods libftdb_ftdbFuncEntry_mapping_methods = {
        .mp_subscript = libftdb_ftdb_func_entry_mp_subscript,
};

static PySequenceMethods libftdb_ftdbFuncEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_entry_sq_contains
};

static PyTypeObject libftdb_ftdbFuncsEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncsEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncEntry_mapping_methods,
    .tp_hash = (hashfunc)libftdb_ftdb_func_entry_hash,
    .tp_doc = "libftdb ftdb func entry object",
    .tp_richcompare = libftdb_ftdb_func_entry_richcompare,
    .tp_methods = libftdb_ftdbFuncEntry_methods,
    .tp_members = libftdb_ftdbFuncEntry_members,
    .tp_getset = &libftdb_ftdbFuncEntry_getset[0],
    .tp_new = libftdb_ftdb_func_entry_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    unsigned long is_refcall;
    struct call_info* entry;
    const libftdb_ftdb_func_entry_object* func_entry;
} libftdb_ftdb_func_callinfo_entry_object;

void libftdb_ftdb_func_callinfo_entry_dealloc(libftdb_ftdb_func_callinfo_entry_object* self);
PyObject* libftdb_ftdb_func_callinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_callinfo_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_callinfo_entry_get_start(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_get_end(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_get_ord(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_get_expr(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_get_loc(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_get_csid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_get_args(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_callinfo_entry_json(libftdb_ftdb_func_callinfo_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_callinfo_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_callinfo_entry_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncCallInfoEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_func_callinfo_entry_json,METH_VARARGS,"Returns the json representation of the callinfo entry"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncCallInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_callinfo_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncCallInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_callinfo_entry_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncCallInfoEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncCallInfoEntry_getset[] = {
    {"start",libftdb_ftdb_func_callinfo_entry_get_start,0,"ftdb func callinfo entry start value",0},
    {"end",libftdb_ftdb_func_callinfo_entry_get_end,0,"ftdb func callinfo entry end value",0},
    {"ord",libftdb_ftdb_func_callinfo_entry_get_ord,0,"ftdb func callinfo entry ord value",0},
    {"expr",libftdb_ftdb_func_callinfo_entry_get_expr,0,"ftdb func callinfo entry expr value",0},
    {"loc",libftdb_ftdb_func_callinfo_entry_get_loc,0,"ftdb func callinfo entry loc value",0},
    {"csid",libftdb_ftdb_func_callinfo_entry_get_csid,0,"ftdb func callinfo entry csid value",0},
    {"args",libftdb_ftdb_func_callinfo_entry_get_args,0,"ftdb func callinfo entry args values",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncCallInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncCallInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncCallInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_callinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_callinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncCallInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncCallInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncCallInfoEntry object",
    .tp_iter = 0,
    .tp_methods = libftdb_ftdbFuncCallInfoEntry_methods,
    .tp_members = libftdb_ftdbFuncCallInfoEntry_members,
    .tp_getset = &libftdb_ftdbFuncCallInfoEntry_getset[0],
    .tp_new = libftdb_ftdb_func_callinfo_entry_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct switch_info* entry;
    const libftdb_ftdb_func_entry_object* func_entry;
} libftdb_ftdb_func_switchinfo_entry_object;

void libftdb_ftdb_func_switchinfo_entry_dealloc(libftdb_ftdb_func_switchinfo_entry_object* self);
PyObject* libftdb_ftdb_func_switchinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_switchinfo_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_switchinfo_entry_get_condition(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_switchinfo_entry_get_cases(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_switchinfo_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_switchinfo_entry_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncSwitchInfoEntry_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncSwitchInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_switchinfo_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncSwitchInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_switchinfo_entry_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncSwitchInfoEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncSwitchInfoEntry_getset[] = {
    {"condition",libftdb_ftdb_func_switchinfo_entry_get_condition,0,"ftdb func switchinfo entry condition value",0},
    {"cases",libftdb_ftdb_func_switchinfo_entry_get_cases,0,"ftdb func switchinfo entry cases values",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncSwitchInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncSwitchInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncSwitchInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_switchinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_switchinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncSwitchInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncSwitchInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncSwitchInfoEntry object",
    .tp_iter = 0,
    .tp_methods = libftdb_ftdbFuncSwitchInfoEntry_methods,
    .tp_members = libftdb_ftdbFuncSwitchInfoEntry_members,
    .tp_getset = &libftdb_ftdbFuncSwitchInfoEntry_getset[0],
    .tp_new = libftdb_ftdb_func_switchinfo_entry_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct if_info* entry;
    const libftdb_ftdb_func_entry_object* func_entry;
} libftdb_ftdb_func_ifinfo_entry_object;

void libftdb_ftdb_func_ifinfo_entry_dealloc(libftdb_ftdb_func_ifinfo_entry_object* self);
PyObject* libftdb_ftdb_func_ifinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_ifinfo_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_ifinfo_entry_get_csid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_ifinfo_entry_get_refs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_ifinfo_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_ifinfo_entry_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncIfInfoEntry_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncIfInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_ifinfo_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncIfnfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_ifinfo_entry_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncIfInfoEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncIfInfoEntry_getset[] = {
    {"csid",libftdb_ftdb_func_ifinfo_entry_get_csid,0,"ftdb func ifinfo entry csid value",0},
    {"refs",libftdb_ftdb_func_ifinfo_entry_get_refs,0,"ftdb func ifinfo entry refs values",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncIfInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncIfInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncIfInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_ifinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_ifinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncIfInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncIfnfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncIfInfoEntry object",
    .tp_iter = 0,
    .tp_methods = libftdb_ftdbFuncIfInfoEntry_methods,
    .tp_members = libftdb_ftdbFuncIfInfoEntry_members,
    .tp_getset = &libftdb_ftdbFuncIfInfoEntry_getset[0],
    .tp_new = libftdb_ftdb_func_ifinfo_entry_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct local_info* entry;
    const libftdb_ftdb_func_entry_object* func_entry;
} libftdb_ftdb_func_localinfo_entry_object;

void libftdb_ftdb_func_localinfo_entry_dealloc(libftdb_ftdb_func_localinfo_entry_object* self);
PyObject* libftdb_ftdb_func_localinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_localinfo_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_localinfo_entry_get_id(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_get_name(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_is_parm(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_get_type(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_is_static(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_is_used(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_get_csid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_get_location(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_localinfo_entry_has_csid(libftdb_ftdb_func_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_localinfo_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_localinfo_entry_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncLocalInfoEntry_methods[] = {
    {"has_csid",(PyCFunction)libftdb_ftdb_func_localinfo_entry_has_csid,METH_VARARGS,
            "Returns True if func localinfo entry has the csid entry"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncLocalInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_localinfo_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncLocalInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_localinfo_entry_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncLocalInfoEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncLocalInfoEntry_getset[] = {
    {"id",libftdb_ftdb_func_localinfo_entry_get_id,0,"ftdb func localinfo entry id values",0},
    {"name",libftdb_ftdb_func_localinfo_entry_get_name,0,"ftdb func localinfo entry name values",0},
    {"parm",libftdb_ftdb_func_localinfo_entry_is_parm,0,"ftdb func localinfo entry is parm values",0},
    {"type",libftdb_ftdb_func_localinfo_entry_get_type,0,"ftdb func localinfo entry type values",0},
    {"static",libftdb_ftdb_func_localinfo_entry_is_static,0,"ftdb func localinfo entry is static values",0},
    {"used",libftdb_ftdb_func_localinfo_entry_is_used,0,"ftdb func localinfo entry is used values",0},
    {"csid",libftdb_ftdb_func_localinfo_entry_get_csid,0,"ftdb func localinfo entry csid values",0},
    {"location",libftdb_ftdb_func_localinfo_entry_get_location,0,"ftdb func localinfo entry location values",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncLocalInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncLocalInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncLocalInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_localinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_localinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncLocalInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncLocalInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncLocalInfoEntry object",
    .tp_iter = 0,
    .tp_methods = libftdb_ftdbFuncLocalInfoEntry_methods,
    .tp_members = libftdb_ftdbFuncLocalInfoEntry_members,
    .tp_getset = &libftdb_ftdbFuncLocalInfoEntry_getset[0],
    .tp_new = libftdb_ftdb_func_localinfo_entry_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct deref_info* entry;
    const libftdb_ftdb_func_entry_object* func_entry;
} libftdb_ftdb_func_derefinfo_entry_object;

void libftdb_ftdb_func_derefinfo_entry_dealloc(libftdb_ftdb_func_derefinfo_entry_object* self);
PyObject* libftdb_ftdb_func_derefinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_derefinfo_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_kind(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_kindname(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_offset(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_basecnt(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_offsetrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_expr(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_csid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_ords(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_member(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_type(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_access(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_shift(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_get_mcall(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_derefinfo_entry_json(libftdb_ftdb_func_derefinfo_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_derefinfo_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_derefinfo_entry_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncDerefInfoEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_func_derefinfo_entry_json,METH_VARARGS,"Returns the json representation of the deref entry"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncDerefInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_derefinfo_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncDerefInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_derefinfo_entry_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncDerefInfoEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncDerefInfoEntry_getset[] = {
    {"kind",libftdb_ftdb_func_derefinfo_entry_get_kind,0,"ftdb func derefinfo entry kind value",0},
    {"kindname",libftdb_ftdb_func_derefinfo_entry_get_kindname,0,"ftdb func derefinfo entry kind name value",0},
    {"offset",libftdb_ftdb_func_derefinfo_entry_get_offset,0,"ftdb func derefinfo entry offset value",0},
    {"basecnt",libftdb_ftdb_func_derefinfo_entry_get_basecnt,0,"ftdb func derefinfo entry basecnt value",0},
    {"offsetrefs",libftdb_ftdb_func_derefinfo_entry_get_offsetrefs,0,"ftdb func derefinfo entry offsetrefs values",0},
    {"expr",libftdb_ftdb_func_derefinfo_entry_get_expr,0,"ftdb func derefinfo entry expr value",0},
    {"csid",libftdb_ftdb_func_derefinfo_entry_get_csid,0,"ftdb func derefinfo entry csid value",0},
    {"ord",libftdb_ftdb_func_derefinfo_entry_get_ords,0,"ftdb func derefinfo entry ord values",0},
    {"ords",libftdb_ftdb_func_derefinfo_entry_get_ords,0,"ftdb func derefinfo entry ord values",0},
    {"member",libftdb_ftdb_func_derefinfo_entry_get_member,0,"ftdb func derefinfo entry member values",0},
    {"type",libftdb_ftdb_func_derefinfo_entry_get_type,0,"ftdb func derefinfo entry type values",0},
    {"access",libftdb_ftdb_func_derefinfo_entry_get_access,0,"ftdb func derefinfo entry access values",0},
    {"shift",libftdb_ftdb_func_derefinfo_entry_get_shift,0,"ftdb func derefinfo entry shift values",0},
    {"mcall",libftdb_ftdb_func_derefinfo_entry_get_mcall,0,"ftdb func derefinfo entry mcall values",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncDerefInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncDerefInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncDerefInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_derefinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_derefinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncDerefInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncDerefInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncDerefInfoEntry object",
    .tp_iter = 0,
    .tp_methods = libftdb_ftdbFuncDerefInfoEntry_methods,
    .tp_members = libftdb_ftdbFuncDerefInfoEntry_members,
    .tp_getset = &libftdb_ftdbFuncDerefInfoEntry_getset[0],
    .tp_new = libftdb_ftdb_func_derefinfo_entry_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long deref_index;
    unsigned long entry_index;
    struct offsetref_info* entry;
    const libftdb_ftdb_func_derefinfo_entry_object* deref_entry;
} libftdb_ftdb_func_offsetrefinfo_entry_object;

void libftdb_ftdb_func_offsetrefinfo_entry_dealloc(libftdb_ftdb_func_offsetrefinfo_entry_object* self);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_kind(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_kindname(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_id(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_mi(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_di(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_cast(PyObject* self, void* closure);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_has_cast(libftdb_ftdb_func_offsetrefinfo_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_json(libftdb_ftdb_func_offsetrefinfo_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_func_offsetrefinfo_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_func_offsetrefinfo_entry_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncOffsetrefInfoEntry_methods[] = {
    {"has_cast",(PyCFunction)libftdb_ftdb_func_offsetrefinfo_entry_has_cast,METH_VARARGS,"Returns True if the offsetref entry contains cast information"},
    {"json",(PyCFunction)libftdb_ftdb_func_offsetrefinfo_entry_json,METH_VARARGS,"Returns the json representation of the offsetref entry"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFuncOffsetrefInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_offsetrefinfo_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFuncOffsetrefInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_offsetrefinfo_entry_mp_subscript,
};

static PyMemberDef libftdb_ftdbFuncOffsetrefInfoEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncOffsetrefInfoEntry_getset[] = {
    {"kind",libftdb_ftdb_func_offsetrefinfo_entry_get_kind,0,"ftdb func offsetrefinfo entry kind value",0},
    {"kindname",libftdb_ftdb_func_offsetrefinfo_entry_get_kindname,0,"ftdb func offsetrefinfo entry kind name value",0},
    {"id",libftdb_ftdb_func_offsetrefinfo_entry_get_id,0,"ftdb func offsetrefinfo entry id value",0},
    {"mi",libftdb_ftdb_func_offsetrefinfo_entry_get_mi,0,"ftdb func offsetrefinfo entry mi value",0},
    {"di",libftdb_ftdb_func_offsetrefinfo_entry_get_di,0,"ftdb func offsetrefinfo entry di values",0},
    {"cast",libftdb_ftdb_func_offsetrefinfo_entry_get_cast,0,"ftdb func offsetrefinfo entry cast value",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncOffsetrefInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncOffsetrefInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncOffsetrefInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_offsetrefinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_offsetrefinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncOffsetrefInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncOffsetrefInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncOffsetrefInfoEntry object",
    .tp_iter = 0,
    .tp_methods = libftdb_ftdbFuncOffsetrefInfoEntry_methods,
    .tp_members = libftdb_ftdbFuncOffsetrefInfoEntry_members,
    .tp_getset = &libftdb_ftdbFuncOffsetrefInfoEntry_getset[0],
    .tp_new = libftdb_ftdb_func_offsetrefinfo_entry_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_funcdecls_object;

void libftdb_ftdb_funcdecls_dealloc(libftdb_ftdb_funcdecls_object* self);
PyObject* libftdb_ftdb_funcdecls_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_funcdecls_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_funcdecls_sq_length(PyObject* self);
PyObject* libftdb_ftdb_funcdecls_getiter(PyObject* self);
PyObject* libftdb_ftdb_funcdecls_mp_subscript(PyObject* self, PyObject* slice);
PyObject* libftdb_ftdb_funcdecls_entry_by_id(libftdb_ftdb_funcdecls_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecls_contains_id(libftdb_ftdb_funcdecls_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecls_entry_by_hash(libftdb_ftdb_funcdecls_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecls_contains_hash(libftdb_ftdb_funcdecls_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecls_entry_by_name(libftdb_ftdb_funcdecls_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecls_contains_name(libftdb_ftdb_funcdecls_object *self, PyObject *args);
int libftdb_ftdb_funcdecls_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbFuncdecls_methods[] = {
    {"entry_by_id",(PyCFunction)libftdb_ftdb_funcdecls_entry_by_id,METH_VARARGS,"Returns the ftdb funcdecl entry with a given id"},
    {"contains_id",(PyCFunction)libftdb_ftdb_funcdecls_contains_id,METH_VARARGS,"Check whether there is a funcdecl entry with a given id"},
    {"entry_by_hash",(PyCFunction)libftdb_ftdb_funcdecls_entry_by_hash,METH_VARARGS,"Returns the ftdb funcdecl entry with a given hash value"},
    {"contains_hash",(PyCFunction)libftdb_ftdb_funcdecls_contains_hash,METH_VARARGS,"Check whether there is a funcdecl entry with a given hash"},
    {"entry_by_name",(PyCFunction)libftdb_ftdb_funcdecls_entry_by_name,METH_VARARGS,"Returns the ftdb funcdecl entry with a given name"},
    {"contains_name",(PyCFunction)libftdb_ftdb_funcdecls_contains_name,METH_VARARGS,"Check whether there is a funcdecl entry with a given name"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMappingMethods libftdb_ftdbFuncdecls_mapping_methods = {
        .mp_subscript = libftdb_ftdb_funcdecls_mp_subscript,
};

static PySequenceMethods libftdb_ftdbFuncdecls_sequence_methods = {
    .sq_length = libftdb_ftdb_funcdecls_sq_length,
    .sq_contains = libftdb_ftdb_funcdecls_sq_contains
};

static PyMemberDef libftdb_ftdbFuncdecls_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbFuncdecls_getset[] = {
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncdeclsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncsdecl",
    .tp_basicsize = sizeof(libftdb_ftdbFuncdeclsType),
    .tp_dealloc = (destructor)libftdb_ftdb_funcdecls_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_funcdecls_repr,
    .tp_as_sequence = &libftdb_ftdbFuncdecls_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncdecls_mapping_methods,
    .tp_doc = "libftdb ftdbFuncdecls object",
    .tp_iter = libftdb_ftdb_funcdecls_getiter,
    .tp_methods = libftdb_ftdbFuncdecls_methods,
    .tp_members = libftdb_ftdbFuncdecls_members,
    .tp_getset = &libftdb_ftdbFuncdecls_getset[0],
    .tp_new = libftdb_ftdb_funcdecls_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_funcdecls_object* funcdecls;
} libftdb_ftdb_funcdecls_iter_object;

void libftdb_ftdb_funcdecls_iter_dealloc(libftdb_ftdb_funcdecls_iter_object* self);
PyObject* libftdb_ftdb_funcdecls_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_funcdecls_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_funcdecls_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbFuncdeclsIter_sequence_methods = {
        .sq_length = libftdb_ftdb_funcdecls_iter_sq_length,
};

static PyTypeObject libftdb_ftdbFuncdeclsIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncdeclsIter",
    .tp_basicsize = sizeof(libftdb_ftdbFuncdeclsIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_funcdecls_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbFuncdeclsIter_sequence_methods,
    .tp_doc = "libftdb ftdb funcdecls iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_funcdecls_iter_next,
    .tp_new = libftdb_ftdb_funcdecls_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb_funcdecl_entry* entry;
    unsigned long index;
    const libftdb_ftdb_funcdecls_object* funcdecls;
} libftdb_ftdb_funcdecl_entry_object;

void libftdb_ftdb_funcdecl_entry_dealloc(libftdb_ftdb_funcdecl_entry_object* self);
PyObject* libftdb_ftdb_funcdecl_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_funcdecl_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_funcdecl_entry_json(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecl_entry_has_namespace(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecl_entry_has_class(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_funcdecl_entry_get_id(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_name(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_namespace(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_fid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_nargs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_variadic(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_template(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_linkage(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_linkage_string(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_member(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_class(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_classid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_template_parameters(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_decl(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_signature(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_declhash(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_location(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_refcount(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_get_types(PyObject* self, void* closure);
PyObject* libftdb_ftdb_funcdecl_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_funcdecl_entry_sq_contains(PyObject* self, PyObject* key);
PyObject* libftdb_ftdb_funcdecl_entry_deepcopy(PyObject* self, PyObject* memo);

static PyMethodDef libftdb_ftdbFuncdeclEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_funcdecl_entry_json,METH_VARARGS,"Returns the json representation of the ftdb funcdecl entry"},
    {"has_namespace",(PyCFunction)libftdb_ftdb_funcdecl_entry_has_namespace,METH_VARARGS,
            "Returns True if funcdecl entry contains namespace information"},
    {"has_class",(PyCFunction)libftdb_ftdb_funcdecl_entry_has_class,METH_VARARGS,
            "Returns True if funcdecl entry is contained within a class"},
    {"__deepcopy__",(PyCFunction)libftdb_ftdb_funcdecl_entry_deepcopy,METH_O,"Deep copy of an object"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMemberDef libftdb_ftdbFuncdeclEntry_members[] = {
    {0}  /* Sentinel */
};

static PyMappingMethods libftdb_ftdbFuncdeclEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_funcdecl_entry_mp_subscript,
};

static PySequenceMethods libftdb_ftdbFuncdeclEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_funcdecl_entry_sq_contains
};

static PyGetSetDef libftdb_ftdbFuncdeclEntry_getset[] = {
    {"id",libftdb_ftdb_funcdecl_entry_get_id,0,"ftdb funcdecl entry id value",0},
    {"name",libftdb_ftdb_funcdecl_entry_get_name,0,"ftdb funcdecl entry name value",0},
    {"namespace",libftdb_ftdb_funcdecl_entry_get_namespace,0,"ftdb funcdecl entry namespace value",0},
    {"fid",libftdb_ftdb_funcdecl_entry_get_fid,0,"ftdb funcdecl entry fid value",0},
    {"nargs",libftdb_ftdb_funcdecl_entry_get_nargs,0,"ftdb funcdecl entry nargs value",0},
    {"variadic",libftdb_ftdb_funcdecl_entry_get_variadic,0,"ftdb funcdecl entry is variadic value",0},
    {"template",libftdb_ftdb_funcdecl_entry_get_template,0,"ftdb funcdecl entry is template value",0},
    {"linkage",libftdb_ftdb_funcdecl_entry_get_linkage,0,"ftdb funcdecl entry linkage value",0},
    {"linkage_string",libftdb_ftdb_funcdecl_entry_get_linkage_string,0,"ftdb funcdecl entry linkage value",0},
    {"member",libftdb_ftdb_funcdecl_entry_get_member,0,"ftdb funcdecl entry is member value",0},
    {"class",libftdb_ftdb_funcdecl_entry_get_class,0,"ftdb funcdecl entry class value",0},
    {"classid",libftdb_ftdb_funcdecl_entry_get_classid,0,"ftdb funcdecl entry classid value",0},
    {"template_parameters",libftdb_ftdb_funcdecl_entry_get_template_parameters,0,
            "ftdb funcdecl entry template_parameters value",0},
    {"decl",libftdb_ftdb_funcdecl_entry_get_decl,0,"ftdb funcdecl entry decl value",0},
    {"signature",libftdb_ftdb_funcdecl_entry_get_signature,0,"ftdb funcdecl entry signature value",0},
    {"declhash",libftdb_ftdb_funcdecl_entry_get_declhash,0,"ftdb funcdecl entry declhash value",0},
    {"location",libftdb_ftdb_funcdecl_entry_get_location,0,"ftdb funcdecl entry location value",0},
    {"refcount",libftdb_ftdb_funcdecl_entry_get_refcount,0,"ftdb funcdecl entry refcount value",0},
    {"types",libftdb_ftdb_funcdecl_entry_get_types,0,"ftdb funcdecl entry types list",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFuncdeclsEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncdeclEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncdeclsEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_funcdecl_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_funcdecl_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncdeclEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncdeclEntry_mapping_methods,
    .tp_doc = "libftdb ftdb funcdecl entry object",
    .tp_methods = libftdb_ftdbFuncdeclEntry_methods,
    .tp_members = libftdb_ftdbFuncdeclEntry_members,
    .tp_getset = &libftdb_ftdbFuncdeclEntry_getset[0],
    .tp_new = libftdb_ftdb_funcdecl_entry_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_unresolvedfuncs_object;

void libftdb_ftdb_unresolvedfuncs_dealloc(libftdb_ftdb_unresolvedfuncs_object* self);
PyObject* libftdb_ftdb_unresolvedfuncs_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_unresolvedfuncs_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_unresolvedfuncs_sq_length(PyObject* self);
PyObject* libftdb_ftdb_unresolvedfuncs_getiter(PyObject* self);
PyObject* libftdb_ftdb_unresolvedfuncs_entry_deepcopy(PyObject* self, PyObject* memo);

static PyMethodDef libftdb_ftdbUnresolvedfuncs_methods[] = {
    {"__deepcopy__",(PyCFunction)libftdb_ftdb_unresolvedfuncs_entry_deepcopy,METH_O,"Deep copy of an object"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbUnresolvedfuncs_sequence_methods = {
    .sq_length = libftdb_ftdb_unresolvedfuncs_sq_length,
};

static PyMemberDef libftdb_ftdbUnresolvedfuncs_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbUnresolvedfuncs_getset[] = {
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbUnresolvedfuncsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbUnresolvedfuncs",
    .tp_basicsize = sizeof(libftdb_ftdbUnresolvedfuncsType),
    .tp_dealloc = (destructor)libftdb_ftdb_unresolvedfuncs_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_unresolvedfuncs_repr,
    .tp_as_sequence = &libftdb_ftdbUnresolvedfuncs_sequence_methods,
    .tp_doc = "libftdb ftdbUnresolvedfuncs object",
    .tp_iter = libftdb_ftdb_unresolvedfuncs_getiter,
    .tp_methods = libftdb_ftdbUnresolvedfuncs_methods,
    .tp_members = libftdb_ftdbUnresolvedfuncs_members,
    .tp_getset = &libftdb_ftdbUnresolvedfuncs_getset[0],
    .tp_new = libftdb_ftdb_unresolvedfuncs_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_unresolvedfuncs_object* unresolvedfuncs;
} libftdb_ftdb_unresolvedfuncs_iter_object;

void libftdb_ftdb_unresolvedfuncs_iter_dealloc(libftdb_ftdb_unresolvedfuncs_iter_object* self);
PyObject* libftdb_ftdb_unresolvedfuncs_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_unresolvedfuncs_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_unresolvedfuncs_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbUnresolvedfuncsIter_sequence_methods = {
        .sq_length = libftdb_ftdb_unresolvedfuncs_iter_sq_length,
};

static PyTypeObject libftdb_ftdbUnresolvedfuncsIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbUnresolvedfuncsIter",
    .tp_basicsize = sizeof(libftdb_ftdbUnresolvedfuncsIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_unresolvedfuncs_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbUnresolvedfuncsIter_sequence_methods,
    .tp_doc = "libftdb ftdb unresolvedfuncs iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_unresolvedfuncs_iter_next,
    .tp_new = libftdb_ftdb_unresolvedfuncs_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_globals_object;

void libftdb_ftdb_globals_dealloc(libftdb_ftdb_globals_object* self);
PyObject* libftdb_ftdb_globals_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_globals_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_globals_sq_length(PyObject* self);
PyObject* libftdb_ftdb_globals_getiter(PyObject* self);
PyObject* libftdb_ftdb_globals_mp_subscript(PyObject* self, PyObject* slice);
PyObject* libftdb_ftdb_globals_entry_by_id(libftdb_ftdb_globals_object *self, PyObject *args);
PyObject* libftdb_ftdb_globals_entry_by_hash(libftdb_ftdb_globals_object *self, PyObject *args);
PyObject* libftdb_ftdb_globals_entry_by_name(libftdb_ftdb_globals_object *self, PyObject *args);
PyObject* libftdb_ftdb_globals_contains_id(libftdb_ftdb_globals_object *self, PyObject *args);
PyObject* libftdb_ftdb_globals_contains_hash(libftdb_ftdb_globals_object *self, PyObject *args);
PyObject* libftdb_ftdb_globals_contains_name(libftdb_ftdb_globals_object *self, PyObject *args);
int libftdb_ftdb_globals_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbGlobals_methods[] = {
    {"entry_by_id",(PyCFunction)libftdb_ftdb_globals_entry_by_id,METH_VARARGS,"Returns the ftdb global entry with a given id"},
    {"contains_id",(PyCFunction)libftdb_ftdb_globals_contains_id,METH_VARARGS,"Check whether there is a global entry with a given id"},
    {"entry_by_hash",(PyCFunction)libftdb_ftdb_globals_entry_by_hash,METH_VARARGS,"Returns the ftdb global entry with a given hash value"},
    {"contains_hash",(PyCFunction)libftdb_ftdb_globals_contains_hash,METH_VARARGS,"Check whether there is a global entry with a given hash"},
    {"entry_by_name",(PyCFunction)libftdb_ftdb_globals_entry_by_name,METH_VARARGS,"Returns the list of ftdb global entries with a given name"},
    {"contains_name",(PyCFunction)libftdb_ftdb_globals_contains_name,METH_VARARGS,"Check whether there is a global entry with a given name"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbGlobals_sequence_methods = {
    .sq_length = libftdb_ftdb_globals_sq_length,
    .sq_contains = libftdb_ftdb_globals_sq_contains
};

static PyMappingMethods libftdb_ftdbGlobals_mapping_methods = {
        .mp_subscript = libftdb_ftdb_globals_mp_subscript,
};

static PyMemberDef libftdb_ftdbGlobals_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbGlobals_getset[] = {
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbGlobalsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbGlobals",
    .tp_basicsize = sizeof(libftdb_ftdbGlobalsType),
    .tp_dealloc = (destructor)libftdb_ftdb_globals_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_globals_repr,
    .tp_as_sequence = &libftdb_ftdbGlobals_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbGlobals_mapping_methods,
    .tp_doc = "libftdb ftdbGlobals object",
    .tp_iter = libftdb_ftdb_globals_getiter,
    .tp_methods = libftdb_ftdbGlobals_methods,
    .tp_members = libftdb_ftdbGlobals_members,
    .tp_getset = &libftdb_ftdbGlobals_getset[0],
    .tp_new = libftdb_ftdb_globals_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_globals_object* globals;
} libftdb_ftdb_globals_iter_object;

void libftdb_ftdb_globals_iter_dealloc(libftdb_ftdb_globals_iter_object* self);
PyObject* libftdb_ftdb_globals_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_globals_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_globals_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbGlobalsIter_sequence_methods = {
        .sq_length = libftdb_ftdb_globals_iter_sq_length,
};

static PyTypeObject libftdb_ftdbGlobalsIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbGlobalsIter",
    .tp_basicsize = sizeof(libftdb_ftdbGlobalsIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_globals_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbGlobalsIter_sequence_methods,
    .tp_doc = "libftdb ftdb globals iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_globals_iter_next,
    .tp_new = libftdb_ftdb_globals_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb_global_entry* entry;
    unsigned long index;
    const libftdb_ftdb_globals_object* globals;
} libftdb_ftdb_global_entry_object;

void libftdb_ftdb_global_entry_dealloc(libftdb_ftdb_global_entry_object* self);
PyObject* libftdb_ftdb_global_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_global_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_global_entry_json(libftdb_ftdb_global_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_global_entry_has_init(libftdb_ftdb_global_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_global_entry_get_name(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_hash(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_id(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_fid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_def(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_type(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_linkage(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_linkage_string(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_location(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_deftype(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_init(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_globalrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_refs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_funrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_decls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_mids(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_integer_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_character_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_floating_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_string_literals(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_get_hasinit(PyObject* self, void* closure);
PyObject* libftdb_ftdb_global_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_global_entry_sq_contains(PyObject* self, PyObject* key);
PyObject* libftdb_ftdb_global_entry_deepcopy(PyObject* self, PyObject* memo);

static PyMethodDef libftdb_ftdbGlobalEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_global_entry_json,METH_VARARGS,"Returns the json representation of the ftdb global entry"},
    {"has_init",(PyCFunction)libftdb_ftdb_global_entry_has_init,METH_VARARGS,"Returns True if global entry has init field"},
    {"__deepcopy__",(PyCFunction)libftdb_ftdb_global_entry_deepcopy,METH_O,"Deep copy of an object"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMemberDef libftdb_ftdbGlobalEntry_members[] = {
    {0}  /* Sentinel */
};

static PyMappingMethods libftdb_ftdbGlobalEntry_mapping_methods = {
       .mp_subscript = libftdb_ftdb_global_entry_mp_subscript,
};

static PySequenceMethods libftdb_ftdbGlobalEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_global_entry_sq_contains
};

static PyGetSetDef libftdb_ftdbGlobalEntry_getset[] = {
    {"name",libftdb_ftdb_global_entry_get_name,0,"ftdb global entry name value",0},
    {"hash",libftdb_ftdb_global_entry_get_hash,0,"ftdb global entry hash value",0},
    {"id",libftdb_ftdb_global_entry_get_id,0,"ftdb global entry id value",0},
    {"fid",libftdb_ftdb_global_entry_get_fid,0,"ftdb global entry fid value",0},
    {"defstring",libftdb_ftdb_global_entry_get_def,0,"ftdb global entry def value",0},
    {"type",libftdb_ftdb_global_entry_get_type,0,"ftdb global entry type value",0},
    {"linkage_string",libftdb_ftdb_global_entry_get_linkage_string,0,"ftdb global entry linkage string value",0},
    {"linkage",libftdb_ftdb_global_entry_get_linkage,0,"ftdb global entry linkage value",0},
    {"location",libftdb_ftdb_global_entry_get_location,0,"ftdb global entry location value",0},
    {"deftype",libftdb_ftdb_global_entry_get_deftype,0,"ftdb global entry deftype value",0},
    {"init",libftdb_ftdb_global_entry_get_init,0,"ftdb global entry init value",0},
    {"globalrefs",libftdb_ftdb_global_entry_get_globalrefs,0,"ftdb global entry globalrefs values",0},
    {"refs",libftdb_ftdb_global_entry_get_refs,0,"ftdb global entry refs values",0},
    {"funrefs",libftdb_ftdb_global_entry_get_funrefs,0,"ftdb global entry funrefs values",0},
    {"decls",libftdb_ftdb_global_entry_get_decls,0,"ftdb global entry decls values",0},
    {"mids",libftdb_ftdb_global_entry_get_mids,0,"ftdb global entry mids values",0},
    {"literals",libftdb_ftdb_global_entry_get_literals,0,"ftdb global entry literals values",0},
    {"integer_literals",libftdb_ftdb_global_entry_get_integer_literals,0,"ftdb global entry integer_literals values",0},
    {"character_literals",libftdb_ftdb_global_entry_get_character_literals,0,"ftdb global entry character_literals values",0},
    {"floating_literals",libftdb_ftdb_global_entry_get_floating_literals,0,"ftdb global entry floating_literals values",0},
    {"string_literals",libftdb_ftdb_global_entry_get_string_literals,0,"ftdb global entry string_literals values",0},
    {"hasinit",libftdb_ftdb_global_entry_get_hasinit,0,"ftdb global entry hasinit value",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbGlobalEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbGlobalEntry",
    .tp_basicsize = sizeof(libftdb_ftdbGlobalEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_global_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_global_entry_repr,
    .tp_as_sequence = &libftdb_ftdbGlobalEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbGlobalEntry_mapping_methods,
    .tp_doc = "libftdb ftdb global entry object",
    .tp_methods = libftdb_ftdbGlobalEntry_methods,
    .tp_members = libftdb_ftdbGlobalEntry_members,
    .tp_getset = &libftdb_ftdbGlobalEntry_getset[0],
    .tp_new = libftdb_ftdb_global_entry_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_types_object;

void libftdb_ftdb_types_dealloc(libftdb_ftdb_types_object* self);
PyObject* libftdb_ftdb_types_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_types_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_types_sq_length(PyObject* self);
PyObject* libftdb_ftdb_types_getiter(PyObject* self);
PyObject* libftdb_ftdb_types_mp_subscript(PyObject* self, PyObject* slice);
PyObject* libftdb_ftdb_types_contains_hash(libftdb_ftdb_types_object *self, PyObject *args);
PyObject* libftdb_ftdb_types_contains_id(libftdb_ftdb_types_object *self, PyObject *args);
PyObject* libftdb_ftdb_types_entry_by_hash(libftdb_ftdb_types_object *self, PyObject *args);
PyObject* libftdb_ftdb_types_entry_by_id(libftdb_ftdb_types_object *self, PyObject *args);
int libftdb_ftdb_types_sq_contains(PyObject* self, PyObject* key);

static PyMethodDef libftdb_ftdbTypes_methods[] = {
    {"entry_by_id",(PyCFunction)libftdb_ftdb_types_entry_by_id,METH_VARARGS,"Returns the ftdb types entry with a given id"},
    {"contains_id",(PyCFunction)libftdb_ftdb_types_contains_id,METH_VARARGS,"Check whether there is a types entry with a given id"},
    {"entry_by_hash",(PyCFunction)libftdb_ftdb_types_entry_by_hash,METH_VARARGS,"Returns the ftdb types entry with a given hash value"},
    {"contains_hash",(PyCFunction)libftdb_ftdb_types_contains_hash,METH_VARARGS,"Check whether there is a types entry with a given hash"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbTypes_sequence_methods = {
    .sq_length = libftdb_ftdb_types_sq_length,
    .sq_contains = libftdb_ftdb_types_sq_contains
};

static PyMappingMethods libftdb_ftdbTypes_mapping_methods = {
    .mp_subscript = libftdb_ftdb_types_mp_subscript,
};

static PyMemberDef libftdb_ftdbTypes_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libftdb_ftdbTypes_getset[] = {
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbTypesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbTypes",
    .tp_basicsize = sizeof(libftdb_ftdbTypesType),
    .tp_dealloc = (destructor)libftdb_ftdb_types_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_types_repr,
    .tp_as_sequence = &libftdb_ftdbTypes_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbTypes_mapping_methods,
    .tp_doc = "libftdb ftdbTypes object",
    .tp_iter = libftdb_ftdb_types_getiter,
    .tp_methods = libftdb_ftdbTypes_methods,
    .tp_members = libftdb_ftdbTypes_members,
    .tp_getset = &libftdb_ftdbTypes_getset[0],
    .tp_new = libftdb_ftdb_types_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_types_object* types;
} libftdb_ftdb_types_iter_object;

void libftdb_ftdb_types_iter_dealloc(libftdb_ftdb_types_iter_object* self);
PyObject* libftdb_ftdb_types_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_types_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_types_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbTypesIter_sequence_methods = {
        .sq_length = libftdb_ftdb_types_iter_sq_length,
};

static PyTypeObject libftdb_ftdbTypesIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbTypesIter",
    .tp_basicsize = sizeof(libftdb_ftdbTypesIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_types_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbTypesIter_sequence_methods,
    .tp_doc = "libftdb ftdb types iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_types_iter_next,
    .tp_new = libftdb_ftdb_types_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb_type_entry* entry;
    unsigned long index;
    const libftdb_ftdb_types_object* types;
} libftdb_ftdb_type_entry_object;

void libftdb_ftdb_type_entry_dealloc(libftdb_ftdb_type_entry_object* self);
PyObject* libftdb_ftdb_type_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_type_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_type_entry_json(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_get_id(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_fid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_hash(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_classname(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_classid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_qualifiers(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_size(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_str(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_refcount(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_refs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_usedrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_decls(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_def(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_memberoffsets(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_attrrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_attrnum(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_name(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_is_dependent(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_globalrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_enumvalues(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_outerfn(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_outerfnid(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_is_implicit(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_is_union(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_refnames(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_location(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_is_variadic(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_funrefs(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_useddef(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_defhead(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_identifiers(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_methods(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_get_bitfields(PyObject* self, void* closure);
PyObject* libftdb_ftdb_type_entry_has_usedrefs(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_has_decls(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_has_attrs(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_has_outerfn(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_has_location(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_is_const(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_to_non_const(libftdb_ftdb_type_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_type_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_type_entry_sq_contains(PyObject* self, PyObject* key);
PyObject* libftdb_ftdb_type_entry_deepcopy(PyObject* self, PyObject* memo);
Py_hash_t libftdb_ftdb_type_entry_hash(PyObject *o);
PyObject* libftdb_ftdb_type_entry_richcompare(PyObject *self, PyObject *other, int op);

static PyMethodDef libftdb_ftdbTypeEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_type_entry_json,METH_VARARGS,
            "Returns the json representation of the ftdb type entry"},
    {"has_usedrefs",(PyCFunction)libftdb_ftdb_type_entry_has_usedrefs,METH_VARARGS,
            "Returns True if type entry has usedrefs field"},
    {"has_decls",(PyCFunction)libftdb_ftdb_type_entry_has_decls,METH_VARARGS,
            "Returns True if type entry has decls field"},
    {"has_attrs",(PyCFunction)libftdb_ftdb_type_entry_has_attrs,METH_VARARGS,
            "Returns True if type entry has attribute references"},
    {"has_outerfn",(PyCFunction)libftdb_ftdb_type_entry_has_outerfn,METH_VARARGS,
            "Returns True if type entry has outerfn field"},
    {"has_location",(PyCFunction)libftdb_ftdb_type_entry_has_location,METH_VARARGS,
            "Returns True if type entry has location field"},
    {"isConst",(PyCFunction)libftdb_ftdb_type_entry_is_const,METH_VARARGS,"Check whether the type is a const type"},
    {"toNonConst",(PyCFunction)libftdb_ftdb_type_entry_to_non_const,METH_VARARGS,"Returns the non-const counterpart of a type (if available)"},
    {"__deepcopy__",(PyCFunction)libftdb_ftdb_type_entry_deepcopy,METH_O,"Deep copy of an object"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMemberDef libftdb_ftdbTypeEntry_members[] = {
    {0}  /* Sentinel */
};

static PySequenceMethods libftdb_ftdbTypeEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_type_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbTypeEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_type_entry_mp_subscript,
};

static PyGetSetDef libftdb_ftdbTypeEntry_getset[] = {
    {"id",libftdb_ftdb_type_entry_get_id,0,"ftdb type entry id value",0},
    {"fid",libftdb_ftdb_type_entry_get_fid,0,"ftdb type entry fid value",0},
    {"hash",libftdb_ftdb_type_entry_get_hash,0,"ftdb type entry hash value",0},
    {"classname",libftdb_ftdb_type_entry_get_classname,0,"ftdb type entry class name value",0},
    {"classid",libftdb_ftdb_type_entry_get_classid,0,"ftdb type entry class id value",0},
    {"qualifiers",libftdb_ftdb_type_entry_get_qualifiers,0,"ftdb type entry qualifiers value",0},
    {"size",libftdb_ftdb_type_entry_get_size,0,"ftdb type entry size value",0},
    {"str",libftdb_ftdb_type_entry_get_str,0,"ftdb type entry str value",0},
    {"refcount",libftdb_ftdb_type_entry_get_refcount,0,"ftdb type entry refcount value",0},
    {"refs",libftdb_ftdb_type_entry_get_refs,0,"ftdb type entry refs values",0},
    {"usedrefs",libftdb_ftdb_type_entry_get_usedrefs,0,"ftdb type entry usedrefs values",0},
    {"decls",libftdb_ftdb_type_entry_get_decls,0,"ftdb type entry decls values",0},
    {"defstring",libftdb_ftdb_type_entry_get_def,0,"ftdb type entry def value",0},
    {"memberoffsets",libftdb_ftdb_type_entry_get_memberoffsets,0,"ftdb type entry memberoffsets values",0},
    {"attrrefs",libftdb_ftdb_type_entry_get_attrrefs,0,"ftdb type entry attrrefs values",0},
    {"attrnum",libftdb_ftdb_type_entry_get_attrnum,0,"ftdb type entry attrnum value",0},
    {"name",libftdb_ftdb_type_entry_get_name,0,"ftdb type entry name value",0},
    {"dependent",libftdb_ftdb_type_entry_get_is_dependent,0,"ftdb type entry is dependent value",0},
    {"globalrefs",libftdb_ftdb_type_entry_get_globalrefs,0,"ftdb type entry globalrefs value",0},
    {"enumvalues",libftdb_ftdb_type_entry_get_enumvalues,0,"ftdb type entry enum values",0},
    {"values",libftdb_ftdb_type_entry_get_enumvalues,0,"ftdb type entry enum values",0},
    {"outerfn",libftdb_ftdb_type_entry_get_outerfn,0,"ftdb type entry outerfn value",0},
    {"outerfnid",libftdb_ftdb_type_entry_get_outerfnid,0,"ftdb type entry outerfnid value",0},
    {"implicit",libftdb_ftdb_type_entry_get_is_implicit,0,"ftdb type entry is implicit value",0},
    {"union",libftdb_ftdb_type_entry_get_is_union,0,"ftdb type entry is union value",0},
    {"isunion",libftdb_ftdb_type_entry_get_is_union,0,"ftdb type entry is union value",0},
    {"refnames",libftdb_ftdb_type_entry_get_refnames,0,"ftdb type entry refnames values",0},
    {"location",libftdb_ftdb_type_entry_get_location,0,"ftdb type entry location value",0},
    {"variadic",libftdb_ftdb_type_entry_get_is_variadic,0,"ftdb type entry is variadic value",0},
    {"funrefs",libftdb_ftdb_type_entry_get_funrefs,0,"ftdb type entry funrefs values",0},
    {"useddef",libftdb_ftdb_type_entry_get_useddef,0,"ftdb type entry useddef values",0},
    {"defhead",libftdb_ftdb_type_entry_get_defhead,0,"ftdb type entry defhead value",0},
    {"identifiers",libftdb_ftdb_type_entry_get_identifiers,0,"ftdb type entry identifiers values",0},
    {"methods",libftdb_ftdb_type_entry_get_methods,0,"ftdb type entry methods values",0},
    {"bitfields",libftdb_ftdb_type_entry_get_bitfields,0,"ftdb type entry bitfields values",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbTypeEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbTypeEntry",
    .tp_basicsize = sizeof(libftdb_ftdbTypeEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_type_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_type_entry_repr,
    .tp_as_sequence = &libftdb_ftdbTypeEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbTypeEntry_mapping_methods,
    .tp_hash = (hashfunc)libftdb_ftdb_type_entry_hash,
    .tp_doc = "libftdb ftdb type entry object",
    .tp_richcompare = libftdb_ftdb_type_entry_richcompare,
    .tp_methods = libftdb_ftdbTypeEntry_methods,
    .tp_members = libftdb_ftdbTypeEntry_members,
    .tp_getset = &libftdb_ftdbTypeEntry_getset[0],
    .tp_new = libftdb_ftdb_type_entry_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb* ftdb;
    const libftdb_ftdb_object* py_ftdb;
} libftdb_ftdb_fops_object;

void libftdb_ftdb_fops_dealloc(libftdb_ftdb_fops_object* self);
PyObject* libftdb_ftdb_fops_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_fops_repr(PyObject* self);
Py_ssize_t libftdb_ftdb_fops_sq_length(PyObject* self);
PyObject* libftdb_ftdb_fops_getiter(PyObject* self);
PyObject* libftdb_ftdb_fops_mp_subscript(PyObject* self, PyObject* slice);

static PyMethodDef libftdb_ftdbFops_methods[] = {
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFops_sequence_methods = {
    .sq_length = libftdb_ftdb_fops_sq_length,
};

static PyMemberDef libftdb_ftdbFops_members[] = {
    {0}  /* Sentinel */
};

static PyMappingMethods libftdb_ftdbFops_mapping_methods = {
    .mp_subscript = libftdb_ftdb_fops_mp_subscript,
};

static PyTypeObject libftdb_ftdbFopsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFops",
    .tp_basicsize = sizeof(libftdb_ftdbFopsType),
    .tp_dealloc = (destructor)libftdb_ftdb_fops_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_fops_repr,
    .tp_as_sequence = &libftdb_ftdbFops_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFops_mapping_methods,
    .tp_doc = "libftdb ftdbFops object",
    .tp_iter = libftdb_ftdb_fops_getiter,
    .tp_methods = libftdb_ftdbFops_methods,
    .tp_members = libftdb_ftdbFops_members,
    .tp_new = libftdb_ftdb_fops_new,
};

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_fops_object* fops;
} libftdb_ftdb_fops_iter_object;

void libftdb_ftdb_fops_iter_dealloc(libftdb_ftdb_fops_iter_object* self);
PyObject* libftdb_ftdb_fops_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_fops_iter_sq_length(PyObject* self);
PyObject* libftdb_ftdb_fops_iter_next(PyObject *self);

static PySequenceMethods libftdb_ftdbFopsIter_sequence_methods = {
        .sq_length = libftdb_ftdb_fops_iter_sq_length,
};

static PyTypeObject libftdb_ftdbFopsIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFopsIter",
    .tp_basicsize = sizeof(libftdb_ftdbFopsIterType),
    .tp_dealloc = (destructor)libftdb_ftdb_fops_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbFopsIter_sequence_methods,
    .tp_doc = "libftdb ftdb fops iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_fops_iter_next,
    .tp_new = libftdb_ftdb_fops_iter_new,
};

typedef struct {
    PyObject_HEAD
    const struct ftdb_fops_entry* entry;
    unsigned long index;
    const libftdb_ftdb_fops_object* fops;
} libftdb_ftdb_fops_entry_object;

void libftdb_ftdb_fops_entry_dealloc(libftdb_ftdb_fops_entry_object* self);
PyObject* libftdb_ftdb_fops_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libftdb_ftdb_fops_entry_repr(PyObject* self);
PyObject* libftdb_ftdb_fops_entry_json(libftdb_ftdb_fops_entry_object *self, PyObject *args);
PyObject* libftdb_ftdb_fops_entry_get_kind(PyObject* self, void* closure);
PyObject* libftdb_ftdb_fops_entry_get_type(PyObject* self, void* closure);
PyObject* libftdb_ftdb_fops_entry_get_var(PyObject* self, void* closure);
PyObject* libftdb_ftdb_fops_entry_get_func(PyObject* self, void* closure);
PyObject* libftdb_ftdb_fops_entry_get_members(PyObject* self, void* closure);
PyObject* libftdb_ftdb_fops_entry_get_location(PyObject* self, void* closure);
PyObject* libftdb_ftdb_fops_entry_mp_subscript(PyObject* self, PyObject* slice);
int libftdb_ftdb_fops_entry_sq_contains(PyObject* self, PyObject* key);
PyObject* libftdb_ftdb_fops_entry_deepcopy(PyObject* self, PyObject* memo);

static PyMethodDef libftdb_ftdbFopsEntry_methods[] = {
    {"json",(PyCFunction)libftdb_ftdb_fops_entry_json,METH_VARARGS,"Returns the json representation of the ftdb fops entry"},
    {"__deepcopy__",(PyCFunction)libftdb_ftdb_fops_entry_deepcopy,METH_O,"Deep copy of an object"},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMemberDef libftdb_ftdbFopsEntry_members[] = {
    {0}  /* Sentinel */
};

static PySequenceMethods libftdb_ftdbFopsEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_fops_entry_sq_contains
};

static PyMappingMethods libftdb_ftdbFopsEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_fops_entry_mp_subscript,
};

static PyGetSetDef libftdb_ftdbFopsEntry_getset[] = {
    {"kind",libftdb_ftdb_fops_entry_get_kind,0,"ftdb fops entry kind value",0},
    {"type",libftdb_ftdb_fops_entry_get_type,0,"ftdb fops entry type value",0},
    {"var",libftdb_ftdb_fops_entry_get_var,0,"ftdb fops entry var value",0},
    {"func",libftdb_ftdb_fops_entry_get_func,0,"ftdb fops entry func value",0},
    {"members",libftdb_ftdb_fops_entry_get_members,0,"ftdb fops entry members values",0},
    {"loc",libftdb_ftdb_fops_entry_get_location,0,"ftdb fops entry location value",0},
    {0,0,0,0,0},
};

static PyTypeObject libftdb_ftdbFopsEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFopsEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFopsEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_fops_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_fops_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFopsEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFopsEntry_mapping_methods,
    .tp_doc = "libftdb ftdb fops entry object",
    .tp_methods = libftdb_ftdbFopsEntry_methods,
    .tp_members = libftdb_ftdbFopsEntry_members,
    .tp_getset = &libftdb_ftdbFopsEntry_getset[0],
    .tp_new = libftdb_ftdb_fops_entry_new,
};
#endif /* __PYFTDB_H__ */
