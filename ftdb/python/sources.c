#include "pyftdb.h"

static void libftdb_ftdb_sources_dealloc(libftdb_ftdb_sources_object *self) {
    Py_DecRef((PyObject *)self->py_ftdb);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_sources_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_sources_object *self;

    self = (libftdb_ftdb_sources_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        self->py_ftdb = (const libftdb_ftdb_object *)PyLong_AsLong(PyTuple_GetItem(args, 1));
        Py_IncRef((PyObject *)self->py_ftdb);
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_sources_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_sources_object *__self = (libftdb_ftdb_sources_object *)self;
    int written = snprintf(repr, 1024, "<ftdbSources object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "%ld sources>", __self->ftdb->sourceindex_table_count);

    return PyUnicode_FromString(repr);
}

static Py_ssize_t libftdb_ftdb_sources_sq_length(PyObject *self) {
    libftdb_ftdb_sources_object *__self = (libftdb_ftdb_sources_object *)self;
    return __self->ftdb->sourceindex_table_count;
}

static PyObject *libftdb_ftdb_sources_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_sources_object *__self = (libftdb_ftdb_sources_object *)self;

    if (PyLong_Check(slice)) {
        unsigned long index = PyLong_AsUnsignedLong(slice);
        if (index >= __self->ftdb->sourceindex_table_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "source table index out of range: %ld\n", index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        return PyUnicode_FromString(__self->ftdb->sourceindex_table[index]);
    } else if (PyUnicode_Check(slice)) {
        const char *key = PyString_get_c_str(slice);
        struct stringRefMap_node *node = stringRefMap_search(&__self->ftdb->sourcemap, key);
        if (!node) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "source table file missing: %s\n", key);
            PYASSTR_DECREF(key);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        PYASSTR_DECREF(key);
        return PyLong_FromUnsignedLong(node->value);
    } else {
        PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
        return 0;
    }
}

static int libftdb_ftdb_sources_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in source contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_sources_object *__self = (libftdb_ftdb_sources_object *)self;
    const char *ckey = PyString_get_c_str(key);
    struct stringRefMap_node *node = stringRefMap_search(&__self->ftdb->sourcemap, ckey);
    PYASSTR_DECREF(ckey);
    if (node)
        return 1;
    return 0;
}

static PyObject *libftdb_ftdb_sources_getiter(PyObject *self) {
    libftdb_ftdb_sources_object *__self = (libftdb_ftdb_sources_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0);
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbSourcesIterType, args);
    Py_DecRef(args);
    return iter;
}

PySequenceMethods libftdb_ftdbSources_sequence_methods = {
    .sq_length = libftdb_ftdb_sources_sq_length,
    .sq_contains = libftdb_ftdb_sources_sq_contains
};

PyMappingMethods libftdb_ftdbSources_mapping_methods = {
    .mp_subscript = libftdb_ftdb_sources_mp_subscript,
};

PyTypeObject libftdb_ftdbSourcesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbSources",
    .tp_basicsize = sizeof(libftdb_ftdbSourcesType),
    .tp_dealloc = (destructor)libftdb_ftdb_sources_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_sources_repr,
    .tp_as_sequence = &libftdb_ftdbSources_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbSources_mapping_methods,
    .tp_doc = "libftdb ftdbSources object",
    .tp_iter = libftdb_ftdb_sources_getiter,
    .tp_new = libftdb_ftdb_sources_new,
};
