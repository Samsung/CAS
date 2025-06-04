#include "pyftdb.h"

static void libftdb_ftdb_global_entry_dealloc(libftdb_ftdb_global_entry_object *self) {
    Py_DecRef((PyObject *)self->globals);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_global_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_global_entry_object *self;

    self = (libftdb_ftdb_global_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->globals = (const libftdb_ftdb_collection_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->globals);
        unsigned long index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (index >= self->globals->ftdb->globals_count) {
            PyErr_SetString(libftdb_ftdbError, "global entry index out of bounds");
            return 0;
        }
        self->index = index;
        self->entry = &self->globals->ftdb->globals[self->index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_global_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbGlobalEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "id:%lu|name:%s>", __self->entry->id, __self->entry->name);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_global_entry_has_init(libftdb_ftdb_global_entry_object *self, PyObject *args) {
    if (self->entry->hasinit) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_global_entry_get_name(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyUnicode_FromString(__self->entry->name);
}

static PyObject *libftdb_ftdb_global_entry_get_hash(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyUnicode_FromString(__self->entry->hash);
}

static PyObject *libftdb_ftdb_global_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->id);
}

static PyObject *libftdb_ftdb_global_entry_get_fid(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->fid);
}

static PyObject *libftdb_ftdb_global_entry_get_def(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyUnicode_FromString(__self->entry->def);
}

static PyObject *libftdb_ftdb_global_entry_get_type(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->type);
}

static PyObject *libftdb_ftdb_global_entry_get_linkage(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyLong_FromLong((long)__self->entry->linkage);
}

static PyObject *libftdb_ftdb_global_entry_get_linkage_string(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyUnicode_FromString(set_functionLinkage(__self->entry->linkage));
}

static PyObject *libftdb_ftdb_global_entry_get_location(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyUnicode_FromString(__self->entry->location);
}

static PyObject *libftdb_ftdb_global_entry_get_deftype(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyLong_FromLong((long)__self->entry->deftype);
}

static PyObject *libftdb_ftdb_global_entry_get_init(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyUnicode_FromString(__self->entry->init);
}

static PyObject *libftdb_ftdb_global_entry_get_globalrefs(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *globalrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->globalrefs_count; ++i) {
        PYLIST_ADD_ULONG(globalrefs, __self->entry->globalrefs[i]);
    }
    return globalrefs;
}

static PyObject *libftdb_ftdb_global_entry_get_refs(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *refs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refs_count; ++i) {
        PYLIST_ADD_ULONG(refs, __self->entry->refs[i]);
    }
    return refs;
}

static PyObject *libftdb_ftdb_global_entry_get_funrefs(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *funrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->funrefs_count; ++i) {
        PYLIST_ADD_ULONG(funrefs, __self->entry->funrefs[i]);
    }
    return funrefs;
}

static PyObject *libftdb_ftdb_global_entry_get_decls(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *decls = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->decls_count; ++i) {
        PYLIST_ADD_ULONG(decls, __self->entry->decls[i]);
    }
    return decls;
}

static PyObject *libftdb_ftdb_global_entry_get_mids(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    if (!__self->entry->mids) {
        Py_RETURN_NONE;
    }

    PyObject *mids = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->mids_count; ++i) {
        PYLIST_ADD_ULONG(mids, __self->entry->mids[i]);
    }
    return mids;
}

static PyObject *libftdb_ftdb_global_entry_get_literals(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *literals = PyDict_New();
    FTDB_SET_ENTRY_INT64_ARRAY(literals, integer, __self->entry->integer_literals);
    FTDB_SET_ENTRY_UINT_ARRAY(literals, character, __self->entry->character_literals);
    FTDB_SET_ENTRY_FLOAT_ARRAY(literals, floating, __self->entry->floating_literals);
    FTDB_SET_ENTRY_STRING_ARRAY(literals, string, __self->entry->string_literals);

    return literals;
}

static PyObject *libftdb_ftdb_global_entry_get_hasinit(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;
    return PyLong_FromLong(__self->entry->hasinit);
}

static PyObject *libftdb_ftdb_global_entry_get_integer_literals(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *integer_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->integer_literals_count; ++i) {
        PYLIST_ADD_LONG(integer_literals, __self->entry->integer_literals[i]);
    }
    return integer_literals;
}

static PyObject *libftdb_ftdb_global_entry_get_character_literals(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *character_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->character_literals_count; ++i) {
        PYLIST_ADD_LONG(character_literals, __self->entry->character_literals[i]);
    }
    return character_literals;
}

static PyObject *libftdb_ftdb_global_entry_get_floating_literals(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *floating_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->floating_literals_count; ++i) {
        PYLIST_ADD_FLOAT(floating_literals, __self->entry->floating_literals[i]);
    }
    return floating_literals;
}

static PyObject *libftdb_ftdb_global_entry_get_string_literals(PyObject *self, void *closure) {
    libftdb_ftdb_global_entry_object *__self = (libftdb_ftdb_global_entry_object *)self;

    PyObject *string_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->string_literals_count; ++i) {
        PYLIST_ADD_STRING(string_literals, __self->entry->string_literals[i]);
    }
    return string_literals;
}

static PyObject *libftdb_ftdb_global_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_name(self, 0);
    } else if (!strcmp(attr, "hash")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_hash(self, 0);
    } else if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_id(self, 0);
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_fid(self, 0);
    } else if (!strcmp(attr, "def")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_def(self, 0);
    } else if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_type(self, 0);
    } else if (!strcmp(attr, "linkage")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_linkage_string(self, 0);
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_location(self, 0);
    } else if (!strcmp(attr, "deftype")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_deftype(self, 0);
    } else if (!strcmp(attr, "init")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_init(self, 0);
    } else if (!strcmp(attr, "globalrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_globalrefs(self, 0);
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_refs(self, 0);
    } else if (!strcmp(attr, "funrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_funrefs(self, 0);
    } else if (!strcmp(attr, "decls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_decls(self, 0);
    } else if (!strcmp(attr, "mids")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_mids(self, 0);
    } else if (!strcmp(attr, "literals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_literals(self, 0);
    } else if (!strcmp(attr, "hasinit")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_global_entry_get_hasinit(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_global_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "hash")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "def")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "file")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "type")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "linkage")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "deftype")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "init")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "globalrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "funrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "decls")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "mids")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "literals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "hasinit")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    return 0;
}

static PyObject *libftdb_ftdb_global_entry_deepcopy(PyObject *self, PyObject *memo) {
    Py_INCREF(self);
    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_global_entry_json(libftdb_ftdb_global_entry_object *self, PyObject *args) {
    PyObject *json_entry = PyDict_New();

    FTDB_SET_ENTRY_STRING(json_entry, name, self->entry->name);
    FTDB_SET_ENTRY_STRING(json_entry, hash, self->entry->hash);
    FTDB_SET_ENTRY_ULONG(json_entry, id, self->entry->id);
    FTDB_SET_ENTRY_STRING(json_entry, def, self->entry->def);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, globalrefs, self->entry->globalrefs);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, refs, self->entry->refs);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, funrefs, self->entry->funrefs);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, decls, self->entry->decls);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, mids, self->entry->mids);
    FTDB_SET_ENTRY_ULONG(json_entry, fid, self->entry->fid);
    FTDB_SET_ENTRY_ULONG(json_entry, type, self->entry->type);
    FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry, linkage, self->entry->linkage, functionLinkage);
    FTDB_SET_ENTRY_STRING(json_entry, location, self->entry->location);
    FTDB_SET_ENTRY_ULONG(json_entry, deftype, (unsigned long)self->entry->deftype);
    FTDB_SET_ENTRY_INT(json_entry, hasinit, self->entry->hasinit);
    FTDB_SET_ENTRY_STRING(json_entry, init, self->entry->init);

    PyObject *literals = PyDict_New();
    FTDB_SET_ENTRY_INT64_ARRAY(literals, integer, self->entry->integer_literals);
    FTDB_SET_ENTRY_INT_ARRAY(literals, character, self->entry->character_literals);
    FTDB_SET_ENTRY_FLOAT_ARRAY(literals, floating, self->entry->floating_literals);
    FTDB_SET_ENTRY_STRING_ARRAY(literals, string, self->entry->string_literals);
    FTDB_SET_ENTRY_PYOBJECT(json_entry, literals, literals);

    return json_entry;
}

PyMethodDef libftdb_ftdbGlobalEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_global_entry_json, METH_VARARGS, "Returns the json representation of the ftdb global entry"},
    {"has_init", (PyCFunction)libftdb_ftdb_global_entry_has_init, METH_VARARGS, "Returns True if global entry has init field"},
    {"__deepcopy__", (PyCFunction)libftdb_ftdb_global_entry_deepcopy, METH_O, "Deep copy of an object"},
    {NULL, NULL, 0, NULL}
};

PyMappingMethods libftdb_ftdbGlobalEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_global_entry_mp_subscript,
};

PySequenceMethods libftdb_ftdbGlobalEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_global_entry_sq_contains
};

PyGetSetDef libftdb_ftdbGlobalEntry_getset[] = {
    {"name", libftdb_ftdb_global_entry_get_name, 0, "ftdb global entry name value", 0},
    {"hash", libftdb_ftdb_global_entry_get_hash, 0, "ftdb global entry hash value", 0},
    {"id", libftdb_ftdb_global_entry_get_id, 0, "ftdb global entry id value", 0},
    {"fid", libftdb_ftdb_global_entry_get_fid, 0, "ftdb global entry fid value", 0},
    {"defstring", libftdb_ftdb_global_entry_get_def, 0, "ftdb global entry def value", 0},
    {"type", libftdb_ftdb_global_entry_get_type, 0, "ftdb global entry type value", 0},
    {"linkage_string", libftdb_ftdb_global_entry_get_linkage_string, 0, "ftdb global entry linkage string value", 0},
    {"linkage", libftdb_ftdb_global_entry_get_linkage, 0, "ftdb global entry linkage value", 0},
    {"location", libftdb_ftdb_global_entry_get_location, 0, "ftdb global entry location value", 0},
    {"deftype", libftdb_ftdb_global_entry_get_deftype, 0, "ftdb global entry deftype value", 0},
    {"init", libftdb_ftdb_global_entry_get_init, 0, "ftdb global entry init value", 0},
    {"globalrefs", libftdb_ftdb_global_entry_get_globalrefs, 0, "ftdb global entry globalrefs values", 0},
    {"refs", libftdb_ftdb_global_entry_get_refs, 0, "ftdb global entry refs values", 0},
    {"funrefs", libftdb_ftdb_global_entry_get_funrefs, 0, "ftdb global entry funrefs values", 0},
    {"decls", libftdb_ftdb_global_entry_get_decls, 0, "ftdb global entry decls values", 0},
    {"mids", libftdb_ftdb_global_entry_get_mids, 0, "ftdb global entry mids values", 0},
    {"literals", libftdb_ftdb_global_entry_get_literals, 0, "ftdb global entry literals values", 0},
    {"integer_literals", libftdb_ftdb_global_entry_get_integer_literals, 0, "ftdb global entry integer_literals values", 0},
    {"character_literals", libftdb_ftdb_global_entry_get_character_literals, 0, "ftdb global entry character_literals values", 0},
    {"floating_literals", libftdb_ftdb_global_entry_get_floating_literals, 0, "ftdb global entry floating_literals values", 0},
    {"string_literals", libftdb_ftdb_global_entry_get_string_literals, 0, "ftdb global entry string_literals values", 0},
    {"hasinit", libftdb_ftdb_global_entry_get_hasinit, 0, "ftdb global entry hasinit value", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbGlobalEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbGlobalEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_global_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_global_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_global_entry_repr,
    .tp_as_sequence = &libftdb_ftdbGlobalEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbGlobalEntry_mapping_methods,
    .tp_doc = "libftdb ftdb global entry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbGlobalEntry_methods,
    .tp_getset = libftdb_ftdbGlobalEntry_getset,
    .tp_new = libftdb_ftdb_global_entry_new,
};
