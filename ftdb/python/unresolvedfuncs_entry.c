#include "pyftdb.h"

static void libftdb_ftdb_unresolvedfunc_entry_dealloc(libftdb_ftdb_unresolvedfunc_entry_object *self) {
    Py_DecRef((PyObject *)self->unresolvedfuncs);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_unresolvedfunc_entry_object *self;

    self = (libftdb_ftdb_unresolvedfunc_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->unresolvedfuncs = (const libftdb_ftdb_collection_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->unresolvedfuncs);
        unsigned long index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (index >= self->unresolvedfuncs->ftdb->unresolvedfuncs_count) {
            PyErr_SetString(libftdb_ftdbError, "unresolvedfunc entry index out of bounds");
            return 0;
        }
        self->index = index;
        self->entry = &self->unresolvedfuncs->ftdb->unresolvedfuncs[self->index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_unresolvedfunc_entry_object *__self = (libftdb_ftdb_unresolvedfunc_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbUnresolvedfuncEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "id:%lu|name:%s>", __self->entry->id, __self->entry->name);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_unresolvedfunc_entry_object *__self = (libftdb_ftdb_unresolvedfunc_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->id);
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_get_name(PyObject *self, void *closure) {
    libftdb_ftdb_unresolvedfunc_entry_object *__self = (libftdb_ftdb_unresolvedfunc_entry_object *)self;
    return PyUnicode_FromString(__self->entry->name);
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    // tuple api compatibility
    if (PyLong_Check(slice)) {
        unsigned long id = PyLong_AsLong(slice);
        if(id == 0){
            return libftdb_ftdb_unresolvedfunc_entry_get_id(self, 0);
        } else if(id == 1){
            return libftdb_ftdb_unresolvedfunc_entry_get_name(self, 0);
        }
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Tuple index out of range: %lu", id);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        Py_RETURN_NONE;
    }

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_unresolvedfunc_entry_get_id(self, 0);
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_unresolvedfunc_entry_get_name(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_unresolvedfunc_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);
    libftdb_ftdb_unresolvedfunc_entry_object *__self = (libftdb_ftdb_unresolvedfunc_entry_object *)self;

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static Py_hash_t libftdb_ftdb_unresolvedfunc_entry_hash(PyObject *o) {
    libftdb_ftdb_unresolvedfunc_entry_object *__self = (libftdb_ftdb_unresolvedfunc_entry_object *)o;
    return __self->index;
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_richcompare(PyObject *self, PyObject *other, int op) {
    if (!PyObject_IsInstance(other, (PyObject *)&libftdb_ftdbUnresolvedfuncEntryType))
        Py_RETURN_FALSE;

    libftdb_ftdb_unresolvedfunc_entry_object *__self = (libftdb_ftdb_unresolvedfunc_entry_object *)self;
    libftdb_ftdb_unresolvedfunc_entry_object *__other = (libftdb_ftdb_unresolvedfunc_entry_object *)other;
    Py_RETURN_RICHCOMPARE_internal(__self->index, __other->index, op);
}

static PyObject *libftdb_ftdb_unresolvedfunc_entry_json(libftdb_ftdb_unresolvedfunc_entry_object *self, PyObject *args) {
    PyObject *json_entry = PyDict_New();

    FTDB_SET_ENTRY_ULONG(json_entry, id, self->entry->id);
    FTDB_SET_ENTRY_STRING(json_entry, name, self->entry->name);

    return json_entry;
}



PyMethodDef libftdb_ftdbUnresolvedfuncEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_unresolvedfunc_entry_json, METH_VARARGS,
     "Returns the json representation of the ftdb unresolvedfunc entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbUnresolvedfuncEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_unresolvedfunc_entry_sq_contains,
};

PyMappingMethods libftdb_ftdbUnresolvedfuncEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_unresolvedfunc_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbUnresolvedfuncEntry_getset[] = {
    {"id", libftdb_ftdb_unresolvedfunc_entry_get_id, 0, "ftdb unresolvedfunc entry id value", 0},
    {"name", libftdb_ftdb_unresolvedfunc_entry_get_name, 0, "ftdb unresolvedfunc entry name value", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbUnresolvedfuncEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbUnresolvedfuncEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_unresolvedfunc_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_unresolvedfunc_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_unresolvedfunc_entry_repr,
    .tp_as_sequence = &libftdb_ftdbUnresolvedfuncEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbUnresolvedfuncEntry_mapping_methods,
    .tp_hash = (hashfunc)libftdb_ftdb_unresolvedfunc_entry_hash,
    .tp_doc = "libftdb ftdb unresolvedfunc entry object",
    .tp_richcompare = libftdb_ftdb_unresolvedfunc_entry_richcompare,
    .tp_methods = libftdb_ftdbUnresolvedfuncEntry_methods,
    .tp_getset = libftdb_ftdbUnresolvedfuncEntry_getset,
    .tp_new = libftdb_ftdb_unresolvedfunc_entry_new,
};
