#ifndef _PYFTDB_H
#define _PYFTDB_H

#define PY_SSIZE_T_CLEAN

#include <Python.h>
#include <structmember.h>
#include <signal.h>
#include <fnmatch.h>
#include <time.h>
#include <string.h>
#include <stdbool.h>

#include "rbtree.h"
#include "utils.h"
#include <ftdb.h>
#include "ftdb_entry.h"
#include <pthread.h>
#include "uflat.h"

#include <unflatten.hpp>

int ftdb_maps(struct ftdb *ftdb, int show_stats);

#define FTDB_MODULE_INIT_CHECK                                                                                                        \
    do {                                                                                                                              \
        if (!__self->init_done) {                                                                                                     \
            PyErr_SetString(libftdb_ftdbError, "Core 'ftdb' module not initialized. Please use 'ftdb = libftdb.ftdb()' or similar."); \
            return 0;                                                                                                                 \
        }                                                                                                                             \
    } while (0)

extern PyObject *libftdb_ftdbError;

extern PyMethodDef libftdb_ftdbFops_methods[];
extern PySequenceMethods libftdb_ftdbFops_sequence_methods;
extern PyMemberDef libftdb_ftdbFops_members[];
extern PyMappingMethods libftdb_ftdbFops_mapping_methods;
extern PyTypeObject libftdb_ftdbFopsType;
extern PyTypeObject libftdb_ftdbFopsIterType;
extern PyMethodDef libftdb_ftdbFopsEntry_methods[];
extern PyMemberDef libftdb_ftdbFopsEntry_members[];
extern PySequenceMethods libftdb_ftdbFopsEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFopsEntry_mapping_methods;
extern PyGetSetDef libftdb_ftdbFopsEntry_getset[];
extern PyTypeObject libftdb_ftdbFopsEntryType;

extern PyMethodDef libftdb_ftdbFuncdecls_methods[];
extern PyMappingMethods libftdb_ftdbFuncdecls_mapping_methods;
extern PySequenceMethods libftdb_ftdbFuncdecls_sequence_methods;
extern PyMemberDef libftdb_ftdbFuncdecls_members[];
extern PyGetSetDef libftdb_ftdbFuncdecls_getset[];
extern PyTypeObject libftdb_ftdbFuncdeclsType;
extern PyMethodDef libftdb_ftdbFuncdeclEntry_methods[];
extern PyMemberDef libftdb_ftdbFuncdeclEntry_members[];
extern PyMappingMethods libftdb_ftdbFuncdeclEntry_mapping_methods;
extern PySequenceMethods libftdb_ftdbFuncdeclEntry_sequence_methods;
extern PyGetSetDef libftdb_ftdbFuncdeclEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncdeclsEntryType;
extern PyTypeObject libftdb_ftdbFuncdeclsIterType;

extern PyMethodDef libftdb_ftdbFuncDerefInfoEntry_methods[];
extern PySequenceMethods libftdb_ftdbFuncDerefInfoEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncDerefInfoEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncDerefInfoEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncDerefInfoEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncDerefInfoEntryType;

extern PyMethodDef libftdb_ftdbFuncCallInfoEntry_methods[];
extern PySequenceMethods libftdb_ftdbFuncCallInfoEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncCallInfoEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncCallInfoEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncCallInfoEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncCallInfoEntryType;

extern PyMethodDef libftdb_ftdbFuncIfInfoEntry_methods[];
extern PySequenceMethods libftdb_ftdbFuncIfInfoEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncIfnfoEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncIfInfoEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncIfInfoEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncIfInfoEntryType;

extern PyMethodDef libftdb_ftdbFuncLocalInfoEntry_methods[];
extern PySequenceMethods libftdb_ftdbFuncLocalInfoEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncLocalInfoEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncLocalInfoEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncLocalInfoEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncLocalInfoEntryType;

extern PyMethodDef libftdb_ftdbFuncOffsetrefInfoEntry_methods[];
extern PySequenceMethods libftdb_ftdbFuncOffsetrefInfoEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncOffsetrefInfoEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncOffsetrefInfoEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncOffsetrefInfoEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncOffsetrefInfoEntryType;

extern PyMethodDef libftdb_ftdbFuncs_methods[];
extern PySequenceMethods libftdb_ftdbFuncs_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncs_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncs_members[];
extern PyGetSetDef libftdb_ftdbFuncs_getset[];
extern PyTypeObject libftdb_ftdbFuncsType;
extern PyMethodDef libftdb_ftdbFuncEntry_methods[];
extern PyMemberDef libftdb_ftdbFuncEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncEntry_getset[];
extern PyMappingMethods libftdb_ftdbFuncEntry_mapping_methods;
extern PySequenceMethods libftdb_ftdbFuncEntry_sequence_methods;
extern PyTypeObject libftdb_ftdbFuncsEntryType;
extern PyTypeObject libftdb_ftdbFuncsIterType;

extern PyMethodDef libftdb_ftdbFuncSwitchInfoEntry_methods[];
extern PySequenceMethods libftdb_ftdbFuncSwitchInfoEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbFuncSwitchInfoEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbFuncSwitchInfoEntry_members[];
extern PyGetSetDef libftdb_ftdbFuncSwitchInfoEntry_getset[];
extern PyTypeObject libftdb_ftdbFuncSwitchInfoEntryType;

extern PyMethodDef libftdb_ftdbGlobals_methods[];
extern PySequenceMethods libftdb_ftdbGlobals_sequence_methods;
extern PyMappingMethods libftdb_ftdbGlobals_mapping_methods;
extern PyMemberDef libftdb_ftdbGlobals_members[];
extern PyGetSetDef libftdb_ftdbGlobals_getset[];
extern PyTypeObject libftdb_ftdbGlobalsType;
extern PyMethodDef libftdb_ftdbGlobalEntry_methods[];
extern PyMemberDef libftdb_ftdbGlobalEntry_members[];
extern PyMappingMethods libftdb_ftdbGlobalEntry_mapping_methods;
extern PySequenceMethods libftdb_ftdbGlobalEntry_sequence_methods;
extern PyGetSetDef libftdb_ftdbGlobalEntry_getset[];
extern PyTypeObject libftdb_ftdbGlobalEntryType;
extern PyTypeObject libftdb_ftdbGlobalsIterType;

extern PyMethodDef libftdb_ftdbModules_methods[];
extern PySequenceMethods libftdb_ftdbModules_sequence_methods;
extern PyMemberDef libftdb_ftdbModules_members[];
extern PyGetSetDef libftdb_ftdbModules_getset[];
extern PyMappingMethods libftdb_ftdbModules_mapping_methods;
extern PyTypeObject libftdb_ftdbModulesType;
extern PyTypeObject libftdb_ftdbModulesIterType;

extern PyMethodDef libftdb_ftdbSources_methods[];
extern PySequenceMethods libftdb_ftdbSources_sequence_methods;
extern PyMemberDef libftdb_ftdbSources_members[];
extern PyGetSetDef libftdb_ftdbSources_getset[];
extern PyMappingMethods libftdb_ftdbSources_mapping_methods;
extern PyTypeObject libftdb_ftdbSourcesType;
extern PyTypeObject libftdb_ftdbSourcesIterType;

extern PyMethodDef libftdb_ftdbTypes_methods[];
extern PySequenceMethods libftdb_ftdbTypes_sequence_methods;
extern PyMappingMethods libftdb_ftdbTypes_mapping_methods;
extern PyMemberDef libftdb_ftdbTypes_members[];
extern PyGetSetDef libftdb_ftdbTypes_getset[];
extern PyTypeObject libftdb_ftdbTypesType;
extern PyMethodDef libftdb_ftdbTypeEntry_methods[];
extern PyMemberDef libftdb_ftdbTypeEntry_members[];
extern PySequenceMethods libftdb_ftdbTypeEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbTypeEntry_mapping_methods;
extern PyGetSetDef libftdb_ftdbTypeEntry_getset[];
extern PyTypeObject libftdb_ftdbTypeEntryType;
extern PyTypeObject libftdb_ftdbTypesIterType;

extern PyMethodDef libftdb_ftdbUnresolvedfuncs_methods[];
extern PySequenceMethods libftdb_ftdbUnresolvedfuncs_sequence_methods;
extern PyMemberDef libftdb_ftdbUnresolvedfuncs_members[];
extern PyGetSetDef libftdb_ftdbUnresolvedfuncs_getset[];
extern PyTypeObject libftdb_ftdbUnresolvedfuncsType;
extern PyMethodDef libftdb_ftdbUnresolvedfuncEntry_methods[];
extern PySequenceMethods libftdb_ftdbUnresolvedfuncEntry_sequence_methods;
extern PyMappingMethods libftdb_ftdbUnresolvedfuncEntry_mapping_methods;
extern PyMemberDef libftdb_ftdbUnresolvedfuncEntry_members[];
extern PyGetSetDef libftdb_ftdbUnresolvedfuncEntry_getset[];
extern PyTypeObject libftdb_ftdbUnresolvedfuncEntryType;
extern PyTypeObject libftdb_ftdbUnresolvedfuncsIterType;

extern PySequenceMethods libftdb_ftdbCollectionIter_sequence_methods;

struct ftdb_ref {
    const struct ftdb *ftdb;
    unsigned long refcount;
};

typedef struct {
    PyObject_HEAD
    int verbose;
    int debug;
    int init_done;
    const struct ftdb *ftdb;
    struct stringRefMap_node *ftdb_image_map_node;

    CUnflatten unflatten;
} libftdb_ftdb_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb *ftdb;
    const libftdb_ftdb_object *py_ftdb;
    unsigned long count;
    const char *name;
} libftdb_ftdb_collection_object;

typedef struct {
    PyObject_HEAD
    unsigned long index;
    const libftdb_ftdb_collection_object *collection;

} libftdb_ftdb_collection_iter_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb_fops_entry *entry;
    unsigned long index;
    const libftdb_ftdb_collection_object *fops;
} libftdb_ftdb_fops_entry_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb_funcdecl_entry *entry;
    unsigned long index;
    const libftdb_ftdb_collection_object *funcdecls;
} libftdb_ftdb_funcdecl_entry_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb_func_entry *entry;
    unsigned long index;
    const libftdb_ftdb_collection_object *funcs;
} libftdb_ftdb_func_entry_object;

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    unsigned long is_refcall;
    struct call_info *entry;
    const libftdb_ftdb_func_entry_object *func_entry;
} libftdb_ftdb_func_callinfo_entry_object;

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct deref_info *entry;
    const libftdb_ftdb_func_entry_object *func_entry;
} libftdb_ftdb_func_derefinfo_entry_object;

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct if_info *entry;
    const libftdb_ftdb_func_entry_object *func_entry;
} libftdb_ftdb_func_ifinfo_entry_object;

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct local_info *entry;
    const libftdb_ftdb_func_entry_object *func_entry;
} libftdb_ftdb_func_localinfo_entry_object;

typedef struct {
    PyObject_HEAD
    unsigned long entry_index;
    struct offsetref_info *entry;
    const libftdb_ftdb_func_derefinfo_entry_object *deref_entry;
} libftdb_ftdb_func_offsetrefinfo_entry_object;

typedef struct {
    PyObject_HEAD
    unsigned long func_index;
    unsigned long entry_index;
    struct switch_info *entry;
    const libftdb_ftdb_func_entry_object *func_entry;
} libftdb_ftdb_func_switchinfo_entry_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb_global_entry *entry;
    unsigned long index;
    const libftdb_ftdb_collection_object *globals;
} libftdb_ftdb_global_entry_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb_type_entry *entry;
    unsigned long index;
    const libftdb_ftdb_collection_object *types;
} libftdb_ftdb_type_entry_object;

typedef struct {
    PyObject_HEAD
    const struct ftdb_unresolvedfunc_entry *entry;
    unsigned long index;
    const libftdb_ftdb_collection_object *unresolvedfuncs;
} libftdb_ftdb_unresolvedfunc_entry_object;

static PyObject *libftdb_generic_richcompare(PyObject *self, PyObject *other, int op) {
    if (op != Py_EQ && op != Py_NE)
        Py_RETURN_NOTIMPLEMENTED;
    if (self == other)
        Py_RETURN_TRUE;

    if (Py_TYPE(self) != Py_TYPE(other))
        Py_RETURN_NOTIMPLEMENTED;

    PyObject *self_json = PyObject_CallMethod(self, "json", "");
    PyObject *other_json = PyObject_CallMethod(other, "json", "");
    bool compare = PyObject_RichCompareBool(self_json, other_json, op);
    Py_DECREF(self_json);
    Py_DECREF(other_json);

    if (compare)
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static const char *PyString_get_c_str(PyObject *s) {
    if (PyUnicode_Check(s)) {
        PyObject *us = PyUnicode_AsUTF8String(s);
        if (us) {
            return PyBytes_AsString(us);
        } else {
            return 0;
        }
    } else {
        return (const char *)0xdeadbeef;
    }
}

void libftdb_ftdb_collection_dealloc(libftdb_ftdb_collection_object *self);
PyObject *libftdb_ftdb_collection_repr(PyObject *self);
PyObject *libftdb_ftdb_collection_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libftdb_ftdb_collection_sq_length(PyObject *self);

void libftdb_ftdb_collection_iter_dealloc(libftdb_ftdb_collection_iter_object *self);
PyObject *libftdb_ftdb_collection_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);

#endif /* _PYFTDB_H */
