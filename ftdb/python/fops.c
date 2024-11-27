#include "pyftdb.h"

static void libftdb_ftdb_fops_dealloc(libftdb_ftdb_fops_object *self) {
    Py_DecRef((PyObject *)self->py_ftdb);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_fops_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_fops_object *self;

    self = (libftdb_ftdb_fops_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        self->py_ftdb = (const libftdb_ftdb_object *)PyLong_AsLong(PyTuple_GetItem(args, 1));
        Py_IncRef((PyObject *)self->py_ftdb);
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_fops_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_fops_object *__self = (libftdb_ftdb_fops_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFops object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "%ld fops variables>", __self->ftdb->fops_count);

    return PyUnicode_FromString(repr);
}

static Py_ssize_t libftdb_ftdb_fops_sq_length(PyObject *self) {
    libftdb_ftdb_fops_object *__self = (libftdb_ftdb_fops_object *)self;
    return __self->ftdb->fops_count;
}

static PyObject *libftdb_ftdb_fops_getiter(PyObject *self) {
    libftdb_ftdb_fops_object *__self = (libftdb_ftdb_fops_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0);
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbFopsIterType, args);

    Py_DecRef(args);
    return iter;
}

static PyObject *libftdb_ftdb_fops_mp_subscript(PyObject *self, PyObject *slice) {
    libftdb_ftdb_fops_object *__self = (libftdb_ftdb_fops_object *)self;

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
    .sq_length = libftdb_ftdb_fops_sq_length,
};

PyMappingMethods libftdb_ftdbFops_mapping_methods = {
    .mp_subscript = libftdb_ftdb_fops_mp_subscript,
};

PyTypeObject libftdb_ftdbFopsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
        .tp_name = "libftdb.ftdbFops",
    .tp_basicsize = sizeof(libftdb_ftdbFopsType),
    .tp_dealloc = (destructor)libftdb_ftdb_fops_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_fops_repr,
    .tp_as_sequence = &libftdb_ftdbFops_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFops_mapping_methods,
    .tp_doc = "libftdb ftdbFops object",
    .tp_iter = libftdb_ftdb_fops_getiter,
    .tp_new = libftdb_ftdb_fops_new,
};
