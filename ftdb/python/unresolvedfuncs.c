#include "pyftdb.h"

static PyObject *libftdb_ftdb_unresolvedfuncs_getiter(PyObject *self) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0);
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbUnresolvedfuncsIterType, args);

    Py_DecRef(args);
    return iter;
}

static PyObject *libftdb_ftdb_unresolvedfuncs_entry_deepcopy(PyObject *self, PyObject *memo) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyMethodDef libftdb_ftdbUnresolvedfuncs_methods[] = {
    {"__deepcopy__", (PyCFunction)libftdb_ftdb_unresolvedfuncs_entry_deepcopy, METH_O, "Deep copy of an object"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbUnresolvedfuncs_sequence_methods = {
    .sq_length = libftdb_ftdb_collection_sq_length,
};

PyTypeObject libftdb_ftdbUnresolvedfuncsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbUnresolvedfuncs",
    .tp_basicsize = sizeof(libftdb_ftdb_collection_object),
    .tp_dealloc = (destructor)libftdb_ftdb_collection_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_collection_repr,
    .tp_as_sequence = &libftdb_ftdbUnresolvedfuncs_sequence_methods,
    .tp_doc = "libftdb ftdbUnresolvedfuncs object",
    .tp_iter = libftdb_ftdb_unresolvedfuncs_getiter,
    .tp_methods = libftdb_ftdbUnresolvedfuncs_methods,
    .tp_new = libftdb_ftdb_collection_new,
};
