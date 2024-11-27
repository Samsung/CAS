#include "pyftdb.h"

static void libftdb_ftdb_modules_dealloc(libftdb_ftdb_modules_object *self) {
    Py_DecRef((PyObject *)self->py_ftdb);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_modules_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_modules_object *self;

    self = (libftdb_ftdb_modules_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        self->py_ftdb = (const libftdb_ftdb_object *)PyLong_AsLong(PyTuple_GetItem(args, 1));
        Py_IncRef((PyObject *)self->py_ftdb);
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_modules_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_modules_object *__self = (libftdb_ftdb_modules_object *)self;
    int written = snprintf(repr, 1024, "<ftdbModules object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "%ld modules>", __self->ftdb->moduleindex_table_count);

    return PyUnicode_FromString(repr);
}

static Py_ssize_t libftdb_ftdb_modules_sq_length(PyObject *self) {
    libftdb_ftdb_modules_object *__self = (libftdb_ftdb_modules_object *)self;
    return __self->ftdb->moduleindex_table_count;
}

static PyObject *libftdb_ftdb_modules_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_modules_object *__self = (libftdb_ftdb_modules_object *)self;

    if (PyLong_Check(slice)) {
        unsigned long index = PyLong_AsUnsignedLong(slice);
        if (index >= __self->ftdb->moduleindex_table_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "module table index out of range: %ld\n", index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        return PyUnicode_FromString(__self->ftdb->moduleindex_table[index]);
    } else if (PyUnicode_Check(slice)) {
        const char *key = PyString_get_c_str(slice);
        struct stringRefMap_node *node = stringRefMap_search(&__self->ftdb->modulemap, key);
        if (!node) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "module table file missing: %s\n", key);
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

static int libftdb_ftdb_modules_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in module contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_modules_object *__self = (libftdb_ftdb_modules_object *)self;
    const char *ckey = PyString_get_c_str(key);
    struct stringRefMap_node *node = stringRefMap_search(&__self->ftdb->modulemap, ckey);
    PYASSTR_DECREF(ckey);
    if (node)
        return 1;
    return 0;
}

static PyObject *libftdb_ftdb_modules_getiter(PyObject *self) {
    libftdb_ftdb_modules_object *__self = (libftdb_ftdb_modules_object *)self;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
    PYTUPLE_SET_ULONG(args, 1, 0);
    PyObject *iter = PyObject_CallObject((PyObject *)&libftdb_ftdbModulesIterType, args);
    Py_DecRef(args);
    return iter;
}

PySequenceMethods libftdb_ftdbModules_sequence_methods = {
    .sq_length = libftdb_ftdb_modules_sq_length,
    .sq_contains = libftdb_ftdb_modules_sq_contains
};

PyMappingMethods libftdb_ftdbModules_mapping_methods = {
    .mp_subscript = libftdb_ftdb_modules_mp_subscript,
};

PyTypeObject libftdb_ftdbModulesType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbModules",
    .tp_basicsize = sizeof(libftdb_ftdbModulesType),
    .tp_dealloc = (destructor)libftdb_ftdb_modules_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_modules_repr,
    .tp_as_sequence = &libftdb_ftdbModules_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbModules_mapping_methods,
    .tp_doc = "libftdb ftdbModules object",
    .tp_iter = libftdb_ftdb_modules_getiter,
    .tp_new = libftdb_ftdb_modules_new,
};
