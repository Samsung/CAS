#include "pyftdb.h"

static PyObject *libftdb_ftdb_fops_getiter(PyObject *self) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0);
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbFopsIterType, args);

    Py_DecRef(args);
    return iter;
}

static PyObject *libftdb_ftdb_fops_mp_subscript(PyObject *self, PyObject *slice) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    if ((!PyLong_Check(slice))) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type fops index (not an (integer))");
        Py_RETURN_NONE;
    }

    unsigned long index = PyLong_AsLong(slice);

    if (index >= __self->ftdb->fops_count) {
        PyErr_SetString(libftdb_ftdbError, "fops index out of range");
        Py_RETURN_NONE;
    }

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFopsEntryType, args);
    Py_DecRef(args);
    return entry;
}

PySequenceMethods libftdb_ftdbFops_sequence_methods = {
    .sq_length = libftdb_ftdb_collection_sq_length,
};

PyMappingMethods libftdb_ftdbFops_mapping_methods = {
    .mp_subscript = libftdb_ftdb_fops_mp_subscript,
};

PyTypeObject libftdb_ftdbFopsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFops",
    .tp_basicsize = sizeof(libftdb_ftdb_collection_object),
    .tp_dealloc = (destructor)libftdb_ftdb_collection_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_collection_repr,
    .tp_as_sequence = &libftdb_ftdbFops_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFops_mapping_methods,
    .tp_doc = "libftdb ftdbFops object",
    .tp_iter = libftdb_ftdb_fops_getiter,
    .tp_new = libftdb_ftdb_collection_new,
};
