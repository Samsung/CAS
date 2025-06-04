#include "pyftdb.h"

static PyObject *libftdb_ftdb_types_getiter(PyObject *self) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0);
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbTypesIterType, args);

    Py_DecRef(args);
    return iter;
}

static PyObject *libftdb_ftdb_types_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    if (PyLong_Check(slice)) {
        unsigned long id = PyLong_AsUnsignedLong(slice);
        struct ulong_entryMap_node *node = ulong_entryMap_search(&__self->ftdb->refmap, id);
        if (!node) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid type id not present in the array: %lu\n", id);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        struct ftdb_type_entry *type_entry = (struct ftdb_type_entry *)node->entry;
        PyObject *args = PyTuple_New(2);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, type_entry->__index);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbTypeEntryType, args);
        Py_DecRef(args);
        return entry;
    } else if (PyUnicode_Check(slice)) {
        const char *hash = PyString_get_c_str(slice);
        struct stringRef_entryMap_node *node = stringRef_entryMap_search(&__self->ftdb->hrefmap, hash);
        if (!node) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid type hash not present in the array: %s\n", hash);
            PYASSTR_DECREF(hash);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        PYASSTR_DECREF(hash);
        struct ftdb_type_entry *type_entry = (struct ftdb_type_entry *)node->entry;
        PyObject *args = PyTuple_New(2);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, type_entry->__index);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbTypeEntryType, args);
        Py_DecRef(args);
        return entry;
    } else {
        PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
        return 0;
    }
}

static PyObject *libftdb_ftdb_types_contains_hash_internal(libftdb_ftdb_collection_object *self, const char *hash) {
    struct stringRef_entryMap_node *node = stringRef_entryMap_search(&self->ftdb->hrefmap, hash);
    if (node) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *libftdb_ftdb_types_contains_hash(libftdb_ftdb_collection_object *self, PyObject *args) {
    const char *hash;
    if (!PyArg_ParseTuple(args, "s", &hash))
        return NULL;
    return libftdb_ftdb_types_contains_hash_internal(self, hash);
}

static PyObject *libftdb_ftdb_types_contains_id_internal(libftdb_ftdb_collection_object *self, unsigned long id) {
    struct ulong_entryMap_node *node = ulong_entryMap_search(&self->ftdb->refmap, id);
    if (node) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *libftdb_ftdb_types_contains_id(libftdb_ftdb_collection_object *self, PyObject *args) {
    unsigned long id;
    if (!PyArg_ParseTuple(args, "k", &id))
        return NULL;
    return libftdb_ftdb_types_contains_id_internal(self, id);
}

static PyObject *libftdb_ftdb_types_entry_by_hash(libftdb_ftdb_collection_object *self, PyObject *args) {
    const char *hash;
    if (!PyArg_ParseTuple(args, "s", &hash))
        return NULL;

    static char errmsg[ERRMSG_BUFFER_SIZE];
    struct stringRef_entryMap_node *node = stringRef_entryMap_search(&self->ftdb->hrefmap, hash);
    if (!node) {
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function hash not present in the array: %s\n", hash);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        return 0;
    }
    struct ftdb_type_entry *types_entry = (struct ftdb_type_entry *)node->entry;
    PyObject *_args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(_args, 0, (uintptr_t)self);
    PYTUPLE_SET_ULONG(_args, 1, types_entry->__index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbTypeEntryType, _args);
    Py_DecRef(_args);
    return entry;
}

static PyObject *libftdb_ftdb_types_entry_by_id(libftdb_ftdb_collection_object *self, PyObject *args) {
    unsigned long id;
    if (!PyArg_ParseTuple(args, "k", &id))
        return NULL;

    static char errmsg[ERRMSG_BUFFER_SIZE];

    struct ulong_entryMap_node *node = ulong_entryMap_search(&self->ftdb->refmap, id);
    if (!node) {
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function id not present in the array: %lu\n", id);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        return 0;
    }
    struct ftdb_type_entry *types_entry = (struct ftdb_type_entry *)node->entry;
    PyObject *_args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(_args, 0, (uintptr_t)self);
    PYTUPLE_SET_ULONG(_args, 1, types_entry->__index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbTypeEntryType, _args);
    Py_DecRef(_args);
    return entry;
}

static int libftdb_ftdb_types_sq_contains(PyObject *self, PyObject *key) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    if (PyUnicode_Check(key)) {
        /* Check hash */
        const char *hash;
        if (!PyArg_Parse(key, "s", &hash))
            return -1;
        PyObject *v = libftdb_ftdb_types_contains_hash_internal(__self, hash);
        if (v) {
            return PyObject_IsTrue(v);
        }
    } else if (PyLong_Check(key)) {
        /* Check id */
        unsigned long id = PyLong_AsUnsignedLong(key);
        PyObject *v = libftdb_ftdb_types_contains_id_internal(__self, id);
        if (v) {
            return PyObject_IsTrue(v);
        }
    }

    PyErr_SetString(libftdb_ftdbError, "Invalid key type in contains check (not a str or integer)");
    return 0;
}

PyMethodDef libftdb_ftdbTypes_methods[] = {
    {"entry_by_id", (PyCFunction)libftdb_ftdb_types_entry_by_id, METH_VARARGS, "Returns the ftdb types entry with a given id"},
    {"contains_id", (PyCFunction)libftdb_ftdb_types_contains_id, METH_VARARGS, "Check whether there is a types entry with a given id"},
    {"entry_by_hash", (PyCFunction)libftdb_ftdb_types_entry_by_hash, METH_VARARGS, "Returns the ftdb types entry with a given hash value"},
    {"contains_hash", (PyCFunction)libftdb_ftdb_types_contains_hash, METH_VARARGS, "Check whether there is a types entry with a given hash"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbTypes_sequence_methods = {
    .sq_length = libftdb_ftdb_collection_sq_length,
    .sq_contains = libftdb_ftdb_types_sq_contains
};

PyMappingMethods libftdb_ftdbTypes_mapping_methods = {
    .mp_subscript = libftdb_ftdb_types_mp_subscript,
};

PyTypeObject libftdb_ftdbTypesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbTypes",
    .tp_basicsize = sizeof(libftdb_ftdb_collection_object),
    .tp_dealloc = (destructor)libftdb_ftdb_collection_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_collection_repr,
    .tp_as_sequence = &libftdb_ftdbTypes_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbTypes_mapping_methods,
    .tp_doc = "libftdb ftdbTypes object",
    .tp_iter = libftdb_ftdb_types_getiter,
    .tp_methods = libftdb_ftdbTypes_methods,
    .tp_new = libftdb_ftdb_collection_new,
};
