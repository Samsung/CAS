#include "pyftdb.h"

static void libftdb_ftdb_func_entry_dealloc(libftdb_ftdb_func_entry_object *self) {
    Py_DecRef((PyObject *)self->funcs);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_func_entry_object *self;

    self = (libftdb_ftdb_func_entry_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->funcs = (const libftdb_ftdb_collection_object *)PyLong_AsLong(PyTuple_GetItem(args, 0));
        Py_IncRef((PyObject *)self->funcs);
        unsigned long index = PyLong_AsLong(PyTuple_GetItem(args, 1));
        if (index >= self->funcs->ftdb->funcs_count) {
            PyErr_SetString(libftdb_ftdbError, "func entry index out of bounds");
            return 0;
        }
        self->index = index;
        self->entry = &self->funcs->ftdb->funcs[self->index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "id:%lu|name:%s>", __self->entry->id, __self->entry->name);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_func_entry_json(libftdb_ftdb_func_entry_object *self, PyObject *args) {
    PyObject *json_entry = PyDict_New();

    FTDB_SET_ENTRY_STRING(json_entry, name, self->entry->name);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, namespace, self->entry->__namespace);
    FTDB_SET_ENTRY_ULONG(json_entry, id, self->entry->id);
    FTDB_SET_ENTRY_ULONG(json_entry, fid, self->entry->fid);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, fids, self->entry->fids);
    FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry, mids, self->entry->mids);
    FTDB_SET_ENTRY_ULONG(json_entry, nargs, self->entry->nargs);
    FTDB_SET_ENTRY_BOOL(json_entry, variadic, self->entry->isvariadic);
    FTDB_SET_ENTRY_STRING(json_entry, firstNonDeclStmt, self->entry->firstNonDeclStmt);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, inline, self->entry->isinline);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, template, self->entry->istemplate);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, classOuterFn, self->entry->classOuterFn);
    FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry, linkage, self->entry->linkage, functionLinkage);
    FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry, member, self->entry->ismember);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, class, self->entry->__class);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry, classid, self->entry->classid);
    FTDB_SET_ENTRY_STRING_ARRAY(json_entry, attributes, self->entry->attributes);
    FTDB_SET_ENTRY_STRING(json_entry, hash, self->entry->hash);
    FTDB_SET_ENTRY_STRING(json_entry, cshash, self->entry->cshash);
    FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry, template_parameters, self->entry->template_parameters);
    FTDB_SET_ENTRY_STRING(json_entry, body, self->entry->body);
    FTDB_SET_ENTRY_STRING(json_entry, unpreprocessed_body, self->entry->unpreprocessed_body);
    FTDB_SET_ENTRY_STRING(json_entry, declbody, self->entry->declbody);
    FTDB_SET_ENTRY_STRING(json_entry, signature, self->entry->signature);
    FTDB_SET_ENTRY_STRING(json_entry, declhash, self->entry->declhash);
    FTDB_SET_ENTRY_STRING(json_entry, location, self->entry->location);
    FTDB_SET_ENTRY_STRING(json_entry, start_loc, self->entry->start_loc);
    FTDB_SET_ENTRY_STRING(json_entry, end_loc, self->entry->end_loc);
    FTDB_SET_ENTRY_ULONG(json_entry, refcount, self->entry->refcount);

    PyObject *literals = PyDict_New();
    FTDB_SET_ENTRY_INT64_ARRAY(literals, integer, self->entry->integer_literals);
    FTDB_SET_ENTRY_INT_ARRAY(literals, character, self->entry->character_literals);
    FTDB_SET_ENTRY_FLOAT_ARRAY(literals, floating, self->entry->floating_literals);
    FTDB_SET_ENTRY_STRING_ARRAY(literals, string, self->entry->string_literals);
    FTDB_SET_ENTRY_PYOBJECT(json_entry, literals, literals);

    FTDB_SET_ENTRY_ULONG(json_entry, declcount, self->entry->declcount);

    PyObject *taint = PyDict_New();
    for (unsigned long i = 0; i < self->entry->taint_count; ++i) {
        struct taint_data *taint_data = &self->entry->taint[i];
        PyObject *taint_list = PyList_New(0);
        for (unsigned long j = 0; j < taint_data->taint_list_count; ++j) {
            struct taint_element *taint_element = &taint_data->taint_list[j];
            PyObject *py_taint_element = PyList_New(0);
            PYLIST_ADD_ULONG(py_taint_element, taint_element->taint_level);
            PYLIST_ADD_ULONG(py_taint_element, taint_element->varid);
            PYLIST_ADD_PYOBJECT(taint_list, py_taint_element);
        }
        PyObject *key = PyUnicode_FromFormat("%lu", i);
        PyDict_SetItem(taint, key, taint_list);
        Py_DecRef(taint_list);
        Py_DecRef(key);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, taint, taint);

    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, calls, self->entry->calls);
    PyObject *call_info = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->calls_count; ++i) {
        struct call_info *call_info_entry = &self->entry->call_info[i];
        PyObject *py_callinfo = PyDict_New();
        FTDB_SET_ENTRY_STRING(py_callinfo, start, call_info_entry->start);
        FTDB_SET_ENTRY_STRING(py_callinfo, end, call_info_entry->end);
        FTDB_SET_ENTRY_ULONG(py_callinfo, ord, call_info_entry->ord);
        FTDB_SET_ENTRY_STRING(py_callinfo, expr, call_info_entry->expr);
        FTDB_SET_ENTRY_STRING(py_callinfo, loc, call_info_entry->loc);
        FTDB_SET_ENTRY_INT64_OPTIONAL(py_callinfo, csid, call_info_entry->csid);
        FTDB_SET_ENTRY_ULONG_ARRAY(py_callinfo, args, call_info_entry->args);
        PYLIST_ADD_PYOBJECT(call_info, py_callinfo);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, call_info, call_info);

    PyObject *callrefs = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->calls_count; ++i) {
        struct callref_info *callref_info = &self->entry->callrefs[i];
        PyObject *py_callref_info = PyList_New(0);
        for (unsigned long j = 0; j < callref_info->callarg_count; ++j) {
            struct callref_data *callref_data = &callref_info->callarg[j];
            PyObject *py_callref_data = PyDict_New();
            FTDB_SET_ENTRY_ENUM_TO_STRING(py_callref_data, type, callref_data->type, exprType);
            FTDB_SET_ENTRY_ULONG(py_callref_data, pos, callref_data->pos);
            if (callref_data->id) {
                FTDB_SET_ENTRY_ULONG_OPTIONAL(py_callref_data, id, callref_data->id);
            } else if (callref_data->integer_literal) {
                FTDB_SET_ENTRY_INT64_OPTIONAL(py_callref_data, id, callref_data->integer_literal);
            } else if (callref_data->character_literal) {
                FTDB_SET_ENTRY_INT_OPTIONAL(py_callref_data, id, callref_data->character_literal);
            } else if (callref_data->floating_literal) {
                FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_callref_data, id, callref_data->floating_literal);
            } else if (callref_data->string_literal) {
                FTDB_SET_ENTRY_STRING_OPTIONAL(py_callref_data, id, callref_data->string_literal);
            }
            PYLIST_ADD_PYOBJECT(py_callref_info, py_callref_data);
        }
        PYLIST_ADD_PYOBJECT(callrefs, py_callref_info);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, callrefs, callrefs);

    PyObject *refcalls = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->refcalls_count; ++i) {
        struct refcall *refcall = &self->entry->refcalls[i];
        PyObject *refcall_info = PyList_New(0);
        if (refcall->ismembercall) {
            PYLIST_ADD_ULONG(refcall_info, refcall->fid);
            PYLIST_ADD_ULONG(refcall_info, refcall->cid);
            PYLIST_ADD_ULONG(refcall_info, refcall->field_index);
        } else {
            PYLIST_ADD_ULONG(refcall_info, refcall->fid);
        }
        PYLIST_ADD_PYOBJECT(refcalls, refcall_info);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, refcalls, refcalls);

    PyObject *refcall_info = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->refcalls_count; ++i) {
        struct call_info *refcall_info_entry = &self->entry->refcall_info[i];
        PyObject *py_refcallinfo = PyDict_New();
        FTDB_SET_ENTRY_STRING(py_refcallinfo, start, refcall_info_entry->start);
        FTDB_SET_ENTRY_STRING(py_refcallinfo, end, refcall_info_entry->end);
        FTDB_SET_ENTRY_ULONG(py_refcallinfo, ord, refcall_info_entry->ord);
        FTDB_SET_ENTRY_STRING(py_refcallinfo, expr, refcall_info_entry->expr);
        FTDB_SET_ENTRY_INT64_OPTIONAL(py_refcallinfo, csid, refcall_info_entry->csid);
        FTDB_SET_ENTRY_STRING_OPTIONAL(py_refcallinfo, loc, refcall_info_entry->loc);
        FTDB_SET_ENTRY_ULONG_ARRAY(py_refcallinfo, args, refcall_info_entry->args);
        PYLIST_ADD_PYOBJECT(refcall_info, py_refcallinfo);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, refcall_info, refcall_info);

    PyObject *refcallrefs = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->refcalls_count; ++i) {
        struct callref_info *refcallref_info = &self->entry->refcallrefs[i];
        PyObject *py_refcallref_info = PyList_New(0);
        for (unsigned long j = 0; j < refcallref_info->callarg_count; ++j) {
            struct callref_data *refcallref_data = &refcallref_info->callarg[j];
            PyObject *py_refcallref_data = PyDict_New();
            FTDB_SET_ENTRY_ENUM_TO_STRING(py_refcallref_data, type, refcallref_data->type, exprType);
            FTDB_SET_ENTRY_ULONG(py_refcallref_data, pos, refcallref_data->pos);
            if (refcallref_data->id) {
                FTDB_SET_ENTRY_ULONG_OPTIONAL(py_refcallref_data, id, refcallref_data->id);
            } else if (refcallref_data->integer_literal) {
                FTDB_SET_ENTRY_INT64_OPTIONAL(py_refcallref_data, id, refcallref_data->integer_literal);
            } else if (refcallref_data->character_literal) {
                FTDB_SET_ENTRY_INT_OPTIONAL(py_refcallref_data, id, refcallref_data->character_literal);
            } else if (refcallref_data->floating_literal) {
                FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_refcallref_data, id, refcallref_data->floating_literal);
            } else if (refcallref_data->string_literal) {
                FTDB_SET_ENTRY_STRING_OPTIONAL(py_refcallref_data, id, refcallref_data->string_literal);
            }
            PYLIST_ADD_PYOBJECT(py_refcallref_info, py_refcallref_data);
        }
        PYLIST_ADD_PYOBJECT(refcallrefs, py_refcallref_info);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, refcallrefs, refcallrefs);

    PyObject *switches = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->switches_count; ++i) {
        struct switch_info *switch_info_entry = &self->entry->switches[i];
        PyObject *py_switch_info = PyDict_New();
        FTDB_SET_ENTRY_STRING(py_switch_info, condition, switch_info_entry->condition);
        PyObject *cases = PyList_New(0);
        for (unsigned long j = 0; j < switch_info_entry->cases_count; ++j) {
            struct case_info *case_info = &switch_info_entry->cases[j];
            PyObject *py_case_info = PyList_New(0);
            PYLIST_ADD_LONG(py_case_info, case_info->lhs.expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.raw_code);
            if (case_info->rhs) {
                PYLIST_ADD_LONG(py_case_info, case_info->rhs->expr_value);
                PYLIST_ADD_STRING(py_case_info, case_info->rhs->enum_code);
                PYLIST_ADD_STRING(py_case_info, case_info->rhs->macro_code);
                PYLIST_ADD_STRING(py_case_info, case_info->rhs->raw_code);
            }
            PYLIST_ADD_PYOBJECT(cases, py_case_info);
        }
        FTDB_SET_ENTRY_PYOBJECT(py_switch_info, cases, cases);
        PYLIST_ADD_PYOBJECT(switches, py_switch_info);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, switches, switches);

    PyObject *csmap = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->csmap_count; ++i) {
        struct csitem *csitem = &self->entry->csmap[i];
        PyObject *py_csitem = PyDict_New();
        FTDB_SET_ENTRY_INT64(py_csitem, pid, csitem->pid);
        FTDB_SET_ENTRY_ULONG(py_csitem, id, csitem->id);
        FTDB_SET_ENTRY_STRING(py_csitem, cf, csitem->cf);
        PYLIST_ADD_PYOBJECT(csmap, py_csitem);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, csmap, csmap);

    if (self->entry->macro_expansions) {
        PyObject *macro_expansions = PyList_New(0);
        for (unsigned long i = 0; i < self->entry->macro_expansions_count; ++i) {
            struct mexp_info *mexp_info = &self->entry->macro_expansions[i];
            PyObject *py_mexp_info = PyDict_New();
            FTDB_SET_ENTRY_ULONG(py_mexp_info, pos, mexp_info->pos);
            FTDB_SET_ENTRY_ULONG(py_mexp_info, len, mexp_info->len);
            FTDB_SET_ENTRY_STRING(py_mexp_info, text, mexp_info->text);
            PYLIST_ADD_PYOBJECT(macro_expansions, py_mexp_info);
        }
        FTDB_SET_ENTRY_PYOBJECT(json_entry, macro_expansions, macro_expansions);
    }

    PyObject *locals = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->locals_count; ++i) {
        struct local_info *local_info_entry = &self->entry->locals[i];
        PyObject *py_local_entry = PyDict_New();
        FTDB_SET_ENTRY_ULONG(py_local_entry, id, local_info_entry->id);
        FTDB_SET_ENTRY_STRING(py_local_entry, name, local_info_entry->name);
        FTDB_SET_ENTRY_ULONG(py_local_entry, type, local_info_entry->type);
        FTDB_SET_ENTRY_BOOL(py_local_entry, parm, local_info_entry->isparm);
        FTDB_SET_ENTRY_BOOL(py_local_entry, static, local_info_entry->isstatic);
        FTDB_SET_ENTRY_BOOL(py_local_entry, used, local_info_entry->isused);
        FTDB_SET_ENTRY_STRING(py_local_entry, location, local_info_entry->location);
        FTDB_SET_ENTRY_INT64_OPTIONAL(py_local_entry, csid, local_info_entry->csid);
        PYLIST_ADD_PYOBJECT(locals, py_local_entry);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, locals, locals);

    PyObject *derefs = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->derefs_count; ++i) {
        struct deref_info *deref_info_entry = &self->entry->derefs[i];
        PyObject *py_deref_entry = PyDict_New();
        FTDB_SET_ENTRY_ENUM_TO_STRING(py_deref_entry, kind, deref_info_entry->kind, exprType);
        FTDB_SET_ENTRY_INT64_OPTIONAL(py_deref_entry, offset, deref_info_entry->offset);
        FTDB_SET_ENTRY_ULONG_OPTIONAL(py_deref_entry, basecnt, deref_info_entry->basecnt);
        PyObject *offsetrefs = PyList_New(0);
        for (unsigned long j = 0; j < deref_info_entry->offsetrefs_count; ++j) {
            struct offsetref_info *offsetref_info = &deref_info_entry->offsetrefs[j];
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
        FTDB_SET_ENTRY_STRING(py_deref_entry, expr, deref_info_entry->expr);
        FTDB_SET_ENTRY_INT64(py_deref_entry, csid, deref_info_entry->csid);
        FTDB_SET_ENTRY_ULONG_ARRAY(py_deref_entry, ord, deref_info_entry->ord);
        FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry, member, deref_info_entry->member);
        FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry, type, deref_info_entry->type);
        FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry, access, deref_info_entry->access);
        FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry, shift, deref_info_entry->shift);
        FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry, mcall, deref_info_entry->mcall);
        PYLIST_ADD_PYOBJECT(derefs, py_deref_entry);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, derefs, derefs);

    PyObject *ifs = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->ifs_count; ++i) {
        struct if_info *if_entry = &self->entry->ifs[i];
        PyObject *py_if_entry = PyDict_New();
        FTDB_SET_ENTRY_INT64(py_if_entry, csid, if_entry->csid);
        PyObject *if_refs = PyList_New(0);
        for (unsigned long j = 0; j < if_entry->refs_count; ++j) {
            struct ifref_info *ifref_info = &if_entry->refs[j];
            PyObject *py_ifref_info = PyDict_New();
            FTDB_SET_ENTRY_ENUM_TO_STRING(py_ifref_info, type, ifref_info->type, exprType);
            if (ifref_info->id) {
                FTDB_SET_ENTRY_ULONG_OPTIONAL(py_ifref_info, id, ifref_info->id);
            } else if (ifref_info->integer_literal) {
                FTDB_SET_ENTRY_INT64_OPTIONAL(py_ifref_info, id, ifref_info->integer_literal);
            } else if (ifref_info->character_literal) {
                FTDB_SET_ENTRY_INT_OPTIONAL(py_ifref_info, id, ifref_info->character_literal);
            } else if (ifref_info->floating_literal) {
                FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_ifref_info, id, ifref_info->floating_literal);
            } else if (ifref_info->string_literal) {
                FTDB_SET_ENTRY_STRING_OPTIONAL(py_ifref_info, id, ifref_info->string_literal);
            }
            PYLIST_ADD_PYOBJECT(if_refs, py_ifref_info);
        }
        FTDB_SET_ENTRY_PYOBJECT(py_if_entry, refs, if_refs);
        PYLIST_ADD_PYOBJECT(ifs, py_if_entry);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, ifs, ifs);

    PyObject *asms = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->asms_count; ++i) {
        struct asm_info *asm_info = &self->entry->asms[i];
        PyObject *py_asm_entry = PyDict_New();
        FTDB_SET_ENTRY_INT64(py_asm_entry, csid, asm_info->csid);
        FTDB_SET_ENTRY_STRING(py_asm_entry, str, asm_info->str);
        PYLIST_ADD_PYOBJECT(asms, py_asm_entry);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, asm, asms);

    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, globalrefs, self->entry->globalrefs);

    PyObject *globalrefInfo = PyList_New(0);
    for (unsigned long i = 0; i < self->entry->globalrefs_count; ++i) {
        struct globalref_data *globalref_data = &self->entry->globalrefInfo[i];
        PyObject *refs = PyList_New(0);
        for (unsigned long j = 0; j < globalref_data->refs_count; ++j) {
            struct globalref_info *globalref_info = &globalref_data->refs[j];
            PyObject *py_globalref_info = PyDict_New();
            FTDB_SET_ENTRY_STRING(py_globalref_info, start, globalref_info->start);
            FTDB_SET_ENTRY_STRING(py_globalref_info, end, globalref_info->end);
            PYLIST_ADD_PYOBJECT(refs, py_globalref_info);
        }
        PYLIST_ADD_PYOBJECT(globalrefInfo, refs);
    }
    FTDB_SET_ENTRY_PYOBJECT(json_entry, globalrefInfo, globalrefInfo);

    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, funrefs, self->entry->funrefs);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, refs, self->entry->refs);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, decls, self->entry->decls);
    FTDB_SET_ENTRY_ULONG_ARRAY(json_entry, types, self->entry->types);

    return json_entry;
}

static PyObject *libftdb_ftdb_func_entry_get_object_refcount(PyObject *self, void *closure) {
    return PyLong_FromLong(self->ob_refcnt);
}

static PyObject *libftdb_ftdb_func_entry_get_id(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyLong_FromUnsignedLong(__self->entry->id);
}

static PyObject *libftdb_ftdb_func_entry_get_hash(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->hash);
}

static PyObject *libftdb_ftdb_func_entry_get_name(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->name);
}

static PyObject *libftdb_ftdb_func_entry_has_namespace(libftdb_ftdb_func_entry_object *self, PyObject *args) {
    if (self->entry->__namespace) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_has_classOuterFn(libftdb_ftdb_func_entry_object *self, PyObject *args) {
    if (self->entry->classOuterFn) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_has_class(libftdb_ftdb_func_entry_object *self, PyObject *args) {
    if ((self->entry->__class) || (self->entry->classid)) {
        if (!self->entry->__class) {
            PyErr_SetString(libftdb_ftdbError, "No 'class' field in function entry");
            return 0;
        }
        if (!self->entry->classid) {
            PyErr_SetString(libftdb_ftdbError, "No 'classid' field in function entry");
            return 0;
        }
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_get_namespace(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->__namespace) {
        PyErr_SetString(libftdb_ftdbError, "No 'namespace' field in function entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->__namespace);
}

static PyObject *libftdb_ftdb_func_entry_get_fid(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyLong_FromUnsignedLong(__self->entry->fid);
}

static PyObject *libftdb_ftdb_func_entry_get_fids(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *fids = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->fids_count; ++i) {
        PYLIST_ADD_ULONG(fids, __self->entry->fids[i]);
    }
    return fids;
}

static PyObject *libftdb_ftdb_func_entry_get_mids(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->mids) {
        Py_RETURN_NONE;
    }

    PyObject *mids = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->mids_count; ++i) {
        PYLIST_ADD_ULONG(mids, __self->entry->mids[i]);
    }
    return mids;
}

static PyObject *libftdb_ftdb_func_entry_get_nargs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyLong_FromUnsignedLong(__self->entry->nargs);
}

static PyObject *libftdb_ftdb_func_entry_get_variadic(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (__self->entry->isvariadic) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_get_firstNonDeclStmt(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->firstNonDeclStmt);
}

static PyObject *libftdb_ftdb_func_entry_get_inline(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if ((__self->entry->isinline) && (*__self->entry->isinline)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_get_template(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if ((__self->entry->istemplate) && (*__self->entry->istemplate)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_get_classOuterFn(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->classOuterFn) {
        PyErr_SetString(libftdb_ftdbError, "No 'classOuterFn' field in function entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->classOuterFn);
}

static PyObject *libftdb_ftdb_func_entry_get_linkage(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyLong_FromLong((long)__self->entry->linkage);
}

static PyObject *libftdb_ftdb_func_entry_get_linkage_string(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(set_functionLinkage(__self->entry->linkage));
}

static PyObject *libftdb_ftdb_func_entry_get_member(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if ((__self->entry->ismember) && (*__self->entry->ismember)) {
        Py_RETURN_TRUE;
    }

    Py_RETURN_FALSE;
}

static PyObject *libftdb_ftdb_func_entry_get_class(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->__class) {
        PyErr_SetString(libftdb_ftdbError, "No 'class' field in function entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->__class);
}

static PyObject *libftdb_ftdb_func_entry_get_classid(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->classid) {
        PyErr_SetString(libftdb_ftdbError, "No 'classid' field in function entry");
        return 0;
    }

    return PyLong_FromUnsignedLong(*__self->entry->classid);
}

static PyObject *libftdb_ftdb_func_entry_get_attributes(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *attributes = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->attributes_count; ++i) {
        PYLIST_ADD_STRING(attributes, __self->entry->attributes[i]);
    }
    return attributes;
}

static PyObject *libftdb_ftdb_func_entry_get_cshash(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->cshash);
}

static PyObject *libftdb_ftdb_func_entry_get_template_parameters(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->template_parameters) {
        PyErr_SetString(libftdb_ftdbError, "No 'template_parameters' field in function entry");
        return 0;
    }

    return PyUnicode_FromString(__self->entry->template_parameters);
}

static PyObject *libftdb_ftdb_func_entry_get_body(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->body);
}

static PyObject *libftdb_ftdb_func_entry_get_unpreprocessed_body(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->unpreprocessed_body);
}

static PyObject *libftdb_ftdb_func_entry_get_declbody(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->declbody);
}

static PyObject *libftdb_ftdb_func_entry_get_signature(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->signature);
}

static PyObject *libftdb_ftdb_func_entry_get_declhash(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->declhash);
}

static PyObject *libftdb_ftdb_func_entry_get_location(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->location);
}

static PyObject *libftdb_ftdb_func_entry_get_start_loc(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->start_loc);
}

static PyObject *libftdb_ftdb_func_entry_get_end_loc(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyUnicode_FromString(__self->entry->end_loc);
}

static PyObject *libftdb_ftdb_func_entry_get_refcount(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyLong_FromUnsignedLong(__self->entry->refcount);
}

static PyObject *libftdb_ftdb_func_entry_get_declcount(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    return PyLong_FromUnsignedLong(__self->entry->declcount);
}

static PyObject *libftdb_ftdb_func_entry_get_literals(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *literals = PyDict_New();
    FTDB_SET_ENTRY_INT64_ARRAY(literals, integer, __self->entry->integer_literals);
    FTDB_SET_ENTRY_FLOAT_ARRAY(literals, floating, __self->entry->floating_literals);
    FTDB_SET_ENTRY_UINT_ARRAY(literals, character, __self->entry->character_literals);
    FTDB_SET_ENTRY_STRING_ARRAY(literals, string, __self->entry->string_literals);

    return literals;
}

static PyObject *libftdb_ftdb_func_entry_get_integer_literals(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *integer_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->integer_literals_count; ++i) {
        PYLIST_ADD_LONG(integer_literals, __self->entry->integer_literals[i]);
    }
    return integer_literals;
}

static PyObject *libftdb_ftdb_func_entry_get_character_literals(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *character_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->character_literals_count; ++i) {
        PYLIST_ADD_ULONG(character_literals, __self->entry->character_literals[i]);
    }
    return character_literals;
}

static PyObject *libftdb_ftdb_func_entry_get_floating_literals(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *floating_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->floating_literals_count; ++i) {
        PYLIST_ADD_FLOAT(floating_literals, __self->entry->floating_literals[i]);
    }
    return floating_literals;
}

static PyObject *libftdb_ftdb_func_entry_get_string_literals(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *string_literals = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->string_literals_count; ++i) {
        PYLIST_ADD_STRING(string_literals, __self->entry->string_literals[i]);
    }
    return string_literals;
}

static PyObject *libftdb_ftdb_func_entry_get_taint(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *taint = PyDict_New();

    for (unsigned long i = 0; i < __self->entry->taint_count; ++i) {
        struct taint_data *taint_data = &__self->entry->taint[i];
        PyObject *py_taint_list = PyList_New(0);
        for (unsigned long j = 0; j < taint_data->taint_list_count; ++j) {
            struct taint_element *taint_element = &taint_data->taint_list[j];
            PyObject *py_taint_element = PyList_New(0);
            PYLIST_ADD_ULONG(py_taint_element, taint_element->taint_level);
            PYLIST_ADD_ULONG(py_taint_element, taint_element->varid);
            PyList_Append(py_taint_list, py_taint_element);
            Py_DecRef(py_taint_element);
        }
        PyObject *key = PyUnicode_FromFormat("%lu", i);
        PyDict_SetItem(taint, key, py_taint_list);
        Py_DecRef(key);
        Py_DecRef(py_taint_list);
    }
    return taint;
}

static PyObject *libftdb_ftdb_func_entry_get_calls(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *calls = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->calls_count; ++i) {
        PYLIST_ADD_ULONG(calls, __self->entry->calls[i]);
    }
    return calls;
}

static PyObject *libftdb_ftdb_func_entry_get_call_info(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *call_info = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->calls_count; ++i) {
        PyObject *args = PyTuple_New(4);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, __self->index);
        PYTUPLE_SET_ULONG(args, 2, i);
        PYTUPLE_SET_ULONG(args, 3, 0);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncCallInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(call_info, entry);
        Py_DecRef(entry);
    }
    return call_info;
}

static PyObject *libftdb_ftdb_func_entry_get_callrefs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *callrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->calls_count; ++i) {
        struct callref_info *callref_info = &__self->entry->callrefs[i];
        PyObject *py_callref_info = PyList_New(0);
        for (unsigned long j = 0; j < callref_info->callarg_count; ++j) {
            struct callref_data *callref_data = &callref_info->callarg[j];
            PyObject *py_callref_data = PyDict_New();
            FTDB_SET_ENTRY_ENUM_TO_STRING(py_callref_data, type, callref_data->type, exprType);
            FTDB_SET_ENTRY_ULONG(py_callref_data, pos, callref_data->pos);
            if (callref_data->id) {
                FTDB_SET_ENTRY_ULONG_OPTIONAL(py_callref_data, id, callref_data->id);
            } else if (callref_data->integer_literal) {
                FTDB_SET_ENTRY_INT64_OPTIONAL(py_callref_data, id, callref_data->integer_literal);
            } else if (callref_data->character_literal) {
                FTDB_SET_ENTRY_UINT_OPTIONAL(py_callref_data, id, callref_data->character_literal);
            } else if (callref_data->floating_literal) {
                FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_callref_data, id, callref_data->floating_literal);
            } else if (callref_data->string_literal) {
                FTDB_SET_ENTRY_STRING_OPTIONAL(py_callref_data, id, callref_data->string_literal);
            }
            PyList_Append(py_callref_info, py_callref_data);
            Py_DecRef(py_callref_data);
        }
        PyList_Append(callrefs, py_callref_info);
        Py_DecRef(py_callref_info);
    }

    return callrefs;
}

static PyObject *libftdb_ftdb_func_entry_get_refcalls(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *refcalls = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refcalls_count; ++i) {
        struct refcall *refcall = &__self->entry->refcalls[i];
        if (refcall->ismembercall) {
            PyObject *refcall_info = PyList_New(0);
            PYLIST_ADD_ULONG(refcall_info, refcall->fid);
            PYLIST_ADD_ULONG(refcall_info, refcall->cid);
            PYLIST_ADD_ULONG(refcall_info, refcall->field_index);
            PyList_Append(refcalls, refcall_info);
            Py_DecRef(refcall_info);
        } else {
            PyObject *refcall_info = PyList_New(0);
            PYLIST_ADD_ULONG(refcall_info, refcall->fid);
            PyList_Append(refcalls, refcall_info);
            Py_DecRef(refcall_info);
        }
    }
    return refcalls;
}

static PyObject *libftdb_ftdb_func_entry_get_refcall_info(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *call_info = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refcalls_count; ++i) {
        PyObject *args = PyTuple_New(4);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, __self->index);
        PYTUPLE_SET_ULONG(args, 2, i);
        PYTUPLE_SET_ULONG(args, 3, 1);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncCallInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(call_info, entry);
        Py_DecRef(entry);
    }
    return call_info;
}

static PyObject *libftdb_ftdb_func_entry_get_refcallrefs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *refcallrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refcalls_count; ++i) {
        struct callref_info *refcallref_info = &__self->entry->refcallrefs[i];
        PyObject *py_refcallref_info = PyList_New(0);
        for (unsigned long j = 0; j < refcallref_info->callarg_count; ++j) {
            struct callref_data *refcallref_data = &refcallref_info->callarg[j];
            PyObject *py_refcallref_data = PyDict_New();
            FTDB_SET_ENTRY_ENUM_TO_STRING(py_refcallref_data, type, refcallref_data->type, exprType);
            FTDB_SET_ENTRY_ULONG(py_refcallref_data, pos, refcallref_data->pos);
            if (refcallref_data->id) {
                FTDB_SET_ENTRY_ULONG_OPTIONAL(py_refcallref_data, id, refcallref_data->id);
            } else if (refcallref_data->integer_literal) {
                FTDB_SET_ENTRY_INT64_OPTIONAL(py_refcallref_data, id, refcallref_data->integer_literal);
            } else if (refcallref_data->character_literal) {
                FTDB_SET_ENTRY_UINT_OPTIONAL(py_refcallref_data, id, refcallref_data->character_literal);
            } else if (refcallref_data->floating_literal) {
                FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_refcallref_data, id, refcallref_data->floating_literal);
            } else if (refcallref_data->string_literal) {
                FTDB_SET_ENTRY_STRING_OPTIONAL(py_refcallref_data, id, refcallref_data->string_literal);
            }
            PyList_Append(py_refcallref_info, py_refcallref_data);
            Py_DecRef(py_refcallref_data);
        }
        PyList_Append(refcallrefs, py_refcallref_info);
        Py_DecRef(py_refcallref_info);
    }

    return refcallrefs;
}

static PyObject *libftdb_ftdb_func_entry_get_switches(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *switch_info = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->switches_count; ++i) {
        PyObject *args = PyTuple_New(3);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, __self->index);
        PYTUPLE_SET_ULONG(args, 2, i);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncSwitchInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(switch_info, entry);
        Py_DecRef(entry);
    }
    return switch_info;
}

static PyObject *libftdb_ftdb_func_entry_get_csmap(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *csmap = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->csmap_count; ++i) {
        struct csitem *csitem = &__self->entry->csmap[i];
        PyObject *py_csitem = PyDict_New();
        FTDB_SET_ENTRY_INT64(py_csitem, pid, csitem->pid);
        FTDB_SET_ENTRY_ULONG(py_csitem, id, csitem->id);
        FTDB_SET_ENTRY_STRING(py_csitem, cf, csitem->cf);
        PyList_Append(csmap, py_csitem);
        Py_DecRef(py_csitem);
    }
    return csmap;
}

static PyObject *libftdb_ftdb_func_entry_get_macro_expansions(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    if (!__self->entry->macro_expansions) {
        PyErr_SetString(libftdb_ftdbError, "No 'macro_expansions' field in func entry");
        return 0;
    }

    PyObject *macro_expansions = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->macro_expansions_count; ++i) {
        struct mexp_info *mexp_info = &__self->entry->macro_expansions[i];
        PyObject *py_mexp_info = PyDict_New();
        FTDB_SET_ENTRY_INT64(py_mexp_info, pos, mexp_info->pos);
        FTDB_SET_ENTRY_ULONG(py_mexp_info, len, mexp_info->len);
        FTDB_SET_ENTRY_STRING(py_mexp_info, text, mexp_info->text);
        PyList_Append(macro_expansions, py_mexp_info);
        Py_DecRef(py_mexp_info);
    }
    return macro_expansions;
}

static PyObject *libftdb_ftdb_func_entry_get_locals(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *local_info = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->locals_count; ++i) {
        PyObject *args = PyTuple_New(3);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, __self->index);
        PYTUPLE_SET_ULONG(args, 2, i);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncLocalInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(local_info, entry);
        Py_DecRef(entry);
    }
    return local_info;
}

static PyObject *libftdb_ftdb_func_entry_get_derefs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *derefs_info = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->derefs_count; ++i) {
        PyObject *args = PyTuple_New(3);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, __self->index);
        PYTUPLE_SET_ULONG(args, 2, i);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncDerefInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(derefs_info, entry);
        Py_DecRef(entry);
    }
    return derefs_info;
}

static PyObject *libftdb_ftdb_func_entry_get_ifs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *ifs_info = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->ifs_count; ++i) {
        PyObject *args = PyTuple_New(3);
        PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self);
        PYTUPLE_SET_ULONG(args, 1, __self->index);
        PYTUPLE_SET_ULONG(args, 2, i);
        PyObject *entry = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncIfInfoEntryType, args);
        Py_DecRef(args);
        PyList_Append(ifs_info, entry);
        Py_DecRef(entry);
    }
    return ifs_info;
}

static PyObject *libftdb_ftdb_func_entry_get_asms(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *asms = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->asms_count; ++i) {
        struct asm_info *asm_item = &__self->entry->asms[i];
        PyObject *py_asm_item = PyDict_New();
        FTDB_SET_ENTRY_INT64(py_asm_item, csid, asm_item->csid);
        FTDB_SET_ENTRY_STRING(py_asm_item, str, asm_item->str);
        PyList_Append(asms, py_asm_item);
        Py_DecRef(py_asm_item);
    }
    return asms;
}

static PyObject *libftdb_ftdb_func_entry_get_globalrefs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *globalrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->globalrefs_count; ++i) {
        PYLIST_ADD_ULONG(globalrefs, __self->entry->globalrefs[i]);
    }
    return globalrefs;
}

static PyObject *libftdb_ftdb_func_entry_get_globalrefInfo(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *globalrefInfo = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->globalrefs_count; ++i) {
        struct globalref_data *globalref_data = &__self->entry->globalrefInfo[i];
        PyObject *py_globalref_data = PyList_New(0);
        for (unsigned long j = 0; j < globalref_data->refs_count; ++j) {
            struct globalref_info *globalref_info = &globalref_data->refs[j];
            PyObject *py_globalref_info = PyDict_New();
            FTDB_SET_ENTRY_STRING(py_globalref_info, start, globalref_info->start);
            FTDB_SET_ENTRY_STRING(py_globalref_info, end, globalref_info->end);
            PyList_Append(py_globalref_data, py_globalref_info);
            Py_DecRef(py_globalref_info);
        }
        PyList_Append(globalrefInfo, py_globalref_data);
        Py_DecRef(py_globalref_data);
    }
    return globalrefInfo;
}

static PyObject *libftdb_ftdb_func_entry_get_funrefs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *funrefs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->funrefs_count; ++i) {
        PYLIST_ADD_ULONG(funrefs, __self->entry->funrefs[i]);
    }
    return funrefs;
}

static PyObject *libftdb_ftdb_func_entry_get_refs(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *refs = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->refs_count; ++i) {
        PYLIST_ADD_ULONG(refs, __self->entry->refs[i]);
    }
    return refs;
}

static PyObject *libftdb_ftdb_func_entry_get_decls(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *decls = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->decls_count; ++i) {
        PYLIST_ADD_ULONG(decls, __self->entry->decls[i]);
    }
    return decls;
}

static PyObject *libftdb_ftdb_func_entry_get_types(PyObject *self, void *closure) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;

    PyObject *types = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->types_count; ++i) {
        PYLIST_ADD_ULONG(types, __self->entry->types[i]);
    }
    return types;
}

static PyObject *libftdb_ftdb_func_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_id(self, 0);
    } else if (!strcmp(attr, "hash")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_hash(self, 0);
    } else if (!strcmp(attr, "name")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_name(self, 0);
    } else if (!strcmp(attr, "namespace")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_namespace(self, 0);
    } else if (!strcmp(attr, "fid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_fid(self, 0);
    } else if (!strcmp(attr, "fids")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_fids(self, 0);
    } else if (!strcmp(attr, "mids")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_mids(self, 0);
    } else if (!strcmp(attr, "nargs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_nargs(self, 0);
    } else if (!strcmp(attr, "variadic")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_variadic(self, 0);
    } else if (!strcmp(attr, "firstNonDeclStmt")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_firstNonDeclStmt(self, 0);
    } else if (!strcmp(attr, "inline")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_inline(self, 0);
    } else if (!strcmp(attr, "template")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_template(self, 0);
    } else if (!strcmp(attr, "classOuterFn")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_classOuterFn(self, 0);
    } else if (!strcmp(attr, "linkage")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_linkage_string(self, 0);
    } else if (!strcmp(attr, "member")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_member(self, 0);
    } else if (!strcmp(attr, "class")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_class(self, 0);
    } else if (!strcmp(attr, "classid")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_classid(self, 0);
    } else if (!strcmp(attr, "attributes")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_attributes(self, 0);
    } else if (!strcmp(attr, "cshash")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_cshash(self, 0);
    } else if (!strcmp(attr, "template_parameters")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_template_parameters(self, 0);
    } else if (!strcmp(attr, "body")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_body(self, 0);
    } else if (!strcmp(attr, "unpreprocessed_body")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_unpreprocessed_body(self, 0);
    } else if (!strcmp(attr, "declbody")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_declbody(self, 0);
    } else if (!strcmp(attr, "signature")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_signature(self, 0);
    } else if (!strcmp(attr, "declhash")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_declhash(self, 0);
    } else if (!strcmp(attr, "location")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_location(self, 0);
    } else if (!strcmp(attr, "start_loc")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_start_loc(self, 0);
    } else if (!strcmp(attr, "end_loc")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_end_loc(self, 0);
    } else if (!strcmp(attr, "refcount")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_refcount(self, 0);
    } else if (!strcmp(attr, "declcount")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_declcount(self, 0);
    } else if (!strcmp(attr, "literals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_literals(self, 0);
    } else if (!strcmp(attr, "integer_literals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_integer_literals(self, 0);
    } else if (!strcmp(attr, "character_literals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_character_literals(self, 0);
    } else if (!strcmp(attr, "floating_literals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_floating_literals(self, 0);
    } else if (!strcmp(attr, "string_literals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_string_literals(self, 0);
    } else if (!strcmp(attr, "taint")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_taint(self, 0);
    } else if (!strcmp(attr, "calls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_calls(self, 0);
    } else if (!strcmp(attr, "call_info")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_call_info(self, 0);
    } else if (!strcmp(attr, "callrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_callrefs(self, 0);
    } else if (!strcmp(attr, "refcalls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_refcalls(self, 0);
    } else if (!strcmp(attr, "refcall_info")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_refcall_info(self, 0);
    } else if (!strcmp(attr, "refcallrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_refcallrefs(self, 0);
    } else if (!strcmp(attr, "switches")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_switches(self, 0);
    } else if (!strcmp(attr, "csmap")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_csmap(self, 0);
    } else if (!strcmp(attr, "macro_expansions")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_macro_expansions(self, 0);
    } else if (!strcmp(attr, "locals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_locals(self, 0);
    } else if (!strcmp(attr, "derefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_derefs(self, 0);
    } else if (!strcmp(attr, "ifs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_ifs(self, 0);
    } else if (!strcmp(attr, "asm")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_asms(self, 0);
    } else if (!strcmp(attr, "globalrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_globalrefs(self, 0);
    } else if (!strcmp(attr, "globalrefInfo")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_globalrefInfo(self, 0);
    } else if (!strcmp(attr, "funrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_funrefs(self, 0);
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_refs(self, 0);
    } else if (!strcmp(attr, "decls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_decls(self, 0);
    } else if (!strcmp(attr, "types")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_entry_get_types(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_entry_sq_contains(PyObject *self, PyObject *slice) {
    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;
    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "id")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "hash")) {
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
    } else if (!strcmp(attr, "fids")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "mids")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "nargs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "variadic")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "firstNonDeclStmt")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "inline")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->isinline;
    } else if (!strcmp(attr, "template")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->istemplate;
    } else if (!strcmp(attr, "classOuterFn")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->classOuterFn;
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
    } else if (!strcmp(attr, "attributes")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "cshash")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "template_parameters")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->template_parameters;
    } else if (!strcmp(attr, "body")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "unpreprocessed_body")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "declbody")) {
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
    } else if (!strcmp(attr, "start_loc")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "end_loc")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refcount")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "declcount")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "literals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "integer_literals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "character_literals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "floating_literals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "string_literals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "taint")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "calls")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "call_info")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "callrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refcalls")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refcall_info")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refcallrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "switches")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "csmap")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "macro_expansions")) {
        PYASSTR_DECREF(attr);
        return !!__self->entry->macro_expansions;
    } else if (!strcmp(attr, "locals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "derefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "ifs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "asm")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "globalrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "globalrefInfo")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "funrefs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "refs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "decls")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "types")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static PyObject *libftdb_ftdb_func_entry_deepcopy(PyObject *self, PyObject *memo) {
    Py_INCREF(self);
    return (PyObject *)self;
}

static Py_hash_t libftdb_ftdb_func_entry_hash(PyObject *o) {
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)o;
    return __self->index;
}

static PyObject *libftdb_ftdb_func_entry_richcompare(PyObject *self, PyObject *other, int op) {
    if (!PyObject_IsInstance(other, (PyObject *)&libftdb_ftdbFuncsEntryType)) {
        Py_RETURN_FALSE;
    }
    libftdb_ftdb_func_entry_object *__self = (libftdb_ftdb_func_entry_object *)self;
    libftdb_ftdb_func_entry_object *__other = (libftdb_ftdb_func_entry_object *)other;
    Py_RETURN_RICHCOMPARE_internal(__self->index, __other->index, op);
}

PyMethodDef libftdb_ftdbFuncEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_func_entry_json, METH_VARARGS, "Returns the json representation of the ftdb func entry"},
    {"has_namespace", (PyCFunction)libftdb_ftdb_func_entry_has_namespace, METH_VARARGS, "Returns True if func entry contains namespace information"},
    {"has_classOuterFn", (PyCFunction)libftdb_ftdb_func_entry_has_classOuterFn, METH_VARARGS, "Returns True if func entry contains outer function class information"},
    {"has_class", (PyCFunction)libftdb_ftdb_func_entry_has_class, METH_VARARGS, "Returns True if func entry is contained within a class"},
    {"__deepcopy__", (PyCFunction)libftdb_ftdb_func_entry_deepcopy, METH_O, "Deep copy of an object"},
    {NULL, NULL, 0, NULL}
};

PyGetSetDef libftdb_ftdbFuncEntry_getset[] = {
    {"__refcount__", libftdb_ftdb_func_entry_get_object_refcount, 0, "ftdb func entry object reference count", 0},
    {"id", libftdb_ftdb_func_entry_get_id, 0, "ftdb func entry id value", 0},
    {"hash", libftdb_ftdb_func_entry_get_hash, 0, "ftdb func entry hash value", 0},
    {"name", libftdb_ftdb_func_entry_get_name, 0, "ftdb func entry name value", 0},
    {"namespace", libftdb_ftdb_func_entry_get_namespace, 0, "ftdb func entry namespace value", 0},
    {"fid", libftdb_ftdb_func_entry_get_fid, 0, "ftdb func entry fid value", 0},
    {"fids", libftdb_ftdb_func_entry_get_fids, 0, "ftdb func entry fids value", 0},
    {"mids", libftdb_ftdb_func_entry_get_mids, 0, "ftdb func entry mids value", 0},
    {"nargs", libftdb_ftdb_func_entry_get_nargs, 0, "ftdb func entry nargs value", 0},
    {"variadic", libftdb_ftdb_func_entry_get_variadic, 0, "ftdb func entry is variadic value", 0},
    {"firstNonDeclStmt", libftdb_ftdb_func_entry_get_firstNonDeclStmt, 0, "ftdb func entry firstNonDeclStmt value", 0},
    {"inline", libftdb_ftdb_func_entry_get_inline, 0, "ftdb func entry is inline value", 0},
    {"template", libftdb_ftdb_func_entry_get_template, 0, "ftdb func entry is template value", 0},
    {"classOuterFn", libftdb_ftdb_func_entry_get_classOuterFn, 0, "ftdb func entry classOuterFn value", 0},
    {"linkage_string", libftdb_ftdb_func_entry_get_linkage_string, 0, "ftdb func entry linkage string value", 0},
    {"linkage", libftdb_ftdb_func_entry_get_linkage, 0, "ftdb func entry linkage value", 0},
    {"member", libftdb_ftdb_func_entry_get_member, 0, "ftdb func entry is member value", 0},
    {"class", libftdb_ftdb_func_entry_get_class, 0, "ftdb func entry class value", 0},
    {"classid", libftdb_ftdb_func_entry_get_classid, 0, "ftdb func entry classid value", 0},
    {"attributes", libftdb_ftdb_func_entry_get_attributes, 0, "ftdb func entry attributes value", 0},
    {"cshash", libftdb_ftdb_func_entry_get_cshash, 0, "ftdb func entry cshash value", 0},
    {"template_parameters", libftdb_ftdb_func_entry_get_template_parameters, 0, "ftdb func entry template_parameters value", 0},
    {"body", libftdb_ftdb_func_entry_get_body, 0, "ftdb func entry body value", 0},
    {"unpreprocessed_body", libftdb_ftdb_func_entry_get_unpreprocessed_body, 0, "ftdb func entry unpreprocessed_body value", 0},
    {"declbody", libftdb_ftdb_func_entry_get_declbody, 0, "ftdb func entry declbody value", 0},
    {"signature", libftdb_ftdb_func_entry_get_signature, 0, "ftdb func entry signature value", 0},
    {"declhash", libftdb_ftdb_func_entry_get_declhash, 0, "ftdb func entry declhash value", 0},
    {"location", libftdb_ftdb_func_entry_get_location, 0, "ftdb func entry location value", 0},
    {"start_loc", libftdb_ftdb_func_entry_get_start_loc, 0, "ftdb func entry start_loc value", 0},
    {"end_loc", libftdb_ftdb_func_entry_get_end_loc, 0, "ftdb func entry end_loc value", 0},
    {"refcount", libftdb_ftdb_func_entry_get_refcount, 0, "ftdb func entry refcount value", 0},
    {"declcount", libftdb_ftdb_func_entry_get_declcount, 0, "ftdb func entry declcount value", 0},
    {"literals", libftdb_ftdb_func_entry_get_literals, 0, "ftdb func entry literals values", 0},
    {"integer_literals", libftdb_ftdb_func_entry_get_integer_literals, 0, "ftdb func entry integer_literals values", 0},
    {"character_literals", libftdb_ftdb_func_entry_get_character_literals, 0, "ftdb func entry character_literals values", 0},
    {"floating_literals", libftdb_ftdb_func_entry_get_floating_literals, 0, "ftdb func entry floating_literals values", 0},
    {"string_literals", libftdb_ftdb_func_entry_get_string_literals, 0, "ftdb func entry string_literals values", 0},
    {"taint", libftdb_ftdb_func_entry_get_taint, 0, "ftdb func entry taint data", 0},
    {"calls", libftdb_ftdb_func_entry_get_calls, 0, "ftdb func entry calls data", 0},
    {"call_info", libftdb_ftdb_func_entry_get_call_info, 0, "ftdb func entry call_info data", 0},
    {"callrefs", libftdb_ftdb_func_entry_get_callrefs, 0, "ftdb func entry callrefs data", 0},
    {"refcalls", libftdb_ftdb_func_entry_get_refcalls, 0, "ftdb func entry refcalls data", 0},
    {"refcall_info", libftdb_ftdb_func_entry_get_refcall_info, 0, "ftdb func entry refcall_info data", 0},
    {"refcallrefs", libftdb_ftdb_func_entry_get_refcallrefs, 0, "ftdb func entry refcallrefs data", 0},
    {"switches", libftdb_ftdb_func_entry_get_switches, 0, "ftdb func entry switches data", 0},
    {"csmap", libftdb_ftdb_func_entry_get_csmap, 0, "ftdb func entry csmap data", 0},
    {"macro_expansions", libftdb_ftdb_func_entry_get_macro_expansions, 0, "ftdb func entry macro_expansions data", 0},
    {"locals", libftdb_ftdb_func_entry_get_locals, 0, "ftdb func entry locals data", 0},
    {"derefs", libftdb_ftdb_func_entry_get_derefs, 0, "ftdb func entry derefs data", 0},
    {"ifs", libftdb_ftdb_func_entry_get_ifs, 0, "ftdb func entry ifs data", 0},
    {"asms", libftdb_ftdb_func_entry_get_asms, 0, "ftdb func entry asms data", 0},
    {"globalrefs", libftdb_ftdb_func_entry_get_globalrefs, 0, "ftdb func entry globalrefs data", 0},
    {"globalrefInfo", libftdb_ftdb_func_entry_get_globalrefInfo, 0, "ftdb func entry globalrefInfo data", 0},
    {"funrefs", libftdb_ftdb_func_entry_get_funrefs, 0, "ftdb func entry funrefs list", 0},
    {"refs", libftdb_ftdb_func_entry_get_refs, 0, "ftdb func entry refs list", 0},
    {"decls", libftdb_ftdb_func_entry_get_decls, 0, "ftdb func entry decls list", 0},
    {"types", libftdb_ftdb_func_entry_get_types, 0, "ftdb func entry types list", 0},
    {0, 0, 0, 0, 0},
};

PyMappingMethods libftdb_ftdbFuncEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_entry_mp_subscript,
};

PySequenceMethods libftdb_ftdbFuncEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_entry_sq_contains
};

PyTypeObject libftdb_ftdbFuncsEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncEntry",
    .tp_basicsize = sizeof(libftdb_ftdb_func_entry_object),
    .tp_dealloc = (destructor)libftdb_ftdb_func_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncEntry_mapping_methods,
    .tp_hash = (hashfunc)libftdb_ftdb_func_entry_hash,
    .tp_doc = "libftdb ftdb func entry object",
    .tp_richcompare = libftdb_ftdb_func_entry_richcompare,
    .tp_methods = libftdb_ftdbFuncEntry_methods,
    .tp_getset = libftdb_ftdbFuncEntry_getset,
    .tp_new = libftdb_ftdb_func_entry_new,
};
