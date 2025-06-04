#include "pyftdb.h"

static void libftdb_ftdb_func_ifinfo_entry_dealloc(libftdb_ftdb_func_ifinfo_entry_object *self) {
    Py_DecRef((PyObject *)self->func_entry);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_ifinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_ifinfo_entry_object *self;

    self = (libftdb_ftdb_func_ifinfo_entry_object *)subtype->tp_alloc(subtype, 0);
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
        if (entry_index >= self->func_entry->funcs->ftdb->funcs[func_index].ifs_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "ifinfo entry index out of bounds: %lu@%lu\n", func_index, entry_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->entry_index = entry_index;
        self->entry = &self->func_entry->funcs->ftdb->funcs[self->func_index].ifs[self->entry_index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_ifinfo_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_func_ifinfo_entry_object *__self = (libftdb_ftdb_func_ifinfo_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncIfInfoEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "fdx:%lu|edx:%lu>", __self->func_index, __self->entry_index);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_func_ifinfo_entry_get_csid(PyObject *self, void *closure) {
    libftdb_ftdb_func_ifinfo_entry_object *__self = (libftdb_ftdb_func_ifinfo_entry_object *)self;
    return PyLong_FromLong(__self->entry->csid);
}

static PyObject *libftdb_ftdb_func_ifinfo_entry_get_refs(PyObject *self, void *closure) {
    libftdb_ftdb_func_ifinfo_entry_object *__self = (libftdb_ftdb_func_ifinfo_entry_object *)self;

    PyObject *refs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refs_count; ++i) {
        struct ifref_info *ifref_info = &__self->entry->refs[i];
        PyObject *py_ifref_info = PyDict_New();
        FTDB_SET_ENTRY_ENUM_TO_STRING(py_ifref_info, type, ifref_info->type, exprType);
        if (ifref_info->id) {
            FTDB_SET_ENTRY_ULONG_OPTIONAL(py_ifref_info, id, ifref_info->id);
        } else if (ifref_info->integer_literal) {
            FTDB_SET_ENTRY_INT64_OPTIONAL(py_ifref_info, id, ifref_info->integer_literal);
        } else if (ifref_info->character_literal) {
            FTDB_SET_ENTRY_UINT_OPTIONAL(py_ifref_info, id, ifref_info->character_literal);
        } else if (ifref_info->floating_literal) {
            FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_ifref_info, id, ifref_info->floating_literal);
        } else if (ifref_info->string_literal) {
            FTDB_SET_ENTRY_STRING_OPTIONAL(py_ifref_info, id, ifref_info->string_literal);
        }
        PyList_Append(refs, py_ifref_info);
        Py_DecRef(py_ifref_info);
    }
    return refs;
}

static PyObject *libftdb_ftdb_func_ifinfo_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_ifinfo_entry_get_csid(self, 0);
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_ifinfo_entry_get_refs(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_ifinfo_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static PyObject *libftdb_ftdb_func_ifinfo_entry_json(libftdb_ftdb_func_ifinfo_entry_object *self, PyObject *args) {
    libftdb_ftdb_func_ifinfo_entry_object *__self = (libftdb_ftdb_func_ifinfo_entry_object *)self;

    PyObject *py_ifinfo_entry = PyDict_New();

    PyObject *refs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refs_count; ++i) {
        struct ifref_info *ifref_info = &__self->entry->refs[i];
        PyObject *py_ifref_info = PyDict_New();
        FTDB_SET_ENTRY_ENUM_TO_STRING(py_ifref_info, type, ifref_info->type, exprType);
        if (ifref_info->id) {
            FTDB_SET_ENTRY_ULONG_OPTIONAL(py_ifref_info, id, ifref_info->id);
        } else if (ifref_info->integer_literal) {
            FTDB_SET_ENTRY_INT64_OPTIONAL(py_ifref_info, id, ifref_info->integer_literal);
        } else if (ifref_info->character_literal) {
            FTDB_SET_ENTRY_UINT_OPTIONAL(py_ifref_info, id, ifref_info->character_literal);
        } else if (ifref_info->floating_literal) {
            FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_ifref_info, id, ifref_info->floating_literal);
        } else if (ifref_info->string_literal) {
            FTDB_SET_ENTRY_STRING_OPTIONAL(py_ifref_info, id, ifref_info->string_literal);
        }
        PyList_Append(refs, py_ifref_info);
        Py_DecRef(py_ifref_info);
    }

    FTDB_SET_ENTRY_PYOBJECT(py_ifinfo_entry, refs, refs);
    FTDB_SET_ENTRY_ULONG(py_ifinfo_entry, csid, __self->entry->csid);

    return py_ifinfo_entry;
}

PyMethodDef libftdb_ftdbFuncIfInfoEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_func_ifinfo_entry_json, METH_VARARGS, "Returns the json representation of the ifinfo entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncIfInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_ifinfo_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFuncIfnfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_ifinfo_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncIfInfoEntry_getset[] = {
    {"csid", libftdb_ftdb_func_ifinfo_entry_get_csid, 0, "ftdb func ifinfo entry csid value", 0},
    {"refs", libftdb_ftdb_func_ifinfo_entry_get_refs, 0, "ftdb func ifinfo entry refs values", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncIfInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncIfInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_func_ifinfo_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_func_ifinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_ifinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncIfInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncIfnfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncIfInfoEntry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFuncIfInfoEntry_methods,
    .tp_getset = libftdb_ftdbFuncIfInfoEntry_getset,
    .tp_new = libftdb_ftdb_func_ifinfo_entry_new,
};
