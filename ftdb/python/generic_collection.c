#include "pyftdb.h"

void libftdb_ftdb_collection_dealloc(libftdb_ftdb_collection_object *self) {
    Py_DecRef((PyObject *)self->py_ftdb);
    Py_TYPE(self)->tp_free(self);
}

PyObject *libftdb_ftdb_collection_repr(PyObject *self) {
    static char repr[1024];
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;
    int written = snprintf(repr, 1024, "<%s object at %lx : ",__self->name, (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "%ld elements>", __self->count);
    return PyUnicode_FromString(repr);
}

PyObject *libftdb_ftdb_collection_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_collection_object *self;
    self = (libftdb_ftdb_collection_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        self->py_ftdb = (const libftdb_ftdb_object *)PyLong_AsLong(PyTuple_GetItem(args, 1));
        self->count = (const unsigned long)PyLong_AsLong(PyTuple_GetItem(args,2));
        if(PyTuple_Size(args)>3)
            self->name = (const char *)PyLong_AsLong(PyTuple_GetItem(args,3));
        else
            self->name = "ftdbCollection";
        Py_IncRef((PyObject *)self->py_ftdb);
    }
    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_collection_sq_length(PyObject *self) {
    libftdb_ftdb_collection_object *__self = (libftdb_ftdb_collection_object *)self;
    return __self->count;
}



void libftdb_ftdb_collection_iter_dealloc(libftdb_ftdb_collection_iter_object *self) {
    Py_DecRef((PyObject *)self->collection);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject *libftdb_ftdb_collection_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_collection_iter_object *self;
    self = (libftdb_ftdb_collection_iter_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->collection = (libftdb_ftdb_collection_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->collection);
        self->index = PyLong_AsLong(PyTuple_GetItem(args, 1));
    }
    return (PyObject *)self;
}

static Py_ssize_t libftdb_ftdb_collection_iter_sq_length(PyObject *self) {
    libftdb_ftdb_collection_iter_object *__self = (libftdb_ftdb_collection_iter_object *)self;
    return __self->collection->count;
}

PySequenceMethods libftdb_ftdbCollectionIter_sequence_methods = {
    .sq_length = libftdb_ftdb_collection_iter_sq_length,
};
