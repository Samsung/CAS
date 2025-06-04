#include "pyftdb.h"

static void libftdb_ftdb_func_offsetrefinfo_entry_dealloc(libftdb_ftdb_func_offsetrefinfo_entry_object *self) {
    Py_DecRef((PyObject *)self->deref_entry);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *self;

    self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self == NULL)
        return NULL;

    self->deref_entry = (const libftdb_ftdb_func_derefinfo_entry_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
    Py_IncRef((PyObject *)self->deref_entry);

    unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args, 1));
    if (entry_index >= self->deref_entry->entry->offsetrefs_count)
        return PyErr_Format(libftdb_ftdbError, "offsetrefinfo entry index out of bounds: %lu", entry_index);

    self->entry_index = entry_index;
    self->entry = &self->deref_entry->entry->offsetrefs[self->entry_index];
    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_repr(PyObject *self) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;
    return PyUnicode_FromFormat("<ftdbFuncOffsetrefInfoEntry object at %p : fdx:%lu|ddx:%lu|edx:%lu>", self, __self->deref_entry->func_entry->index, __self->deref_entry->entry_index, __self->entry_index);
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;
    if (__self->entry->kind == EXPR_STRING) {
        return PyUnicode_FromString(__self->entry->id_string);
    } else if (__self->entry->kind == EXPR_FLOATING) {
        return PyFloat_FromDouble(__self->entry->id_float);
    } else {
        return PyLong_FromLong(__self->entry->id_integer);
    }
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_richcompare(PyObject *self, PyObject *other, int op) {
    bool equals = true;
    PyObject *self_id, *other_id;
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;
    libftdb_ftdb_func_offsetrefinfo_entry_object *__other = (libftdb_ftdb_func_offsetrefinfo_entry_object *)other;

    if (op != Py_EQ && op != Py_NE)
        Py_RETURN_NOTIMPLEMENTED;

    if (!PyObject_IsInstance(other, (PyObject *)&libftdb_ftdbFuncOffsetrefInfoEntryType))
        Py_RETURN_FALSE;

    if (__self->entry->kind != __other->entry->kind) {
        equals = false;
        goto out;
    }

    if (!SAFE_OPTIONAL_COMPARE(__self->entry->mi, __other->entry->mi))
        equals = false;
    if (!SAFE_OPTIONAL_COMPARE(__self->entry->di, __other->entry->di))
        equals = false;
    if (!SAFE_OPTIONAL_COMPARE(__self->entry->cast, __other->entry->cast))
        equals = false;

    // Comapare IDs
    self_id = libftdb_ftdb_func_offsetrefinfo_entry_get_id((PyObject *)__self, NULL);
    other_id = libftdb_ftdb_func_offsetrefinfo_entry_get_id((PyObject *)__other, NULL);
    if (!PyObject_RichCompareBool(self_id, other_id, Py_EQ))
        equals = false;
    Py_DecRef(self_id);
    Py_DecRef(other_id);

out:
    if ((op == Py_EQ && equals) || (op == Py_NE && !equals))
        Py_RETURN_TRUE;
    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_get_kind(PyObject *self, void *closure) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;
    return PyLong_FromLong((long)__self->entry->kind);
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_get_kindname(PyObject *self, void *closure) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;
    return PyUnicode_FromString(set_exprType(__self->entry->kind));
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_get_mi(PyObject *self, void *closure) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;

    if (!__self->entry->mi) {
        PyErr_SetString(libftdb_ftdbError, "No 'mi' field in offsetrefinfo entry");
        return 0;
    }
    return PyLong_FromUnsignedLong(*__self->entry->mi);
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_get_di(PyObject *self, void *closure) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;

    if (!__self->entry->di) {
        PyErr_SetString(libftdb_ftdbError, "No 'di' field in offsetrefinfo entry");
        return 0;
    }
    return PyLong_FromUnsignedLong(*__self->entry->di);
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_get_cast(PyObject *self, void *closure) {
    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;

    if (!__self->entry->cast) {
        PyErr_SetString(libftdb_ftdbError, "No 'cast' field in offsetrefinfo entry");
        return 0;
    }
    return PyLong_FromUnsignedLong(*__self->entry->cast);
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_has_cast(libftdb_ftdb_func_offsetrefinfo_entry_object *self, PyObject *args) {
    if (self->entry->cast) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_json(libftdb_ftdb_func_offsetrefinfo_entry_object *self, PyObject *args) {
    PyObject *py_offsetref_entry = PyDict_New();

    struct offsetref_info *offsetref_info = self->entry;
    FTDB_SET_ENTRY_ENUM_TO_STRING(py_offsetref_entry, kind, offsetref_info->kind, exprType);
    if (offsetref_info->kind == EXPR_STRING) {
        FTDB_SET_ENTRY_STRING(py_offsetref_entry, id, offsetref_info->id_string);
    } else if (offsetref_info->kind == EXPR_FLOATING) {
        FTDB_SET_ENTRY_FLOAT(py_offsetref_entry, id, offsetref_info->id_float);
    } else {
        FTDB_SET_ENTRY_INT64(py_offsetref_entry, id, offsetref_info->id_integer);
    }
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry, mi, offsetref_info->mi);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry, di, offsetref_info->di);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry, cast, offsetref_info->cast);

    return py_offsetref_entry;
}

static PyObject *libftdb_ftdb_func_offsetrefinfo_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "kind")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_offsetrefinfo_entry_get_kindname(self, 0);
    } else if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_offsetrefinfo_entry_get_id(self, 0);
    } else if (!strcmp(attr, "mi")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_offsetrefinfo_entry_get_mi(self, 0);
    } else if (!strcmp(attr, "di")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_offsetrefinfo_entry_get_di(self, 0);
    } else if (!strcmp(attr, "cast")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_offsetrefinfo_entry_get_cast(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_offsetrefinfo_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_func_offsetrefinfo_entry_object *__self = (libftdb_ftdb_func_offsetrefinfo_entry_object *)self;
    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "kind")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "mi")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->mi;
    } else if (!strcmp(attr, "di")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->di;
    } else if (!strcmp(attr, "cast")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->cast;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

PyMethodDef libftdb_ftdbFuncOffsetrefInfoEntry_methods[] = {
    {"has_cast", (PyCFunction)libftdb_ftdb_func_offsetrefinfo_entry_has_cast, METH_VARARGS, "Returns True if the offsetref entry contains cast information"},
    {"json", (PyCFunction)libftdb_ftdb_func_offsetrefinfo_entry_json, METH_VARARGS, "Returns the json representation of the offsetref entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncOffsetrefInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_offsetrefinfo_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFuncOffsetrefInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_offsetrefinfo_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncOffsetrefInfoEntry_getset[] = {
    {"kind", libftdb_ftdb_func_offsetrefinfo_entry_get_kind, 0, "ftdb func offsetrefinfo entry kind value", 0},
    {"kindname", libftdb_ftdb_func_offsetrefinfo_entry_get_kindname, 0, "ftdb func offsetrefinfo entry kind name value", 0},
    {"id", libftdb_ftdb_func_offsetrefinfo_entry_get_id, 0, "ftdb func offsetrefinfo entry id value", 0},
    {"mi", libftdb_ftdb_func_offsetrefinfo_entry_get_mi, 0, "ftdb func offsetrefinfo entry mi value", 0},
    {"di", libftdb_ftdb_func_offsetrefinfo_entry_get_di, 0, "ftdb func offsetrefinfo entry di values", 0},
    {"cast", libftdb_ftdb_func_offsetrefinfo_entry_get_cast, 0, "ftdb func offsetrefinfo entry cast value", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncOffsetrefInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncOffsetrefInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_func_offsetrefinfo_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_func_offsetrefinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_offsetrefinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncOffsetrefInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncOffsetrefInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncOffsetrefInfoEntry object",
    .tp_richcompare = libftdb_ftdb_func_offsetrefinfo_entry_richcompare,
    .tp_methods = libftdb_ftdbFuncOffsetrefInfoEntry_methods,
    .tp_getset = libftdb_ftdbFuncOffsetrefInfoEntry_getset,
    .tp_new = libftdb_ftdb_func_offsetrefinfo_entry_new,
};
