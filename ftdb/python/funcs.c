#include "pyftdb.h"

static PyObject *libftdb_ftdb_funcs_getiter(PyObject *self) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0); /* index */
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsIterType, args);

    Py_DecRef(args);
    return iter;
}

static PyObject *libftdb_ftdb_funcs_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    if (PyLong_Check(slice)) {
        unsigned long id = PyLong_AsUnsignedLong(slice);
        struct ulong_entryMap_node *node = ulong_entryMap_search(&__self->ftdb->frefmap, id);
        if (!node) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function id not present in the array: %lu\n", id);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        struct ftdb_func_entry *func_entry = (struct ftdb_func_entry *)node->entry;
        PyObject *args = PyTuple_New(2);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, func_entry->__index);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsEntryType, args);
        Py_DecRef(args);
        return entry;
    } else if (PyUnicode_Check(slice)) {
        const char *hash = PyString_get_c_str(slice);
        struct stringRef_entryMap_node *node = stringRef_entryMap_search(&__self->ftdb->fhrefmap, hash);
        if (!node) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function hash not present in the array: %s\n", hash);
            PYASSTR_DECREF(hash);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        PYASSTR_DECREF(hash);
        struct ftdb_func_entry *func_entry = (struct ftdb_func_entry *)node->entry;
        PyObject *args = PyTuple_New(2);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, func_entry->__index);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsEntryType, args);
        Py_DecRef(args);
        return entry;
    } else {
        PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
        return 0;
    }
}

static PyObject *libftdb_ftdb_funcs_entry_by_id(libftdb_ftdb_collection_object *self, PyObject *args) {
    unsigned long id;
    if (!PyArg_ParseTuple(args, "k", &id))
        return NULL;

    static char errmsg[ERRMSG_BUFFER_SIZE];

    struct ulong_entryMap_node *node = ulong_entryMap_search(&self->ftdb->frefmap, id);
    if (!node) {
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function id not present in the array: %lu\n", id);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        return 0;
    }
    struct ftdb_func_entry *func_entry = (struct ftdb_func_entry *)node->entry;
    PyObject *_args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(_args, 0, (uintptr_t)self);
    PYTUPLE_SET_ULONG(_args, 1, func_entry->__index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsEntryType, _args);
    Py_DecRef(_args);
    return entry;
}

static PyObject *libftdb_ftdb_funcs_contains_id_internal(libftdb_ftdb_collection_object *self, unsigned long id) {
    struct ulong_entryMap_node *node = ulong_entryMap_search(&self->ftdb->frefmap, id);
    if (node) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *libftdb_ftdb_funcs_contains_id(libftdb_ftdb_collection_object *self, PyObject *args) {
    unsigned long id;
    if (!PyArg_ParseTuple(args, "k", &id))
        return NULL;

    return libftdb_ftdb_funcs_contains_id_internal(self, id);
}

static PyObject *libftdb_ftdb_funcs_entry_by_hash(libftdb_ftdb_collection_object *self, PyObject *args) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    const char *hash;
    if (!PyArg_ParseTuple(args, "s", &hash))
        return NULL;

    struct stringRef_entryMap_node *node = stringRef_entryMap_search(&self->ftdb->fhrefmap, hash);
    if (!node) {
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function hash not present in the array: %s\n", hash);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        return 0;
    }
    struct ftdb_func_entry *func_entry = (struct ftdb_func_entry *)node->entry;
    PyObject *_args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(_args, 0, (uintptr_t)self);
    PYTUPLE_SET_ULONG(_args, 1, func_entry->__index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsEntryType, _args);
    Py_DecRef(_args);
    return entry;
}

static PyObject *libftdb_ftdb_funcs_contains_hash_internal(libftdb_ftdb_collection_object *self, const char *hash) {
    struct stringRef_entryMap_node *node = stringRef_entryMap_search(&self->ftdb->fhrefmap, hash);
    if (node) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *libftdb_ftdb_funcs_contains_hash(libftdb_ftdb_collection_object *self, PyObject *args) {
    const char *hash;
    if (!PyArg_ParseTuple(args, "s", &hash))
        return NULL;

    return libftdb_ftdb_funcs_contains_hash_internal(self, hash);
}

static PyObject *libftdb_ftdb_funcs_entry_by_name(libftdb_ftdb_collection_object *self, PyObject *args) {
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    static char errmsg[ERRMSG_BUFFER_SIZE];

    struct stringRef_entryListMap_node *node = stringRef_entryListMap_search(&self->ftdb->fnrefmap, name);
    if (!node) {
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "invalid function name not present in the array: %s\n", name);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        return 0;
    }
    struct ftdb_func_entry **func_entry_list = (struct ftdb_func_entry **)node->entry_list;
    PyObject *entry_list = PyList_New(0);
    for (unsigned long i = 0; i < node->entry_count; ++i) {
        struct ftdb_func_entry *func_entry = (struct ftdb_func_entry *)(func_entry_list[i]);
        PyObject *args = PyTuple_New(2);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)self);
        PYTUPLE_SET_ULONG(args, 1, func_entry->__index);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsEntryType, args);
        Py_DecRef(args);
        PyList_Append(entry_list, entry);
        Py_DecRef(entry);
    }
    return entry_list;
}

static PyObject *libftdb_ftdb_funcs_contains_name_internal(libftdb_ftdb_collection_object *self, const char *name) {
    struct stringRef_entryListMap_node *node = stringRef_entryListMap_search(&self->ftdb->fnrefmap, name);
    if (node) {
        Py_RETURN_TRUE;
    } else {
        Py_RETURN_FALSE;
    }
}

static PyObject *libftdb_ftdb_funcs_contains_name(libftdb_ftdb_collection_object *self, PyObject *args) {
    const char *name;
    if (!PyArg_ParseTuple(args, "s", &name))
        return NULL;

    return libftdb_ftdb_funcs_contains_name_internal(self, name);
}

static int libftdb_ftdb_funcs_sq_contains(PyObject *self, PyObject *key) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    if (PyUnicode_Check(key)) {
        /* Check name */
        const char *name;
        if (!PyArg_Parse(key, "s", &name))
            return -1;

        PyObject *v = libftdb_ftdb_funcs_contains_name_internal(__self, name);
        if (v) {
            return PyObject_IsTrue(v);
        }
    } else if (PyLong_Check(key)) {
        /* Check id */
        unsigned long id = PyLong_AsUnsignedLong(key);
        PyObject *v = libftdb_ftdb_funcs_contains_id_internal(__self, id);
        if (v) {
            return PyObject_IsTrue(v);
        }
    } else if (PyTuple_Check(key)) {
        /* Check hash */
        const char *hash;
        if (!PyArg_ParseTuple(key, "s", &hash))
            return -1;

        PyObject *v = libftdb_ftdb_funcs_contains_hash_internal(__self, hash);
        if (v) {
            return PyObject_IsTrue(v);
        }
    }

    PyErr_SetString(libftdb_ftdbError, "Invalid key type in contains check (not a str, tuple or integer)");
    return 0;
}

static PyObject *libftdb_ftdb_funcs_get_refcount(PyObject *self, void *closure) {
    return PyLong_FromLong(self->ob_refcnt);
}

PyMethodDef libftdb_ftdbFuncs_methods[] = {
    {"entry_by_id", (PyCFunction)libftdb_ftdb_funcs_entry_by_id, METH_VARARGS, "Returns the ftdb func entry with a given id"},
    {"contains_id", (PyCFunction)libftdb_ftdb_funcs_contains_id, METH_VARARGS, "Check whether there is a func entry with a given id"},
    {"entry_by_hash", (PyCFunction)libftdb_ftdb_funcs_entry_by_hash, METH_VARARGS, "Returns the ftdb func entry with a given hash value"},
    {"contains_hash", (PyCFunction)libftdb_ftdb_funcs_contains_hash, METH_VARARGS, "Check whether there is a func entry with a given hash"},
    {"entry_by_name", (PyCFunction)libftdb_ftdb_funcs_entry_by_name, METH_VARARGS, "Returns the list of ftdb func entries with a given name"},
    {"contains_name", (PyCFunction)libftdb_ftdb_funcs_contains_name, METH_VARARGS, "Check whether there is a func entry with a given name"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncs_sequence_methods = {
    .sq_length = libftdb_ftdb_collection_sq_length,
    .sq_contains = libftdb_ftdb_funcs_sq_contains
};

PyMappingMethods libftdb_ftdbFuncs_mapping_methods = {
    .mp_subscript = libftdb_ftdb_funcs_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncs_getset[] = {
    {"__refcount__", libftdb_ftdb_funcs_get_refcount, 0, "ftdb funcs object reference count", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncs",
    .tp_basicsize = sizeof(libftdb_ftdb_collection_object),
    .tp_dealloc = (destructor)libftdb_ftdb_collection_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_collection_repr,
    .tp_as_sequence = &libftdb_ftdbFuncs_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncs_mapping_methods,
    .tp_doc = "libftdb ftdbFuncs object",
    .tp_iter = libftdb_ftdb_funcs_getiter,
    .tp_methods = libftdb_ftdbFuncs_methods,
    .tp_getset = libftdb_ftdbFuncs_getset,
    .tp_new = libftdb_ftdb_collection_new,
};
