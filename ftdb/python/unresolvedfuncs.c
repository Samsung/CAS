#include "pyftdb.h"

static void libftdb_ftdb_unresolvedfuncs_dealloc(libftdb_ftdb_unresolvedfuncs_object *self) {
    Py_DecRef((PyObject *)self->py_ftdb);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_unresolvedfuncs_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_unresolvedfuncs_object *self;

    self = (libftdb_ftdb_unresolvedfuncs_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        self->py_ftdb = (const libftdb_ftdb_object *)PyLong_AsLong(PyTuple_GetItem(args, 1));
        Py_IncRef((PyObject *)self->py_ftdb);
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_unresolvedfuncs_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_unresolvedfuncs_object *__self = (libftdb_ftdb_unresolvedfuncs_object *)self;
    int written = snprintf(repr, 1024, "<ftdbUnresolvedfuncs object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "%ld unresolvedfuncs>", __self->ftdb->unresolvedfuncs_count);

    return PyUnicode_FromString(repr);
}

static Py_ssize_t libftdb_ftdb_unresolvedfuncs_sq_length(PyObject *self) {
    libftdb_ftdb_unresolvedfuncs_object *__self = (libftdb_ftdb_unresolvedfuncs_object *)self;
    return __self->ftdb->unresolvedfuncs_count;
}

static PyObject *libftdb_ftdb_unresolvedfuncs_getiter(PyObject *self) {
    libftdb_ftdb_unresolvedfuncs_object *__self = (libftdb_ftdb_unresolvedfuncs_object *)self;

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
    .sq_length = libftdb_ftdb_unresolvedfuncs_sq_length,
};

PyTypeObject libftdb_ftdbUnresolvedfuncsType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbUnresolvedfuncs",
    .tp_basicsize = sizeof(libftdb_ftdbUnresolvedfuncsType),
    .tp_dealloc = (destructor)libftdb_ftdb_unresolvedfuncs_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_unresolvedfuncs_repr,
    .tp_as_sequence = &libftdb_ftdbUnresolvedfuncs_sequence_methods,
    .tp_doc = "libftdb ftdbUnresolvedfuncs object",
    .tp_iter = libftdb_ftdb_unresolvedfuncs_getiter,
    .tp_methods = libftdb_ftdbUnresolvedfuncs_methods,
    .tp_new = libftdb_ftdb_unresolvedfuncs_new,
};
