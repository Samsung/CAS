#include "pyftdb.h"

static void libftdb_ftdb_fops_entry_dealloc(libftdb_ftdb_fops_entry_object *self) {
    Py_DecRef((PyObject *)self->fops);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_fops_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_fops_entry_object *self;

    self = (libftdb_ftdb_fops_entry_object *) subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->fops = (libftdb_ftdb_collection_object *) PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *) self->fops);
        unsigned long index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (index >= self->fops->ftdb->fops_count) {
            PyErr_SetString(libftdb_ftdbError, "fops entry index out of bounds");
            return 0;
        }
        self->index = index;
        self->entry = &self->fops->ftdb->fops[self->index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_fops_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFopsEntry object at %lx : ", (uintptr_t)__self);
    (void)written;

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_fops_entry_json(libftdb_ftdb_fops_entry_object *self, PyObject *args) {
    PyObject *json_entry = PyDict_New();

    FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry, kind, self->entry->kind, fopsKind);
    FTDB_SET_ENTRY_ULONG(json_entry, type, self->entry->type_id);
    FTDB_SET_ENTRY_ULONG(json_entry, var, self->entry->var_id);
    FTDB_SET_ENTRY_ULONG(json_entry, func, self->entry->func_id);
    FTDB_SET_ENTRY_STRING(json_entry, loc, self->entry->location);
    PyObject *members = PyDict_New();
    for (unsigned long i = 0; i < self->entry->members_count; ++i) {
        struct ftdb_fops_member_entry *fops_member_info = &self->entry->members[i];
        PyObject *py_member = PyUnicode_FromFormat("%lu", fops_member_info->member_id);
        PyObject *py_func_ids = PyList_New(0);
        for (unsigned long j = 0; j < fops_member_info->func_ids_count; j++) {
            PYLIST_ADD_ULONG(py_func_ids, fops_member_info->func_ids[j]);
        }
        PyDict_SetItem(members, py_member, py_func_ids);
        Py_DecRef(py_member);
        Py_DecRef(py_func_ids);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, members, members);

    return json_entry;
}

static PyObject *libftdb_ftdb_fops_entry_get_kind(PyObject *self, void *closure) {
    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;
    return PyUnicode_FromString(set_fopsKind(__self->entry->kind));
}

static PyObject *libftdb_ftdb_fops_entry_get_type(PyObject *self, void *closure) {
    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->type_id);
}

static PyObject *libftdb_ftdb_fops_entry_get_var(PyObject *self, void *closure) {
    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->var_id);
}

static PyObject *libftdb_ftdb_fops_entry_get_func(PyObject *self, void *closure) {
    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->func_id);
}

static PyObject *libftdb_ftdb_fops_entry_get_members(PyObject *self, void *closure) {
    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;

    PyObject *members = PyDict_New();
    for (unsigned long i = 0; i < __self->entry->members_count; ++i) {
        struct ftdb_fops_member_entry *fops_member_info = &__self->entry->members[i];
        PyObject *py_member = PyUnicode_FromFormat("%lu", fops_member_info->member_id);
        PyObject *py_func_ids = PyList_New(0);
        for (unsigned long j = 0; j < fops_member_info->func_ids_count; j++) {
            PyObject *py_fnid = PyLong_FromUnsignedLong(fops_member_info->func_ids[j]);
            PyList_Append(py_func_ids, py_fnid);
            Py_DecRef(py_fnid);
        }
        PyDict_SetItem(members, py_member, py_func_ids);
        Py_DecRef(py_member);
    }
    return members;
}

static PyObject *libftdb_ftdb_fops_entry_get_location(PyObject *self, void *closure) {
    libftdb_ftdb_fops_entry_object *__self = (libftdb_ftdb_fops_entry_object *)self;
    return PyUnicode_FromString(__self->entry->location);
}

static PyObject *libftdb_ftdb_fops_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "kind")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_fops_entry_get_kind(self, 0);
    }
    if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_fops_entry_get_type(self, 0);
    }
    if (!strcmp(attr, "var")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_fops_entry_get_var(self, 0);
    }
    if (!strcmp(attr, "func")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_fops_entry_get_func(self, 0);
    } else if (!strcmp(attr, "members")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_fops_entry_get_members(self, 0);
    } else if (!strcmp(attr, "loc")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_fops_entry_get_location(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_fops_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "var")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "func")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "kind")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "members")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "loc")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static PyObject *libftdb_ftdb_fops_entry_deepcopy(PyObject *self, PyObject *memo) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyMethodDef libftdb_ftdbFopsEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_fops_entry_json, METH_VARARGS, "Returns the json representation of the ftdb fops entry"},
    {"__deepcopy__", (PyCFunction)libftdb_ftdb_fops_entry_deepcopy, METH_O, "Deep copy of an object"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFopsEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_fops_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFopsEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_fops_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFopsEntry_getset[] = {
    {"kind", libftdb_ftdb_fops_entry_get_kind, 0, "ftdb fops entry kind value", 0},
    {"type", libftdb_ftdb_fops_entry_get_type, 0, "ftdb fops entry type value", 0},
    {"var", libftdb_ftdb_fops_entry_get_var, 0, "ftdb fops entry var value", 0},
    {"func", libftdb_ftdb_fops_entry_get_func, 0, "ftdb fops entry func value", 0},
    {"members", libftdb_ftdb_fops_entry_get_members, 0, "ftdb fops entry members values", 0},
    {"loc", libftdb_ftdb_fops_entry_get_location, 0, "ftdb fops entry location value", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFopsEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFopsEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_fops_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_fops_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_fops_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFopsEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFopsEntry_mapping_methods,
    .tp_doc = "libftdb ftdb fops entry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFopsEntry_methods,
    .tp_getset = libftdb_ftdbFopsEntry_getset,
    .tp_new = libftdb_ftdb_fops_entry_new,
};
