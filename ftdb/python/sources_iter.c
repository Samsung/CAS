#include "pyftdb.h"

static void libftdb_ftdb_sources_iter_dealloc(libftdb_ftdb_sources_iter_object *self) {
    Py_DecRef((PyObject *)self->sources);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_sources_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_sources_iter_object *self;

    self = (libftdb_ftdb_sources_iter_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->sources = (libftdb_ftdb_sources_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->sources);
        self->index = PyLong_AsLong(PyTuple_GetItem(args, 1));
    }

    return (PyObject *)self;
}

static Py_ssize_t libftdb_ftdb_sources_iter_sq_length(PyObject *self) {
    libftdb_ftdb_sources_iter_object *__self = (libftdb_ftdb_sources_iter_object *)self;
    return __self->sources->ftdb->sourceindex_table_count;
}

static PyObject *libftdb_ftdb_sources_iter_next(PyObject *self) {
    libftdb_ftdb_sources_iter_object *__self = (libftdb_ftdb_sources_iter_object *)self;

    if (__self->index >= __self->sources->ftdb->sourceindex_table_count) {
        PyErr_SetNone(PyExc_StopIteration);
        return 0;
    }

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, __self->index);
    PYTUPLE_SET_STR(args, 1, __self->sources->ftdb->sourceindex_table[__self->index]);
    __self->index++;
    return args;
}

PySequenceMethods libftdb_ftdbSourcesIter_sequence_methods = {
    .sq_length = libftdb_ftdb_sources_iter_sq_length,
};

PyTypeObject libftdb_ftdbSourcesIterType = {
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
