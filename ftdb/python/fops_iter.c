#include "pyftdb.h"

static PyObject *libftdb_ftdb_fops_iter_next(PyObject *self) {
    libftdb_ftdb_collection_iter_object *__self = (libftdb_ftdb_collection_iter_object *)self;

    if (__self->index >= __self->collection->ftdb->fops_count) {
        PyErr_SetNone(PyExc_StopIteration);
        return 0;
    }

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->collection);
    PYTUPLE_SET_ULONG(args, 1, __self->index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFopsEntryType, args);
    Py_DecRef(args);
    __self->index++;
    return entry;
}

PyTypeObject libftdb_ftdbFopsIterType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFopsIter",
    .tp_basicsize = sizeof(libftdb_ftdb_collection_iter_object),
    .tp_dealloc = (destructor)libftdb_ftdb_collection_iter_dealloc,
    .tp_as_sequence = &libftdb_ftdbCollectionIter_sequence_methods,
    .tp_doc = "libftdb ftdb fops iterator",
    .tp_iter = PyObject_SelfIter,
    .tp_iternext = libftdb_ftdb_fops_iter_next,
    .tp_new = libftdb_ftdb_collection_iter_new,
};
