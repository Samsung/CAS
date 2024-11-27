#include "pyftdb.h"

static void libftdb_ftdb_func_callinfo_entry_dealloc(libftdb_ftdb_func_callinfo_entry_object *self) {
    Py_DecRef((PyObject *)self->func_entry);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_callinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_callinfo_entry_object *self;

    self = (libftdb_ftdb_func_callinfo_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->func_entry = (const libftdb_ftdb_func_entry_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->func_entry);
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (func_index >= self->func_entry->funcs->ftdb->funcs_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "func entry index out of bounds: %lu\n", func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args, 2));
        self->is_refcall = PyLong_AsUnsignedLong(PyTuple_GetItem(args, 3));

        if (self->is_refcall == 0) {
            if (entry_index >= self->func_entry->funcs->ftdb->funcs[func_index].calls_count) {
                snprintf(errmsg, ERRMSG_BUFFER_SIZE, "callinfo entry index out of bounds: %lu@%lu\n", func_index, entry_index);
                PyErr_SetString(libftdb_ftdbError, errmsg);
                return 0;
            }
            self->entry_index = entry_index;
            self->entry = &self->func_entry->funcs->ftdb->funcs[self->func_index].call_info[self->entry_index];
        } else {
            if (entry_index >= self->func_entry->funcs->ftdb->funcs[func_index].refcalls_count) {
                snprintf(errmsg, ERRMSG_BUFFER_SIZE, "refcallinfo entry index out of bounds: %lu@%lu\n", func_index, entry_index);
                PyErr_SetString(libftdb_ftdbError, errmsg);
                return 0;
            }
            self->entry_index = entry_index;
            self->entry = &self->func_entry->funcs->ftdb->funcs[self->func_index].refcall_info[self->entry_index];
        }
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_callinfo_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncCallInfoEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "fdx:%lu|edx:%lu>", __self->func_index, __self->entry_index);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_start(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    return PyUnicode_FromString(__self->entry->start);
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_end(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    return PyUnicode_FromString(__self->entry->end);
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_ord(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->ord);
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_expr(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    return PyUnicode_FromString(__self->entry->expr);
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_loc(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    if (__self->entry->loc) {
        return PyUnicode_FromString(__self->entry->loc);
    } else {
        Py_RETURN_NONE;
    }
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_csid(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;
    if (__self->entry->csid) {
        return PyLong_FromLong(*__self->entry->csid);
    } else {
        Py_RETURN_NONE;
    }
}

static PyObject *libftdb_ftdb_func_callinfo_entry_get_args(PyObject *self, void *closure) {
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;

    PyObject *args = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->args_count; ++i) {
        PYLIST_ADD_ULONG(args, __self->entry->args[i]);
    }
    return args;
}

static PyObject *libftdb_ftdb_func_callinfo_entry_json(libftdb_ftdb_func_callinfo_entry_object *self, PyObject *args) {
    PyObject *py_callinfo_entry = PyDict_New();

    FTDB_SET_ENTRY_STRING(py_callinfo_entry, start, self->entry->start);
    FTDB_SET_ENTRY_STRING(py_callinfo_entry, end, self->entry->end);
    FTDB_SET_ENTRY_ULONG(py_callinfo_entry, ord, self->entry->ord);
    FTDB_SET_ENTRY_STRING(py_callinfo_entry, expr, self->entry->expr);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_callinfo_entry, loc, self->entry->loc);
    FTDB_SET_ENTRY_INT64_OPTIONAL(py_callinfo_entry, csid, self->entry->csid);
    PyObject *py_args = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->args_count; ++i) {
        PYLIST_ADD_ULONG(py_args, self->entry->args[i]);
    }
    FTDB_SET_ENTRY_PYOBJECT(py_callinfo_entry, args, py_args);
    return py_callinfo_entry;
}

static PyObject *libftdb_ftdb_func_callinfo_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "start")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_start(self, 0);
    } else if (!strcmp(attr, "end")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_end(self, 0);
    } else if (!strcmp(attr, "ord")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_ord(self, 0);
    } else if (!strcmp(attr, "expr")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_expr(self, 0);
    } else if (!strcmp(attr, "loc")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_loc(self, 0);
    } else if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_csid(self, 0);
    } else if (!strcmp(attr, "args")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_callinfo_entry_get_args(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_callinfo_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);
    libftdb_ftdb_func_callinfo_entry_object *__self = (libftdb_ftdb_func_callinfo_entry_object *)self;

    if (!strcmp(attr, "start")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "end")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "ord")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "expr")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "loc")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->loc;
    } else if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "args")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

PyMethodDef libftdb_ftdbFuncCallInfoEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_func_callinfo_entry_json, METH_VARARGS, "Returns the json representation of the callinfo entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncCallInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_callinfo_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFuncCallInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_callinfo_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncCallInfoEntry_getset[] = {
    {"start", libftdb_ftdb_func_callinfo_entry_get_start, 0, "ftdb func callinfo entry start value", 0},
    {"end", libftdb_ftdb_func_callinfo_entry_get_end, 0, "ftdb func callinfo entry end value", 0},
    {"ord", libftdb_ftdb_func_callinfo_entry_get_ord, 0, "ftdb func callinfo entry ord value", 0},
    {"expr", libftdb_ftdb_func_callinfo_entry_get_expr, 0, "ftdb func callinfo entry expr value", 0},
    {"loc", libftdb_ftdb_func_callinfo_entry_get_loc, 0, "ftdb func callinfo entry loc value", 0},
    {"csid", libftdb_ftdb_func_callinfo_entry_get_csid, 0, "ftdb func callinfo entry csid value", 0},
    {"args", libftdb_ftdb_func_callinfo_entry_get_args, 0, "ftdb func callinfo entry args values", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncCallInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncCallInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncCallInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_callinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_callinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncCallInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncCallInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncCallInfoEntry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFuncCallInfoEntry_methods,
    .tp_getset = libftdb_ftdbFuncCallInfoEntry_getset,
    .tp_new = libftdb_ftdb_func_callinfo_entry_new,
};
