#include "pyftdb.h"

static void libftdb_ftdb_types_iter_dealloc(libftdb_ftdb_types_iter_object *self) {
    Py_DecRef((PyObject *)self->types);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_types_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_types_iter_object *self;

    self = (libftdb_ftdb_types_iter_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->types = (libftdb_ftdb_types_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->types);
        self->index = PyLong_AsLong(PyTuple_GetItem(args, 1));
    }

    return (PyObject *)self;
}

static Py_ssize_t libftdb_ftdb_types_iter_sq_length(PyObject *self) {
    libftdb_ftdb_types_iter_object *__self = (libftdb_ftdb_types_iter_object *)self;
    return __self->types->ftdb->types_count;
}

static PyObject *libftdb_ftdb_types_iter_next(PyObject *self) {
    libftdb_ftdb_types_iter_object *__self = (libftdb_ftdb_types_iter_object *)self;

    if (__self->index >= __self->types->ftdb->types_count) {
        PyErr_SetNone(PyExc_StopIteration);
        return 0;
    }

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->types);
    PYTUPLE_SET_ULONG(args, 1, __self->index);
    PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbTypeEntryType, args);
    Py_DecRef(args);
    __self->index++;
    return entry;
}

PySequenceMethods libftdb_ftdbTypesIter_sequence_methods = {
    .sq_length = libftdb_ftdb_types_iter_sq_length,
};

PyTypeObject libftdb_ftdbTypesIterType = {
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
