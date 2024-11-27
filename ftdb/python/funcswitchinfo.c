#include "pyftdb.h"

static void libftdb_ftdb_func_switchinfo_entry_dealloc(libftdb_ftdb_func_switchinfo_entry_object *self) {
    Py_DecRef((PyObject *)self->func_entry);
    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

static PyObject *libftdb_ftdb_func_switchinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_switchinfo_entry_object *self;

    self = (libftdb_ftdb_func_switchinfo_entry_object *)subtype->tp_alloc(subtype, 0);
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
        if (entry_index >= self->func_entry->funcs->ftdb->funcs[func_index].switches_count) {
            snprintf(errmsg, ERRMSG_BUFFER_SIZE, "switchinfo entry index out of bounds: %lu@%lu\n", func_index, entry_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->entry_index = entry_index;
        self->entry = &self->func_entry->funcs->ftdb->funcs[self->func_index].switches[self->entry_index];
    }

    return (PyObject *)self;
}

static PyObject *libftdb_ftdb_func_switchinfo_entry_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_func_switchinfo_entry_object *__self = (libftdb_ftdb_func_switchinfo_entry_object *)self;
    int written = snprintf(repr, 1024, "<ftdbFuncSwitchInfoEntry object at %lx : ", (uintptr_t)self);
    written += snprintf(repr + written, 1024 - written, "fdx:%lu|edx:%lu>", __self->func_index, __self->entry_index);

    return PyUnicode_FromString(repr);
}

static PyObject *libftdb_ftdb_func_switchinfo_entry_get_condition(PyObject *self, void *closure) {
    libftdb_ftdb_func_switchinfo_entry_object *__self = (libftdb_ftdb_func_switchinfo_entry_object *)self;

    return PyUnicode_FromString(__self->entry->condition);
}

static PyObject *libftdb_ftdb_func_switchinfo_entry_get_cases(PyObject *self, void *closure) {
    libftdb_ftdb_func_switchinfo_entry_object *__self = (libftdb_ftdb_func_switchinfo_entry_object *)self;

    PyObject *cases = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->cases_count; ++i) {
        struct case_info *case_info = &__self->entry->cases[i];
        if (!case_info->rhs) {
            PyObject *py_case_info = PyList_New(0);
            PYLIST_ADD_LONG(py_case_info, case_info->lhs.expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.raw_code);
            PyList_Append(cases, py_case_info);
            Py_DecRef(py_case_info);
        } else {
            PyObject *py_case_info = PyList_New(0);
            PYLIST_ADD_LONG(py_case_info, case_info->lhs.expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.raw_code);
            PYLIST_ADD_LONG(py_case_info, case_info->rhs->expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->rhs->enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->rhs->macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->rhs->raw_code);
            PyList_Append(cases, py_case_info);
            Py_DecRef(py_case_info);
        }
    }
    return cases;
}

static PyObject *libftdb_ftdb_func_switchinfo_entry_mp_subscript(PyObject *self, PyObject *slice) {
    static char errmsg[ERRMSG_BUFFER_SIZE];

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "condition")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_switchinfo_entry_get_condition(self, 0);
    } else if (!strcmp(attr, "cases")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_func_switchinfo_entry_get_cases(self, 0);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

static int libftdb_ftdb_func_switchinfo_entry_sq_contains(PyObject *self, PyObject *key) {
    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "condition")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "cases")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

static PyObject *libftdb_ftdb_func_switchinfo_entry_json(libftdb_ftdb_func_switchinfo_entry_object *self, PyObject *args) {
    libftdb_ftdb_func_switchinfo_entry_object *__self = (libftdb_ftdb_func_switchinfo_entry_object *)self;
    PyObject *py_switchinfo_entry = PyDict_New();

    PyObject *cases = PyList_New(0);
    for (unsigned long i = 0; i < __self->entry->cases_count; ++i) {
        struct case_info *case_info = &__self->entry->cases[i];
        if (!case_info->rhs) {
            PyObject *py_case_info = PyList_New(0);
            PYLIST_ADD_LONG(py_case_info, case_info->lhs.expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.raw_code);
            PyList_Append(cases, py_case_info);
            Py_DecRef(py_case_info);
        } else {
            PyObject *py_case_info = PyList_New(0);
            PYLIST_ADD_LONG(py_case_info, case_info->lhs.expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->lhs.raw_code);
            PYLIST_ADD_LONG(py_case_info, case_info->rhs->expr_value);
            PYLIST_ADD_STRING(py_case_info, case_info->rhs->enum_code);
            PYLIST_ADD_STRING(py_case_info, case_info->rhs->macro_code);
            PYLIST_ADD_STRING(py_case_info, case_info->rhs->raw_code);
            PyList_Append(cases, py_case_info);
            Py_DecRef(py_case_info);
        }
    }

    FTDB_SET_ENTRY_PYOBJECT(py_switchinfo_entry, cases, cases);
    FTDB_SET_ENTRY_STRING(py_switchinfo_entry, condition, __self->entry->condition);

    return py_switchinfo_entry;
}

PyMethodDef libftdb_ftdbFuncSwitchInfoEntry_methods[] = {
    {"json", (PyCFunction)libftdb_ftdb_func_switchinfo_entry_json, METH_VARARGS, "Returns the json representation of the switchinfo entry"},
    {NULL, NULL, 0, NULL}
};

PySequenceMethods libftdb_ftdbFuncSwitchInfoEntry_sequence_methods = {
    .sq_contains = libftdb_ftdb_func_switchinfo_entry_sq_contains
};

PyMappingMethods libftdb_ftdbFuncSwitchInfoEntry_mapping_methods = {
    .mp_subscript = libftdb_ftdb_func_switchinfo_entry_mp_subscript,
};

PyGetSetDef libftdb_ftdbFuncSwitchInfoEntry_getset[] = {
    {"condition", libftdb_ftdb_func_switchinfo_entry_get_condition, 0, "ftdb func switchinfo entry condition value", 0},
    {"cases", libftdb_ftdb_func_switchinfo_entry_get_cases, 0, "ftdb func switchinfo entry cases values", 0},
    {0, 0, 0, 0, 0},
};

PyTypeObject libftdb_ftdbFuncSwitchInfoEntryType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdbFuncSwitchInfoEntry",
    .tp_basicsize = sizeof(libftdb_ftdbFuncSwitchInfoEntryType),
    .tp_dealloc = (destructor)libftdb_ftdb_func_switchinfo_entry_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_func_switchinfo_entry_repr,
    .tp_as_sequence = &libftdb_ftdbFuncSwitchInfoEntry_sequence_methods,
    .tp_as_mapping = &libftdb_ftdbFuncSwitchInfoEntry_mapping_methods,
    .tp_doc = "libftdb ftdbFuncSwitchInfoEntry object",
    .tp_richcompare = libftdb_generic_richcompare,
    .tp_methods = libftdb_ftdbFuncSwitchInfoEntry_methods,
    .tp_getset = libftdb_ftdbFuncSwitchInfoEntry_getset,
    .tp_new = libftdb_ftdb_func_switchinfo_entry_new,
};
