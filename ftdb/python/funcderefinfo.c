#include "pyftdb.h"

static void libftdb_ftdb_func_derefinfo_entry_dealloc(libftdb_ftdb_func_derefinfo_entry_object *self) {
    Py_DecRef((PyObject *)self->func_entry);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_derefinfo_entry_object *self;

    self = (libftdb_ftdb_func_derefinfo_entry_object *)subtype->tp_alloc(subtype, 0);
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
        if (entry_index >= self->func_entry->funcs->ftdb->funcs[func_index].derefs_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "derefinfo entry index out of bounds: %lu@%lu\n", func_index, entry_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->entry_index = entry_index;
        self->entry = &self->func_entry->funcs->ftdb->funcs[self->func_index].derefs[self->entry_index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncDerefInfoEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "fdx:%lu|edx:%lu>", __self->func_index, __self->entry_index);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_kind(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;
    return PyLong_FromLong((long)__self->entry->kind);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_kindname(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;
    return PyUnicode_FromString(set_exprType(__self->entry->kind));
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_offset(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->offset) {
        PyErr_SetString(libftdb_ftdbError, "No 'offset' field in derefinfo entry");
        return 0;
    }
    return PyLong_FromLong(*__self->entry->offset);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_basecnt(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->basecnt) {
        PyErr_SetString(libftdb_ftdbError, "No 'basecnt' field in derefinfo entry");
        return 0;
    }
    return PyLong_FromUnsignedLong(*__self->entry->basecnt);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_offsetrefs(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    PyObject *offsetrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->offsetrefs_count; ++i) {
        PyObject *args = PyTuple_New(4);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, i);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncOffsetrefInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(offsetrefs, entry);
        Py_DecRef(entry);
    }
    return offsetrefs;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_expr(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;
    return PyUnicode_FromString(__self->entry->expr);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_csid(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;
    return PyLong_FromLong(__self->entry->csid);
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_ords(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    PyObject *ords = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->ord_count; ++i) {
        PYLIST_ADD_ULONG(ords, __self->entry->ord[i]);
    }
    return ords;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_member(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->member) {
        PyErr_SetString(libftdb_ftdbError, "No 'member' field in derefinfo entry");
        return 0;
    }

    PyObject *member = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->member_count; ++i) {
        PYLIST_ADD_LONG(member, __self->entry->member[i]);
    }
    return member;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_type(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->type) {
        PyErr_SetString(libftdb_ftdbError, "No 'type' field in derefinfo entry");
        return 0;
    }

    PyObject *type = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->type_count; ++i) {
        PYLIST_ADD_ULONG(type, __self->entry->type[i]);
    }
    return type;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_access(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->access) {
        PyErr_SetString(libftdb_ftdbError, "No 'access' field in derefinfo entry");
        return 0;
    }

    PyObject *access = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->access_count; ++i) {
        PYLIST_ADD_ULONG(access, __self->entry->access[i]);
    }
    return access;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_shift(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->shift) {
        PyErr_SetString(libftdb_ftdbError, "No 'shift' field in derefinfo entry");
        return 0;
    }

    PyObject *shift = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->shift_count; ++i) {
        PYLIST_ADD_LONG(shift, __self->entry->shift[i]);
    }
    return shift;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_get_mcall(PyObject *self, void *closure) {
    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;

    if (!__self->entry->mcall) {
        PyErr_SetString(libftdb_ftdbError, "No 'mcall' field in derefinfo entry");
        return 0;
    }

    PyObject *mcall = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->mcall_count; ++i) {
        PYLIST_ADD_LONG(mcall, __self->entry->mcall[i]);
    }
    return mcall;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_json(libftdb_ftdb_func_derefinfo_entry_object *self, PyObject *args) {
    PyObject *py_deref_entry = PyDict_New();

    FTDB_SET_ENTRY_ENUM_TO_STRING(py_deref_entry, kind, self->entry->kind, exprType);
    FTDB_SET_ENTRY_INT64_OPTIONAL(py_deref_entry, offset, self->entry->offset);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_deref_entry, basecnt, self->entry->basecnt);
    PyObject *offsetrefs = PyList_New(0);
    for (unsigned long j = 0; j < self->entry->offsetrefs_count; ++j) {
        struct offsetref_info *offsetref_info = &self->entry->offsetrefs[j];
        PyObject *py_offsetref_entry = PyDict_New();
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
        PYLIST_ADD_PYOBJECT(offsetrefs, py_offsetref_entry);
    }
    FTDB_SET_ENTRY_PYOBJECT(py_deref_entry, offsetrefs, offsetrefs);
    FTDB_SET_ENTRY_STRING(py_deref_entry, expr, self->entry->expr);
    FTDB_SET_ENTRY_INT64(py_deref_entry, csid, self->entry->csid);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_deref_entry, ord, self->entry->ord);
    FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry, member, self->entry->member);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry, type, self->entry->type);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry, access, self->entry->access);
    FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry, shift, self->entry->shift);
    FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry, mcall, self->entry->mcall);

    return py_deref_entry;
}

static PyObject *libftdb_ftdb_func_derefinfo_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "kind")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_kindname(self, 0);
    } else if (!strcmp(attr, "offset")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_offset(self, 0);
    } else if (!strcmp(attr, "basecnt")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_basecnt(self, 0);
    } else if (!strcmp(attr, "offsetrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_offsetrefs(self, 0);
    } else if (!strcmp(attr, "expr")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_expr(self, 0);
    } else if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_csid(self, 0);
    } else if (!strcmp(attr, "ord")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_ords(self, 0);
    } else if (!strcmp(attr, "member")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_member(self, 0);
    } else if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_type(self, 0);
    } else if (!strcmp(attr, "access")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_access(self, 0);
    } else if (!strcmp(attr, "shift")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_shift(self, 0);
    } else if (!strcmp(attr, "mcall")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_derefinfo_entry_get_mcall(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_derefinfo_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_func_derefinfo_entry_object *__self = (libftdb_ftdb_func_derefinfo_entry_object *)self;
    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "kind")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "offset")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->offset;
    } else if (!strcmp(attr, "basecnt")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->basecnt;
    } else if (!strcmp(attr, "offsetrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "expr")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "csid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "ord")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "member")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->member;
    } else if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->type;
    } else if (!strcmp(attr, "access")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->access;
    } else if (!strcmp(attr, "shift")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->shift;
    } else if (!strcmp(attr, "mcall")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->mcall;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

PyMethodDef libftdb_ftdbFuncDerefInfoEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_func_derefinfo_entry_json, METH_VARARGS, "Returns the json representation of the deref entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncDerefInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_derefinfo_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFuncDerefInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_derefinfo_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncDerefInfoEntry_getset[] = {
    {"kind", libftdb_ftdb_func_derefinfo_entry_get_kind, 0, "ftdb func derefinfo entry kind value", 0},
    {"kindname", libftdb_ftdb_func_derefinfo_entry_get_kindname, 0, "ftdb func derefinfo entry kind name value", 0},
    {"offset", libftdb_ftdb_func_derefinfo_entry_get_offset, 0, "ftdb func derefinfo entry offset value", 0},
    {"basecnt", libftdb_ftdb_func_derefinfo_entry_get_basecnt, 0, "ftdb func derefinfo entry basecnt value", 0},
    {"offsetrefs", libftdb_ftdb_func_derefinfo_entry_get_offsetrefs, 0, "ftdb func derefinfo entry offsetrefs values", 0},
    {"expr", libftdb_ftdb_func_derefinfo_entry_get_expr, 0, "ftdb func derefinfo entry expr value", 0},
    {"csid", libftdb_ftdb_func_derefinfo_entry_get_csid, 0, "ftdb func derefinfo entry csid value", 0},
    {"ord", libftdb_ftdb_func_derefinfo_entry_get_ords, 0, "ftdb func derefinfo entry ord values", 0},
    {"ords", libftdb_ftdb_func_derefinfo_entry_get_ords, 0, "ftdb func derefinfo entry ord values", 0},
    {"member", libftdb_ftdb_func_derefinfo_entry_get_member, 0, "ftdb func derefinfo entry member values", 0},
    {"type", libftdb_ftdb_func_derefinfo_entry_get_type, 0, "ftdb func derefinfo entry type values", 0},
    {"access", libftdb_ftdb_func_derefinfo_entry_get_access, 0, "ftdb func derefinfo entry access values", 0},
    {"shift", libftdb_ftdb_func_derefinfo_entry_get_shift, 0, "ftdb func derefinfo entry shift values", 0},
    {"mcall", libftdb_ftdb_func_derefinfo_entry_get_mcall, 0, "ftdb func derefinfo entry mcall values", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncDerefInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncDerefInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_func_derefinfo_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_func_derefinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_derefinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncDerefInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncDerefInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncDerefInfoEntry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFuncDerefInfoEntry_methods,
    .tp_getset = libftdb_ftdbFuncDerefInfoEntry_getset,
    .tp_new = libftdb_ftdb_func_derefinfo_entry_new,
};
