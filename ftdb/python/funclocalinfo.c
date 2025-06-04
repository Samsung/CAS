#include "pyftdb.h"

static void libftdb_ftdb_func_localinfo_entry_dealloc(libftdb_ftdb_func_localinfo_entry_object *self) {
    Py_DecRef((PyObject *)self->func_entry);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_localinfo_entry_object *self;

    self = (libftdb_ftdb_func_localinfo_entry_object *)subtype->tp_alloc(subtype, 0);
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
        if (entry_index >= self->func_entry->funcs->ftdb->funcs[func_index].locals_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "localinfo entry index out of bounds: %lu@%lu\n", func_index, entry_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->entry_index = entry_index;
        self->entry = &self->func_entry->funcs->ftdb->funcs[self->func_index].locals[self->entry_index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_localinfo_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncLocalInfoEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "fdx:%lu|edx:%lu>", __self->func_index, __self->entry_index);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->id);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_get_name(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    return PyUnicode_FromString(__self->entry->name);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_is_parm(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;

    if (__self->entry->isparm) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_localinfo_entry_get_type(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->type);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_is_static(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;

    if (__self->entry->isstatic) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_localinfo_entry_is_used(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;

    if (__self->entry->isused) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_localinfo_entry_get_csid(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    if (!__self->entry->csid) {
        PyErr_SetString(libftdb_ftdbError, "No 'csid' field in localinfo entry");
        return 0;
    }
    return PyLong_FromUnsignedLong(*__self->entry->csid);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_get_location(PyObject *self, void *closure) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    return PyUnicode_FromString(__self->entry->location);
}

static PyObject *libftdb_ftdb_func_localinfo_entry_has_csid(libftdb_ftdb_func_entry_object *self, PyObject *args) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;

    if (__self->entry->csid) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_localinfo_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_get_id(self, 0);
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_get_name(self, 0);
    } else if (!strcmp(attr, "parm")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_is_parm(self, 0);
    } else if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_get_type(self, 0);
    } else if (!strcmp(attr, "static")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_is_static(self, 0);
    } else if (!strcmp(attr, "used")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_is_used(self, 0);
    } else if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_get_csid(self, 0);
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_localinfo_entry_get_location(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_localinfo_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;
    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "parm")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "static")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "used")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->csid;
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static PyObject *libftdb_ftdb_func_localinfo_entry_json(libftdb_ftdb_func_localinfo_entry_object *self, PyObject *args) {
    libftdb_ftdb_func_localinfo_entry_object *__self = (libftdb_ftdb_func_localinfo_entry_object *)self;

    PyObject *py_localinfo_entry = PyDict_New();

    FTDB_SET_ENTRY_ULONG(py_localinfo_entry, id, __self->entry->id);
    FTDB_SET_ENTRY_STRING(py_localinfo_entry, name, __self->entry->name);
    FTDB_SET_ENTRY_BOOL(py_localinfo_entry, parm, __self->entry->isparm);
    FTDB_SET_ENTRY_ULONG(py_localinfo_entry, type, __self->entry->type);
    FTDB_SET_ENTRY_BOOL(py_localinfo_entry, static, __self->entry->isstatic);
    FTDB_SET_ENTRY_BOOL(py_localinfo_entry, used, __self->entry->isused);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_localinfo_entry, csid, __self->entry->csid);
    FTDB_SET_ENTRY_STRING(py_localinfo_entry, location, __self->entry->location);

    return py_localinfo_entry;
}

PyMethodDef libftdb_ftdbFuncLocalInfoEntry_methods[] = {
    {"has_csid", (PyCFunction)libftdb_ftdb_func_localinfo_entry_has_csid, METH_VARARGS, "Returns True if func localinfo entry has the csid entry"},
    {"json", (PyCFunction)libftdb_ftdb_func_localinfo_entry_json, METH_VARARGS, "Returns the json representation of the localinfo entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncLocalInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_localinfo_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFuncLocalInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_localinfo_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncLocalInfoEntry_getset[] = {
    {"id", libftdb_ftdb_func_localinfo_entry_get_id, 0, "ftdb func localinfo entry id values", 0},
    {"name", libftdb_ftdb_func_localinfo_entry_get_name, 0, "ftdb func localinfo entry name values", 0},
    {"parm", libftdb_ftdb_func_localinfo_entry_is_parm, 0, "ftdb func localinfo entry is parm values", 0},
    {"type", libftdb_ftdb_func_localinfo_entry_get_type, 0, "ftdb func localinfo entry type values", 0},
    {"static", libftdb_ftdb_func_localinfo_entry_is_static, 0, "ftdb func localinfo entry is static values", 0},
    {"used", libftdb_ftdb_func_localinfo_entry_is_used, 0, "ftdb func localinfo entry is used values", 0},
    {"csid", libftdb_ftdb_func_localinfo_entry_get_csid, 0, "ftdb func localinfo entry csid values", 0},
    {"location", libftdb_ftdb_func_localinfo_entry_get_location, 0, "ftdb func localinfo entry location values", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncLocalInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncLocalInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_func_localinfo_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_func_localinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_localinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncLocalInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncLocalInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncLocalInfoEntry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFuncLocalInfoEntry_methods,
    .tp_getset = libftdb_ftdbFuncLocalInfoEntry_getset,
    .tp_new = libftdb_ftdb_func_localinfo_entry_new,
};
