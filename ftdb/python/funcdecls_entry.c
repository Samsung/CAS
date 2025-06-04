#include "pyftdb.h"

static void libftdb_ftdb_funcdecl_entry_dealloc(libftdb_ftdb_funcdecl_entry_object *self) {
    Py_DecRef((PyObject *)self->funcdecls);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_funcdecl_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_funcdecl_entry_object *self;

    self = (libftdb_ftdb_funcdecl_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->funcdecls = (const libftdb_ftdb_collection_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->funcdecls);
        unsigned long index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (index >= self->funcdecls->ftdb->funcdecls_count) {
            PyErr_SetString(libftdb_ftdbError, "funcdecl entry index out of bounds");
            return 0;
        }
        self->index = index;
        self->entry = &self->funcdecls->ftdb->funcdecls[self->index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_funcdecl_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncdeclEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "id:%lu|name:%s>", __self->entry->id, __self->entry->name);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_funcdecl_entry_json(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args) {
    PyObject *json_entry = PyDict_New();

    FTDB_SET_ENTRY_STRING(json_entry, name, self->entry->name);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, namespace, self->entry->__namespace);
    FTDB_SET_ENTRY_ULONG(json_entry, id, self->entry->id);
    FTDB_SET_ENTRY_ULONG(json_entry, fid, self->entry->fid);
    FTDB_SET_ENTRY_ULONG(json_entry, nargs, self->entry->nargs);
    FTDB_SET_ENTRY_BOOL(json_entry, variadic, self->entry->isvariadic);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, template, self->entry->istemplate);
    FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry, linkage, self->entry->linkage, functionLinkage);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, member, self->entry->ismember);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, class, self->entry->__class);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry, classid, self->entry->classid);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, template_parameters, self->entry->template_parameters);
    FTDB_SET_ENTRY_STRING(json_entry, decl, self->entry->decl);
    FTDB_SET_ENTRY_STRING(json_entry, signature, self->entry->signature);
    FTDB_SET_ENTRY_STRING(json_entry, declhash, self->entry->declhash);
    FTDB_SET_ENTRY_STRING(json_entry, location, self->entry->location);
    FTDB_SET_ENTRY_ULONG(json_entry, refcount, self->entry->refcount);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, types, self->entry->types);

    return json_entry;
}

static PyObject *libftdb_ftdb_funcdecl_entry_has_namespace(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args) {
    if (self->entry->__namespace) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_funcdecl_entry_has_class(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args) {
    if ((self->entry->__class) || (self->entry->classid)) {
        if (!self->entry->__class) {
            PyErr_SetString(libftdb_ftdbError, "No 'class' field in function decl entry");
            return 0;
        }
        if (!self->entry->classid) {
            PyErr_SetString(libftdb_ftdbError, "No 'classid' field in function decl entry");
            return 0;
        }
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->id);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_name(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyUnicode_FromString(__self->entry->name);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_namespace(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if (!__self->entry->__namespace) {
        PyErr_SetString(libftdb_ftdbError, "No 'namespace' field in function decl entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->__namespace);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_fid(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->fid);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_nargs(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->nargs);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_variadic(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if (__self->entry->isvariadic) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_template(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if ((__self->entry->istemplate) && (*__self->entry->istemplate)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_linkage(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyLong_FromLong((long)__self->entry->linkage);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_linkage_string(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyUnicode_FromString(set_functionLinkage(__self->entry->linkage));
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_member(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if ((__self->entry->ismember) && (*__self->entry->ismember)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_class(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if (!__self->entry->__class) {
        PyErr_SetString(libftdb_ftdbError, "No 'class' field in function decl entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->__class);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_classid(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if (!__self->entry->classid) {
        PyErr_SetString(libftdb_ftdbError, "No 'classid' field in function decl entry");
        return 0;
    }

    return PyLong_FromUnsignedLong(*__self->entry->classid);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_template_parameters(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    if (!__self->entry->template_parameters) {
        PyErr_SetString(libftdb_ftdbError, "No 'template_parameters' field in function decl entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->template_parameters);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_decl(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyUnicode_FromString(__self->entry->decl);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_signature(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyUnicode_FromString(__self->entry->signature);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_declhash(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyUnicode_FromString(__self->entry->declhash);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_location(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyUnicode_FromString(__self->entry->location);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_refcount(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->refcount);
}

static PyObject *libftdb_ftdb_funcdecl_entry_get_types(PyObject *self, void *closure) {
    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;

    PyObject *types = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->types_count; ++i) {
        PYLIST_ADD_ULONG(types, __self->entry->types[i]);
    }
    return types;
}

static PyObject *libftdb_ftdb_funcdecl_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_id(self, 0);
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_name(self, 0);
    } else if (!strcmp(attr, "namespace")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_namespace(self, 0);
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_fid(self, 0);
    } else if (!strcmp(attr, "nargs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_nargs(self, 0);
    } else if (!strcmp(attr, "variadic")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_variadic(self, 0);
    } else if (!strcmp(attr, "template")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_template(self, 0);
    } else if (!strcmp(attr, "linkage")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_linkage_string(self, 0);
    } else if (!strcmp(attr, "member")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_member(self, 0);
    } else if (!strcmp(attr, "class")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_class(self, 0);
    } else if (!strcmp(attr, "classid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_classid(self, 0);
    } else if (!strcmp(attr, "template_parameters")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_template_parameters(self, 0);
    } else if (!strcmp(attr, "decl")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_decl(self, 0);
    } else if (!strcmp(attr, "signature")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_signature(self, 0);
    } else if (!strcmp(attr, "declhash")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_declhash(self, 0);
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_location(self, 0);
    } else if (!strcmp(attr, "refcount")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_refcount(self, 0);
    } else if (!strcmp(attr, "types")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_funcdecl_entry_get_types(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_funcdecl_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_funcdecl_entry_object *__self = (libftdb_ftdb_funcdecl_entry_object *)self;
    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "namespace")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->__namespace;
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "nargs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "variadic")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "template")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->istemplate;
    } else if (!strcmp(attr, "linkage")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "member")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->ismember;
    } else if (!strcmp(attr, "class")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->__class;
    } else if (!strcmp(attr, "classid")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->classid;
    } else if (!strcmp(attr, "template_parameters")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->template_parameters;
    } else if (!strcmp(attr, "decl")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "signature")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "declhash")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refcount")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "types")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    return 0;
}

static PyObject *libftdb_ftdb_funcdecl_entry_deepcopy(PyObject *self, PyObject *memo) {
    Py_INCREF(self);
    return (PyObject *)self;
}

PyMethodDef libftdb_ftdbFuncdeclEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_funcdecl_entry_json, METH_VARARGS, "Returns the json representation of the ftdb funcdecl entry"},
    {"has_namespace", (PyCFunction)libftdb_ftdb_funcdecl_entry_has_namespace, METH_VARARGS,
     "Returns True if funcdecl entry contains namespace information"},
    {"has_class", (PyCFunction)libftdb_ftdb_funcdecl_entry_has_class, METH_VARARGS,
     "Returns True if funcdecl entry is contained within a class"},
    {"__deepcopy__", (PyCFunction)libftdb_ftdb_funcdecl_entry_deepcopy, METH_O, "Deep copy of an object"},
    {NULL, NULL, 0, NULL}
};

PyMappingMethods libftdb_ftdbFuncdeclEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_funcdecl_entry_mp_subscript,
};

PySequenceMethods libftdb_ftdbFuncdeclEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_funcdecl_entry_sq_contains
};

PyGetSetDef libftdb_ftdbFuncdeclEntry_getset[] = {
    {"id", libftdb_ftdb_funcdecl_entry_get_id, 0, "ftdb funcdecl entry id value", 0},
    {"name", libftdb_ftdb_funcdecl_entry_get_name, 0, "ftdb funcdecl entry name value", 0},
    {"namespace", libftdb_ftdb_funcdecl_entry_get_namespace, 0, "ftdb funcdecl entry namespace value", 0},
    {"fid", libftdb_ftdb_funcdecl_entry_get_fid, 0, "ftdb funcdecl entry fid value", 0},
    {"nargs", libftdb_ftdb_funcdecl_entry_get_nargs, 0, "ftdb funcdecl entry nargs value", 0},
    {"variadic", libftdb_ftdb_funcdecl_entry_get_variadic, 0, "ftdb funcdecl entry is variadic value", 0},
    {"template", libftdb_ftdb_funcdecl_entry_get_template, 0, "ftdb funcdecl entry is template value", 0},
    {"linkage", libftdb_ftdb_funcdecl_entry_get_linkage, 0, "ftdb funcdecl entry linkage value", 0},
    {"linkage_string", libftdb_ftdb_funcdecl_entry_get_linkage_string, 0, "ftdb funcdecl entry linkage value", 0},
    {"member", libftdb_ftdb_funcdecl_entry_get_member, 0, "ftdb funcdecl entry is member value", 0},
    {"class", libftdb_ftdb_funcdecl_entry_get_class, 0, "ftdb funcdecl entry class value", 0},
    {"classid", libftdb_ftdb_funcdecl_entry_get_classid, 0, "ftdb funcdecl entry classid value", 0},
    {"template_parameters", libftdb_ftdb_funcdecl_entry_get_template_parameters, 0,
     "ftdb funcdecl entry template_parameters value", 0},
    {"decl", libftdb_ftdb_funcdecl_entry_get_decl, 0, "ftdb funcdecl entry decl value", 0},
    {"signature", libftdb_ftdb_funcdecl_entry_get_signature, 0, "ftdb funcdecl entry signature value", 0},
    {"declhash", libftdb_ftdb_funcdecl_entry_get_declhash, 0, "ftdb funcdecl entry declhash value", 0},
    {"location", libftdb_ftdb_funcdecl_entry_get_location, 0, "ftdb funcdecl entry location value", 0},
    {"refcount", libftdb_ftdb_funcdecl_entry_get_refcount, 0, "ftdb funcdecl entry refcount value", 0},
    {"types", libftdb_ftdb_funcdecl_entry_get_types, 0, "ftdb funcdecl entry types list", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncdeclsEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncdeclEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_funcdecl_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_funcdecl_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_funcdecl_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncdeclEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncdeclEntry_mapping_methods,
    .tp_doc = "libftdb ftdb funcdecl entry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFuncdeclEntry_methods,
    .tp_getset = libftdb_ftdbFuncdeclEntry_getset,
    .tp_new = libftdb_ftdb_funcdecl_entry_new,
};
