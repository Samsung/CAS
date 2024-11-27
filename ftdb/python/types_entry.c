#include "pyftdb.h"

static void libftdb_ftdb_type_entry_dealloc(libftdb_ftdb_type_entry_object *self) {
    Py_DecRef((PyObject *)self->types);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_type_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_type_entry_object *self;

    self = (libftdb_ftdb_type_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->types = (const libftdb_ftdb_types_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->types);
        unsigned long index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (index >= self->types->ftdb->types_count) {
            PyErr_SetString(libftdb_ftdbError, "type entry index out of bounds");
            return 0;
        }
        self->index = index;
        self->entry = &self->types->ftdb->types[self->index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_type_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbTypeEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "id:%lu|class:%s,str:%s>", __self->entry->id, __self->entry->class_name, __self->entry->str);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_type_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->id);
}

static PyObject *libftdb_ftdb_type_entry_get_fid(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->fid);
}

static PyObject *libftdb_ftdb_type_entry_get_hash(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyUnicode_FromString(__self->entry->hash);
}

static PyObject *libftdb_ftdb_type_entry_get_classname(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyUnicode_FromString(__self->entry->class_name);
}

static PyObject *libftdb_ftdb_type_entry_get_classid(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyLong_FromLong((long)__self->entry->__class);
}

static PyObject *libftdb_ftdb_type_entry_get_qualifiers(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyUnicode_FromString(__self->entry->qualifiers);
}

static PyObject *libftdb_ftdb_type_entry_get_size(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->size);
}

static PyObject *libftdb_ftdb_type_entry_get_str(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyUnicode_FromString(__self->entry->str);
}

static PyObject *libftdb_ftdb_type_entry_get_refcount(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    return PyLong_FromUnsignedLong(__self->entry->refcount);
}

static PyObject *libftdb_ftdb_type_entry_get_refs(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    PyObject *refs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refs_count; ++i) {
        PYLIST_ADD_ULONG(refs, __self->entry->refs[i]);
    }
    return refs;
}

static PyObject *libftdb_ftdb_type_entry_get_usedrefs(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->usedrefs) {
        PyErr_SetString(libftdb_ftdbError, "No 'usedrefs' field in type entry");
        return 0;
    }

    PyObject *usedrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->usedrefs_count; ++i) {
        PYLIST_ADD_LONG(usedrefs, __self->entry->usedrefs[i]);
    }
    return usedrefs;
}

static PyObject *libftdb_ftdb_type_entry_get_decls(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->decls) {
        PyErr_SetString(libftdb_ftdbError, "No 'decls' field in type entry");
        return 0;
    }

    PyObject *decls = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->decls_count; ++i) {
        PYLIST_ADD_ULONG(decls, __self->entry->decls[i]);
    }
    return decls;
}

static PyObject *libftdb_ftdb_type_entry_get_def(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->def) {
        PyErr_SetString(libftdb_ftdbError, "No 'def' field in type entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->def);
}

static PyObject *libftdb_ftdb_type_entry_get_memberoffsets(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->memberoffsets) {
        PyErr_SetString(libftdb_ftdbError, "No 'memberoffsets' field in type entry");
        return 0;
    }

    PyObject *memberoffsets = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->memberoffsets_count; ++i) {
        PYLIST_ADD_ULONG(memberoffsets, __self->entry->memberoffsets[i]);
    }
    return memberoffsets;
}

static PyObject *libftdb_ftdb_type_entry_get_attrrefs(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->attrrefs) {
        PyErr_SetString(libftdb_ftdbError, "No 'attrrefs' field in type entry");
        return 0;
    }

    PyObject *attrrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->attrrefs_count; ++i) {
        PYLIST_ADD_ULONG(attrrefs, __self->entry->attrrefs[i]);
    }
    return attrrefs;
}

static PyObject *libftdb_ftdb_type_entry_get_attrnum(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->attrnum) {
        return PyLong_FromUnsignedLong(0);
    }

    return PyLong_FromUnsignedLong(*__self->entry->attrnum);
}

static PyObject *libftdb_ftdb_type_entry_get_name(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->name) {
        PyErr_SetString(libftdb_ftdbError, "No 'name' field in type entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->name);
}

static PyObject *libftdb_ftdb_type_entry_get_is_dependent(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if ((__self->entry->isdependent) && (*__self->entry->isdependent)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_get_globalrefs(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->globalrefs) {
        PyErr_SetString(libftdb_ftdbError, "No 'globalrefs' field in type entry");
        return 0;
    }

    PyObject *globalrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->globalrefs_count; ++i) {
        PYLIST_ADD_ULONG(globalrefs, __self->entry->globalrefs[i]);
    }
    return globalrefs;
}

static PyObject *libftdb_ftdb_type_entry_get_enumvalues(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->enumvalues) {
        PyErr_SetString(libftdb_ftdbError, "No 'enumvalues' field in type entry");
        return 0;
    }

    PyObject *enumvalues = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->enumvalues_count; ++i) {
        PYLIST_ADD_LONG(enumvalues, __self->entry->enumvalues[i]);
    }
    return enumvalues;
}

static PyObject *libftdb_ftdb_type_entry_get_outerfn(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->outerfn) {
        PyErr_SetString(libftdb_ftdbError, "No 'outerfn' field in type entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->outerfn);
}

static PyObject *libftdb_ftdb_type_entry_get_outerfnid(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->outerfnid) {
        PyErr_SetString(libftdb_ftdbError, "No 'outerfnid' field in type entry");
        return 0;
    }

    return PyLong_FromUnsignedLong(*__self->entry->outerfnid);
}

static PyObject *libftdb_ftdb_type_entry_get_is_implicit(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if ((__self->entry->isimplicit) && (*__self->entry->isimplicit)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_get_is_union(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if ((__self->entry->isunion) && (*__self->entry->isunion)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_get_refnames(PyObject *self, void *closure) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->refnames) {
        snprintf(errmsg, ERRMSG_BUFFER_SIZE, "No 'refnames' field in type entry ([%lu])\n", __self->entry->refnames_count);
        PyErr_SetString(libftdb_ftdbError, errmsg);
        return 0;
    }

    PyObject *refnames = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refnames_count; ++i) {
        PYLIST_ADD_STRING(refnames, __self->entry->refnames[i]);
    }
    return refnames;
}

static PyObject *libftdb_ftdb_type_entry_get_location(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->location) {
        PyErr_SetString(libftdb_ftdbError, "No 'location' field in type entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->location);
}

static PyObject *libftdb_ftdb_type_entry_get_is_variadic(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if ((__self->entry->isvariadic) && (*__self->entry->isvariadic)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_get_funrefs(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->funrefs) {
        PyErr_SetString(libftdb_ftdbError, "No 'funrefs' field in type entry");
        return 0;
    }

    PyObject *funrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->funrefs_count; ++i) {
        PYLIST_ADD_ULONG(funrefs, __self->entry->funrefs[i]);
    }
    return funrefs;
}

static PyObject *libftdb_ftdb_type_entry_get_useddef(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->useddef) {
        PyErr_SetString(libftdb_ftdbError, "No 'useddef' field in type entry");
        return 0;
    }

    PyObject *useddef = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->useddef_count; ++i) {
        PYLIST_ADD_STRING(useddef, __self->entry->useddef[i]);
    }
    return useddef;
}

static PyObject *libftdb_ftdb_type_entry_get_defhead(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->defhead) {
        PyErr_SetString(libftdb_ftdbError, "No 'defhead' field in type entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->defhead);
}

static PyObject *libftdb_ftdb_type_entry_get_identifiers(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->identifiers) {
        PyErr_SetString(libftdb_ftdbError, "No 'identifiers' field in type entry");
        return 0;
    }

    PyObject *identifiers = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->identifiers_count; ++i) {
        PYLIST_ADD_STRING(identifiers, __self->entry->identifiers[i]);
    }
    return identifiers;
}

static PyObject *libftdb_ftdb_type_entry_get_methods(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->methods) {
        PyErr_SetString(libftdb_ftdbError, "No 'methods' field in type entry");
        return 0;
    }

    PyObject *methods = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->methods_count; ++i) {
        PYLIST_ADD_ULONG(methods, __self->entry->methods[i]);
    }
    return methods;
}

static PyObject *libftdb_ftdb_type_entry_get_bitfields(PyObject *self, void *closure) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!__self->entry->bitfields) {
        PyErr_SetString(libftdb_ftdbError, "No 'bitfields' field in type entry");
        return 0;
    }

    PyObject *bitfields = PyDict_New();
    for (unsigned long i = 0; i < __self->entry->bitfields_count; ++i) {
        struct bitfield *bitfield = &__self->entry->bitfields[i];
        PyObject *py_bkey = PyUnicode_FromFormat("%lu", bitfield->index);
        PyObject *py_bval = PyLong_FromUnsignedLong(bitfield->bitwidth);
        PyDict_SetItem(bitfields, py_bkey, py_bval);
        Py_DecRef(py_bkey);
        Py_DecRef(py_bval);
    }
    return bitfields;
}

static PyObject *libftdb_ftdb_type_entry_has_usedrefs(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if (self->entry->usedrefs) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_has_decls(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if (self->entry->decls) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_has_attrs(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if ((self->entry->attrnum) && (*self->entry->attrnum > 0)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_has_outerfn(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if (self->entry->outerfn) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_type_entry_has_location(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if (self->entry->location) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static int libftdb_ftdb_type_entry_is_const_internal_type(struct ftdb_type_entry *entry) {
    const char *c = entry->qualifiers;
    while (*c) {
        if (*c++ == 'c')
            return 1;
    }
    return 0;
}

static int libftdb_ftdb_type_entry_is_const_internal(libftdb_ftdb_type_entry_object *self) {
    const char *c = self->entry->qualifiers;
    while (*c) {
        if (*c++ == 'c')
            return 1;
    }
    return 0;
}

static PyObject *libftdb_ftdb_type_entry_is_const(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if (libftdb_ftdb_type_entry_is_const_internal(self)) {
        Py_RETURN_TRUE;
    }
    Py_RETURN_FALSE;
}

static inline const char *__real_hash(const char *hash) {
    int coln = 0;
    while (*hash) {
        if (*hash++ == ':')
            coln++;
        if (coln >= 2)
            break;
    }
    return hash;
}

static PyObject *libftdb_ftdb_type_entry_to_non_const(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    if (!libftdb_ftdb_type_entry_is_const_internal(self)) {
        Py_IncRef((PyObject *)self);
        return (PyObject *)self;
    }

    for (unsigned long i = 0; i < self->types->ftdb->types_count; ++i) {
        struct ftdb_type_entry *entry = &self->types->ftdb->types[i];
        if (strcmp(self->entry->str, entry->str))
            continue;
        if (entry->__class == TYPECLASS_RECORDFORWARD)
            continue;
        if (strcmp(__real_hash(self->entry->hash), __real_hash(entry->hash)))
            continue;
        if (libftdb_ftdb_type_entry_is_const_internal_type(entry))
            continue;
        PyObject *args = PyTuple_New(2);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)self->types);
        PYTUPLE_SET_ULONG(args, 1, entry->__index);
        PyObject *py_entry = PyObject_CallObject((PyObject *)&libftdb_ftdbTypeEntryType, args);
        Py_DecRef(args);
        return py_entry;
    }

    Py_IncRef((PyObject *)self);
    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_type_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_id(self, 0);
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_fid(self, 0);
    } else if (!strcmp(attr, "hash")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_hash(self, 0);
    } else if (!strcmp(attr, "class")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_classname(self, 0);
    } else if (!strcmp(attr, "classid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_classid(self, 0);
    } else if (!strcmp(attr, "qualifiers")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_qualifiers(self, 0);
    } else if (!strcmp(attr, "size")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_size(self, 0);
    } else if (!strcmp(attr, "str")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_str(self, 0);
    } else if (!strcmp(attr, "refcount")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_refcount(self, 0);
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_refs(self, 0);
    } else if (!strcmp(attr, "usedrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_usedrefs(self, 0);
    } else if (!strcmp(attr, "decls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_decls(self, 0);
    } else if (!strcmp(attr, "def")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_def(self, 0);
    } else if (!strcmp(attr, "memberoffsets")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_memberoffsets(self, 0);
    } else if (!strcmp(attr, "attrrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_attrrefs(self, 0);
    } else if (!strcmp(attr, "attrnum")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_attrnum(self, 0);
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_name(self, 0);
    } else if (!strcmp(attr, "dependent")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_is_dependent(self, 0);
    } else if (!strcmp(attr, "globalrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_globalrefs(self, 0);
    } else if (!strcmp(attr, "values")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_enumvalues(self, 0);
    } else if (!strcmp(attr, "outerfn")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_outerfn(self, 0);
    } else if (!strcmp(attr, "outerfnid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_outerfnid(self, 0);
    } else if (!strcmp(attr, "implicit")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_is_implicit(self, 0);
    } else if (!strcmp(attr, "union")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_is_union(self, 0);
    } else if (!strcmp(attr, "refnames")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_refnames(self, 0);
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_location(self, 0);
    } else if (!strcmp(attr, "variadic")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_is_variadic(self, 0);
    } else if (!strcmp(attr, "funrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_funrefs(self, 0);
    } else if (!strcmp(attr, "useddef")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_useddef(self, 0);
    } else if (!strcmp(attr, "defhead")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_defhead(self, 0);
    } else if (!strcmp(attr, "identifiers")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_identifiers(self, 0);
    } else if (!strcmp(attr, "methods")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_methods(self, 0);
    } else if (!strcmp(attr, "bitfields")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_type_entry_get_bitfields(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_type_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "hash")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "class")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "classid")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "qualifiers")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "size")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "str")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refcount")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "usedrefs")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->usedrefs;
    } else if (!strcmp(attr, "decls")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->decls;
    } else if (!strcmp(attr, "def")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->def;
    } else if (!strcmp(attr, "memberoffsets")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->memberoffsets;
    } else if (!strcmp(attr, "attrrefs")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->attrrefs;
    } else if (!strcmp(attr, "attrnum")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->attrnum;
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->name;
    } else if (!strcmp(attr, "dependent")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->isdependent;
    } else if (!strcmp(attr, "globalrefs")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->globalrefs;
    } else if (!strcmp(attr, "values")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->enumvalues;
    } else if (!strcmp(attr, "outerfn")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->outerfn;
    } else if (!strcmp(attr, "outerfnid")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->outerfnid;
    } else if (!strcmp(attr, "implicit")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->isimplicit;
    } else if (!strcmp(attr, "union")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->isunion;
    } else if (!strcmp(attr, "refnames")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->refnames;
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->location;
    } else if (!strcmp(attr, "variadic")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->isvariadic;
    } else if (!strcmp(attr, "funrefs")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->funrefs;
    } else if (!strcmp(attr, "useddef")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->useddef;
    } else if (!strcmp(attr, "defhead")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->defhead;
    } else if (!strcmp(attr, "identifiers")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->identifiers;
    } else if (!strcmp(attr, "methods")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->methods;
    } else if (!strcmp(attr, "bitfields")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->bitfields;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static PyObject *libftdb_ftdb_type_entry_deepcopy(PyObject *self, PyObject *memo) {
    Py_INCREF(self);
    return (PyObject *)self;
}

static Py_hash_t libftdb_ftdb_type_entry_hash(PyObject *o) {
    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)o;
    return __self->index;
}

static PyObject *libftdb_ftdb_type_entry_richcompare(PyObject *self, PyObject *other, int op) {
    if (!PyObject_IsInstance(other, (PyObject *)&libftdb_ftdbTypeEntryType))
        Py_RETURN_FALSE;

    libftdb_ftdb_type_entry_object *__self = (libftdb_ftdb_type_entry_object *)self;
    libftdb_ftdb_type_entry_object *__other = (libftdb_ftdb_type_entry_object *)other;
    Py_RETURN_RICHCOMPARE_internal(__self->index, __other->index, op);
}

static PyObject *libftdb_ftdb_type_entry_json(libftdb_ftdb_type_entry_object *self, PyObject *args) {
    PyObject *json_entry = PyDict_New();

    FTDB_SET_ENTRY_ULONG(json_entry, id, self->entry->id);
    FTDB_SET_ENTRY_ULONG(json_entry, fid, self->entry->fid);
    FTDB_SET_ENTRY_STRING(json_entry, hash, self->entry->hash);
    FTDB_SET_ENTRY_STRING(json_entry, class, self->entry->class_name);
    FTDB_SET_ENTRY_STRING(json_entry, qualifiers, self->entry->qualifiers);
    FTDB_SET_ENTRY_ULONG(json_entry, size, self->entry->size);
    FTDB_SET_ENTRY_STRING(json_entry, str, self->entry->str);
    FTDB_SET_ENTRY_ULONG(json_entry, refcount, self->entry->refcount);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, refs, self->entry->refs);
    FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(json_entry, usedrefs, self->entry->usedrefs);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, decls, self->entry->decls);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, def, self->entry->def);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, memberoffsets, self->entry->memberoffsets);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, attrrefs, self->entry->attrrefs);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry, attrnum, self->entry->attrnum);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, name, self->entry->name);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, dependent, self->entry->isdependent);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, globalrefs, self->entry->globalrefs);
    FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(json_entry, values, self->entry->enumvalues);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, outerfn, self->entry->outerfn);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry, outerfnid, self->entry->outerfnid);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, implicit, self->entry->isimplicit);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, union, self->entry->isunion);
    FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(json_entry, refnames, self->entry->refnames);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, location, self->entry->location);
    FTDB_SET_ENTRY_INT_OPTIONAL(json_entry, variadic, self->entry->isvariadic);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, funrefs, self->entry->funrefs);
    FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(json_entry, useddef, self->entry->useddef);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, defhead, self->entry->defhead);
    FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(json_entry, identifiers, self->entry->identifiers);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, methods, self->entry->methods);

    if (self->entry->bitfields) {
        PyObject *bitfields = PyDict_New();
        for (unsigned long i = 0; i < self->entry->bitfields_count; ++i) {
            struct bitfield *bitfield = &self->entry->bitfields[i];
            PyObject *py_bkey = PyUnicode_FromFormat("%lu", bitfield->index);
            PyObject *py_bval = PyLong_FromUnsignedLong(bitfield->bitwidth);
            PyDict_SetItem(bitfields, py_bkey, py_bval);
            Py_DecRef(py_bkey);
            Py_DecRef(py_bval);
        }
        FTDB_SET_ENTRY_PYOBJECT(json_entry, bitfields, bitfields);
    }

    return json_entry;
}

PyMethodDef libftdb_ftdbTypeEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_type_entry_json, METH_VARARGS,
     "Returns the json representation of the ftdb type entry"},
    {"has_usedrefs", (PyCFunction)libftdb_ftdb_type_entry_has_usedrefs, METH_VARARGS,
     "Returns True if type entry has usedrefs field"},
    {"has_decls", (PyCFunction)libftdb_ftdb_type_entry_has_decls, METH_VARARGS,
     "Returns True if type entry has decls field"},
    {"has_attrs", (PyCFunction)libftdb_ftdb_type_entry_has_attrs, METH_VARARGS,
     "Returns True if type entry has attribute references"},
    {"has_outerfn", (PyCFunction)libftdb_ftdb_type_entry_has_outerfn, METH_VARARGS,
     "Returns True if type entry has outerfn field"},
    {"has_location", (PyCFunction)libftdb_ftdb_type_entry_has_location, METH_VARARGS,
     "Returns True if type entry has location field"},
    {"isConst", (PyCFunction)libftdb_ftdb_type_entry_is_const, METH_VARARGS, "Check whether the type is a const type"},
    {"toNonConst", (PyCFunction)libftdb_ftdb_type_entry_to_non_const, METH_VARARGS, "Returns the non-const counterpart of a type (if available)"},
    {"__deepcopy__", (PyCFunction)libftdb_ftdb_type_entry_deepcopy, METH_O, "Deep copy of an object"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbTypeEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_type_entry_sq_contains
};

PyMappingMethods libftdb_ftdbTypeEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_type_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbTypeEntry_getset[] = {
    {"id", libftdb_ftdb_type_entry_get_id, 0, "ftdb type entry id value", 0},
    {"fid", libftdb_ftdb_type_entry_get_fid, 0, "ftdb type entry fid value", 0},
    {"hash", libftdb_ftdb_type_entry_get_hash, 0, "ftdb type entry hash value", 0},
    {"classname", libftdb_ftdb_type_entry_get_classname, 0, "ftdb type entry class name value", 0},
    {"classid", libftdb_ftdb_type_entry_get_classid, 0, "ftdb type entry class id value", 0},
    {"qualifiers", libftdb_ftdb_type_entry_get_qualifiers, 0, "ftdb type entry qualifiers value", 0},
    {"size", libftdb_ftdb_type_entry_get_size, 0, "ftdb type entry size value", 0},
    {"str", libftdb_ftdb_type_entry_get_str, 0, "ftdb type entry str value", 0},
    {"refcount", libftdb_ftdb_type_entry_get_refcount, 0, "ftdb type entry refcount value", 0},
    {"refs", libftdb_ftdb_type_entry_get_refs, 0, "ftdb type entry refs values", 0},
    {"usedrefs", libftdb_ftdb_type_entry_get_usedrefs, 0, "ftdb type entry usedrefs values", 0},
    {"decls", libftdb_ftdb_type_entry_get_decls, 0, "ftdb type entry decls values", 0},
    {"defstring", libftdb_ftdb_type_entry_get_def, 0, "ftdb type entry def value", 0},
    {"memberoffsets", libftdb_ftdb_type_entry_get_memberoffsets, 0, "ftdb type entry memberoffsets values", 0},
    {"attrrefs", libftdb_ftdb_type_entry_get_attrrefs, 0, "ftdb type entry attrrefs values", 0},
    {"attrnum", libftdb_ftdb_type_entry_get_attrnum, 0, "ftdb type entry attrnum value", 0},
    {"name", libftdb_ftdb_type_entry_get_name, 0, "ftdb type entry name value", 0},
    {"dependent", libftdb_ftdb_type_entry_get_is_dependent, 0, "ftdb type entry is dependent value", 0},
    {"globalrefs", libftdb_ftdb_type_entry_get_globalrefs, 0, "ftdb type entry globalrefs value", 0},
    {"enumvalues", libftdb_ftdb_type_entry_get_enumvalues, 0, "ftdb type entry enum values", 0},
    {"values", libftdb_ftdb_type_entry_get_enumvalues, 0, "ftdb type entry enum values", 0},
    {"outerfn", libftdb_ftdb_type_entry_get_outerfn, 0, "ftdb type entry outerfn value", 0},
    {"outerfnid", libftdb_ftdb_type_entry_get_outerfnid, 0, "ftdb type entry outerfnid value", 0},
    {"implicit", libftdb_ftdb_type_entry_get_is_implicit, 0, "ftdb type entry is implicit value", 0},
    {"union", libftdb_ftdb_type_entry_get_is_union, 0, "ftdb type entry is union value", 0},
    {"isunion", libftdb_ftdb_type_entry_get_is_union, 0, "ftdb type entry is union value", 0},
    {"refnames", libftdb_ftdb_type_entry_get_refnames, 0, "ftdb type entry refnames values", 0},
    {"location", libftdb_ftdb_type_entry_get_location, 0, "ftdb type entry location value", 0},
    {"variadic", libftdb_ftdb_type_entry_get_is_variadic, 0, "ftdb type entry is variadic value", 0},
    {"funrefs", libftdb_ftdb_type_entry_get_funrefs, 0, "ftdb type entry funrefs values", 0},
    {"useddef", libftdb_ftdb_type_entry_get_useddef, 0, "ftdb type entry useddef values", 0},
    {"defhead", libftdb_ftdb_type_entry_get_defhead, 0, "ftdb type entry defhead value", 0},
    {"identifiers", libftdb_ftdb_type_entry_get_identifiers, 0, "ftdb type entry identifiers values", 0},
    {"methods", libftdb_ftdb_type_entry_get_methods, 0, "ftdb type entry methods values", 0},
    {"bitfields", libftdb_ftdb_type_entry_get_bitfields, 0, "ftdb type entry bitfields values", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbTypeEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbTypeEntry",
    .tp_basicsize = sizeof(libftdb_ftdbTypeEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_type_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_type_entry_repr,
    .tp_as_sequence = &libftdb_ftdbTypeEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbTypeEntry_mapping_methods,
    .tp_hash = (hashfunc)libftdb_ftdb_type_entry_hash,
    .tp_doc = "libftdb ftdb type entry object",
    .tp_richcompare = libftdb_ftdb_type_entry_richcompare,
    .tp_methods = libftdb_ftdbTypeEntry_methods,
    .tp_getset = libftdb_ftdbTypeEntry_getset,
    .tp_new = libftdb_ftdb_type_entry_new,
};
