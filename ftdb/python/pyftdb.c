#include "pyftdb.h"
#include "ftdb_entry.h"
#include "recipe.h"

pthread_mutex_t unflatten_lock = PTHREAD_MUTEX_INITIALIZER;
struct rb_root ftdb_image_map;

PyObject *libftdb_ftdbError;

PyObject *libftdb_parse_c_fmt_string(PyObject *self, PyObject *args) {
    const char *fmt;
    if (!PyArg_ParseTuple(args, "s", &fmt))
        return NULL;

    enum format_type *par_type = malloc(256 * sizeof(enum format_type));
    int n = vsnprintf_parse_format(&par_type, 256, fmt);

    if (n < 0) {
        free(par_type);
        Py_RETURN_NONE;
    }

    PyObject* argv = PyList_New(0);
    for (int i = 0; i < n; ++i) {
        enum format_type v = par_type[i];
        PyList_Append(argv, PyLong_FromLong((long) v));
    }

    free(par_type);
    return argv;
}

void libftdb_ftdb_dealloc(libftdb_ftdb_object *self) {
    PyTypeObject *tp = Py_TYPE(self);
    if (self->init_done) {
        int mutex_err = pthread_mutex_lock(&unflatten_lock);
        if (mutex_err) {
            printf("libftdb_ftdb_dealloc(): failed to lock mutex - %d\n", mutex_err);
        } else {
            struct ftdb_ref *ftdb_ref = (struct ftdb_ref *)self->ftdb_image_map_node->value;
            if (--ftdb_ref->refcount == 0) {
                /* No more ftdb objects are holding this image file */
                stringRefMap_remove(&ftdb_image_map, self->ftdb_image_map_node);
                free((void *)self->ftdb_image_map_node->value);
                free((void *)self->ftdb_image_map_node);
                unflatten_deinit(self->unflatten);
            }
            pthread_mutex_unlock(&unflatten_lock);
        }
    }
    tp->tp_free(self);
}

PyObject *libftdb_ftdb_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {
    libftdb_ftdb_object *self;

    self = (libftdb_ftdb_object *)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        /* Use the 'load' function (or constructor) to initialize the cache */
        self->init_done = 0;
        self->ftdb = 0;
    }

    return (PyObject *)self;
}

PyObject *libftdb_ftdb_load(libftdb_ftdb_object *self, PyObject *args, PyObject *kwargs) {
    static char *kwlist[] = {"filename", "quiet", "debug", NULL};
    const char *cache_filename = "db.img";
    int debug = self->debug;
    int quiet = 0;
    bool err = true;

    if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|s$pp", kwlist, &cache_filename, &quiet, &debug))
        return NULL; /* Exception is already set by PyArg_ParseTupleAndKeywords */

    DBG(debug, "--- libftdb_ftdb_load(\"%s\")\n", cache_filename);

    if (self->init_done) {
        PyErr_SetString(libftdb_ftdbError, "ftdb cache already initialized");
        goto done;
    }

    int mutex_err = pthread_mutex_lock(&unflatten_lock);
    if (mutex_err) {
        printf("libftdb_ftdb_load(): failed to lock mutex - %d\n", mutex_err);
        PyErr_SetString(libftdb_ftdbError, "Loading ftdb cache failed");
        goto done;
    }

    struct stringRefMap_node *node = stringRefMap_search(&ftdb_image_map, cache_filename);
    if (node) {
        /* The cache database for the requested filename is already present and loaded into the process memory */
        self->ftdb = (const struct ftdb *)((struct ftdb_ref *)node->value)->ftdb;
        self->ftdb_image_map_node = node;
        ((struct ftdb_ref *)node->value)->refcount++;
        DBG(true, "Re-used loaded input file\n");
    } else {
        /* Try to load new cache database file */
        FILE *in = fopen(cache_filename, "r+b");
        if (!in) {
            in = fopen(cache_filename, "rb");
            if (!in) {
                PyErr_Format(libftdb_ftdbError, "Cannot open cache file - (%d) %s", errno, strerror(errno));
                goto done;
            }
        }

        int debug_level = 0;
        if (debug)
            debug_level = 2;
        else if (!quiet)
            debug_level = 1;

        self->unflatten = unflatten_init(debug_level);
        if (self->unflatten == NULL) {
            PyErr_SetString(libftdb_ftdbError, "Failed to intialize unflatten library");
            goto done;
        }

        UnflattenStatus status = unflatten_load_continuous(self->unflatten, in, NULL);
        if (status) {
            PyErr_Format(libftdb_ftdbError, "Failed to read cache file: %s\n", unflatten_explain_status(status));
            unflatten_deinit(self->unflatten);
            goto done;
        }
        fclose(in);

        self->ftdb = (const struct ftdb *)unflatten_root_pointer_next(self->unflatten);

        /* Check whether it's correct file and in supported version */
        if (self->ftdb->db_magic != FTDB_MAGIC_NUMBER) {
            PyErr_Format(libftdb_ftdbError, "Failed to parse cache file - invalid magic %p", self->ftdb->db_magic);
            unflatten_deinit(self->unflatten);
            goto done;
        }
        if (self->ftdb->db_version != FTDB_VERSION) {
            PyErr_Format(libftdb_ftdbError, "Failed to parse cache file - unsupported image version %p (required: %p)", self->ftdb->db_version, FTDB_VERSION);
            unflatten_deinit(self->unflatten);
            goto done;
        }

        struct ftdb_ref *ftdb_ref = malloc(sizeof(struct ftdb_ref));
        ftdb_ref->ftdb = self->ftdb;
        ftdb_ref->refcount = 1;
        self->ftdb_image_map_node = stringRefMap_insert(&ftdb_image_map, cache_filename, (unsigned long)ftdb_ref);
    }

    self->init_done = 1;
    err = false;

done:
    pthread_mutex_unlock(&unflatten_lock);
    if (err) {
        self->ftdb = self->unflatten = NULL;
        return NULL; /* Indicate that exception has been set */
    }
    Py_RETURN_TRUE;
}

int libftdb_ftdb_init(libftdb_ftdb_object *self, PyObject *args, PyObject *kwds) {
    if (PyTuple_Size(args) > 0) {
        (void)libftdb_ftdb_load(self, args, kwds);
    }

    return 0;
}

PyObject *libftdb_ftdb_repr(PyObject *self) {
    static char repr[1024];

    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    const char *inited = "initialized";
    const char *ninited = "not initialized";
    int written = snprintf(repr, 1024, "<ftdb object at %lx : %s", (uintptr_t)self, __self->init_done ? inited : ninited);
    if (__self->init_done) {
        written += snprintf(repr + written, 1024 - written, "|debug:%d;verbose:%d|", __self->debug, __self->verbose);
        written += snprintf(repr + written, 1024 - written, "funcs_count:%lu;types_count:%lu;global_count:%lu;sources:%lu>", __self->ftdb->funcs_count, __self->ftdb->types_count, __self->ftdb->globals_count, __self->ftdb->sourceindex_table_count);
    } else {
        written += snprintf(repr + written, 1024 - written, ">");
    }

    return PyUnicode_FromString(repr);
}

int libftdb_ftdb_bool(PyObject *self) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    return __self->init_done;
}

PyObject *libftdb_ftdb_get_refcount(PyObject *self, void *closure) {
    return PyLong_FromLong(self->ob_refcnt);
}

PyObject *libftdb_ftdb_get_version(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    return PyUnicode_FromString(__self->ftdb->version);
}

PyObject *libftdb_ftdb_get_module(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    return PyUnicode_FromString(__self->ftdb->module);
}

PyObject *libftdb_ftdb_get_directory(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    return PyUnicode_FromString(__self->ftdb->directory);
}

PyObject *libftdb_ftdb_get_release(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    return PyUnicode_FromString(__self->ftdb->release);
}

PyObject *libftdb_ftdb_get_sources(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PyTuple_SetItem(args, 0, PyLong_FromLong((uintptr_t)__self->ftdb));
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *sources = PyObject_CallObject((PyObject *)&libftdb_ftdbSourcesType, args);
    Py_DECREF(args);
    return sources;
}

PyObject *libftdb_ftdb_get_sources_as_dict(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *sources = PyList_New(0);
    for (unsigned long i = 0; i < __self->ftdb->sourceindex_table_count; ++i) {
        PyObject *py_sources_entry = PyDict_New();
        FTDB_SET_ENTRY_ULONG(py_sources_entry, id, i);
        FTDB_SET_ENTRY_STRING(py_sources_entry, name, __self->ftdb->sourceindex_table[i]);
        PyList_Append(sources, py_sources_entry);
        Py_DecRef(py_sources_entry);
    }
    return sources;
}

PyObject *libftdb_ftdb_get_sources_as_list(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;

    PyObject *sources = PyList_New(0);
    for (unsigned long i = 0; i < __self->ftdb->sourceindex_table_count; ++i) {
        PyObject *py_sources_entry = PyDict_New();
        PyObject *key = PyUnicode_FromString(__self->ftdb->sourceindex_table[i]);
        PyObject *val = PyLong_FromUnsignedLong(i);
        PyDict_SetItem(py_sources_entry, key, val);
        Py_DecRef(key);
        Py_DecRef(val);
        PyList_Append(sources, py_sources_entry);
        Py_DecRef(py_sources_entry);
    }
    return sources;
}

PyObject *libftdb_ftdb_get_modules(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PyTuple_SetItem(args, 0, PyLong_FromLong((uintptr_t)__self->ftdb));
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *modules = PyObject_CallObject((PyObject *)&libftdb_ftdbModulesType, args);
    Py_DECREF(args);
    return modules;
}

PyObject *libftdb_ftdb_get_modules_as_dict(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *modules = PyList_New(0);
    for (unsigned long i = 0; i < __self->ftdb->moduleindex_table_count; ++i) {
        PyObject *py_modules_entry = PyDict_New();
        FTDB_SET_ENTRY_ULONG(py_modules_entry, id, i);
        FTDB_SET_ENTRY_STRING(py_modules_entry, name, __self->ftdb->moduleindex_table[i]);
        PyList_Append(modules, py_modules_entry);
        Py_DecRef(py_modules_entry);
    }
    return modules;
}

PyObject *libftdb_ftdb_get_modules_as_list(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;

    PyObject *modules = PyList_New(0);
    for (unsigned long i = 0; i < __self->ftdb->moduleindex_table_count; ++i) {
        PyObject *py_modules_entry = PyDict_New();
        PyObject *key = PyUnicode_FromString(__self->ftdb->moduleindex_table[i]);
        PyObject *val = PyLong_FromUnsignedLong(i);
        PyDict_SetItem(py_modules_entry, key, val);
        Py_DecRef(key);
        Py_DecRef(val);
        PyList_Append(modules, py_modules_entry);
        Py_DecRef(py_modules_entry);
    }
    return modules;
}

PyObject *libftdb_ftdb_get_funcs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->ftdb);
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *funcs = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncsType, args);
    Py_DECREF(args);
    return funcs;
}

PyObject *libftdb_ftdb_get_funcdecls(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->ftdb);
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *funcdecls = PyObject_CallObject((PyObject *)&libftdb_ftdbFuncdeclsType, args);
    Py_DECREF(args);
    return funcdecls;
}

PyObject *libftdb_ftdb_get_unresolvedfuncs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->ftdb);
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *unresolvedfuncs = PyObject_CallObject((PyObject *)&libftdb_ftdbUnresolvedfuncsType, args);
    Py_DECREF(args);
    return unresolvedfuncs;
}

PyObject *libftdb_ftdb_get_unresolvedfuncs_as_list(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;

    PyObject *unresolvedfuncs = PyList_New(0);
    for (unsigned long i = 0; i < __self->ftdb->unresolvedfuncs_count; ++i) {
        struct ftdb_unresolvedfunc_entry *entry = &__self->ftdb->unresolvedfuncs[i];
        PyObject *py_unresolvedfuncs_entry = PyDict_New();
        FTDB_SET_ENTRY_STRING(py_unresolvedfuncs_entry, name, entry->name);
        FTDB_SET_ENTRY_ULONG(py_unresolvedfuncs_entry, id, entry->id);
        PyList_Append(unresolvedfuncs, py_unresolvedfuncs_entry);
        Py_DECREF(py_unresolvedfuncs_entry);
    }
    return unresolvedfuncs;
}

PyObject *libftdb_ftdb_get_globals(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->ftdb);
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *globals = PyObject_CallObject((PyObject *)&libftdb_ftdbGlobalsType, args);
    Py_DECREF(args);
    return globals;
}

PyObject *libftdb_ftdb_get_types(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->ftdb);
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *types = PyObject_CallObject((PyObject *)&libftdb_ftdbTypesType, args);
    Py_DECREF(args);
    return types;
}

PyObject *libftdb_ftdb_get_fops(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    PyObject *args = PyTuple_New(2);
    PYTUPLE_SET_ULONG(args, 0, (uintptr_t)__self->ftdb);
    PYTUPLE_SET_ULONG(args, 1, (uintptr_t)self);
    PyObject *fops = PyObject_CallObject((PyObject *)&libftdb_ftdbFopsType, args);
    Py_DECREF(args);
    return fops;
}

PyObject *libftdb_ftdb_get_matrix_data_as_dict(struct matrix_data *matrix_data) {
    PyObject *md = PyList_New(0);

    PyObject *data = PyDict_New();
    FTDB_SET_ENTRY_STRING(data, name, "data");
    FTDB_SET_ENTRY_ULONG_ARRAY(data, data, matrix_data->data);
    PyList_Append(md, data);
    Py_DecRef(data);
    PyObject *row_ind = PyDict_New();
    FTDB_SET_ENTRY_STRING(row_ind, name, "row_ind");
    FTDB_SET_ENTRY_ULONG_ARRAY(row_ind, data, matrix_data->row_ind);
    PyList_Append(md, row_ind);
    Py_DecRef(row_ind);
    PyObject *col_ind = PyDict_New();
    FTDB_SET_ENTRY_STRING(col_ind, name, "col_ind");
    FTDB_SET_ENTRY_ULONG_ARRAY(col_ind, data, matrix_data->col_ind);
    PyList_Append(md, col_ind);
    Py_DecRef(col_ind);
    PyObject *matrix_size = PyDict_New();
    FTDB_SET_ENTRY_STRING(matrix_size, name, "matrix_size");
    FTDB_SET_ENTRY_ULONG(matrix_size, data, matrix_data->matrix_size);
    PyList_Append(md, matrix_size);
    Py_DecRef(matrix_size);

    return md;
}

PyObject *libftdb_ftdb_get_funcs_tree_calls_no_asm(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_calls_no_asm) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_calls_no_asm);
}

PyObject *libftdb_ftdb_get_funcs_tree_calls_no_known(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_calls_no_known) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_calls_no_known);
}

PyObject *libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_calls_no_known_no_asm) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_calls_no_known_no_asm);
}

PyObject *libftdb_ftdb_get_funcs_tree_func_calls(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_func_calls) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_func_calls);
}

PyObject *libftdb_ftdb_get_funcs_tree_func_refs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_func_refs) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_func_refs);
}

PyObject *libftdb_ftdb_get_funcs_tree_funrefs_no_asm(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_funrefs_no_asm) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_funrefs_no_asm);
}

PyObject *libftdb_ftdb_get_funcs_tree_funrefs_no_known(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_funrefs_no_known) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_funrefs_no_known);
}

PyObject *libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->funcs_tree_funrefs_no_known_no_asm) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_funrefs_no_known_no_asm);
}

PyObject *libftdb_ftdb_get_globs_tree_globalrefs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->globs_tree_globalrefs) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->globs_tree_globalrefs);
}

PyObject *libftdb_ftdb_get_types_tree_refs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->types_tree_refs) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->types_tree_refs);
}

PyObject *libftdb_ftdb_get_types_tree_usedrefs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->types_tree_usedrefs) {
        Py_RETURN_NONE;
    }

    return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->types_tree_usedrefs);
}

PyObject *libftdb_ftdb_get_static_funcs_map(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->static_funcs_map) {
        Py_RETURN_NONE;
    }

    PyObject *sfm = PyList_New(0);

    for (Py_ssize_t i = 0; i < __self->ftdb->static_funcs_map_count; ++i) {
        struct func_map_entry *entry = &__self->ftdb->static_funcs_map[i];
        PyObject *data = PyDict_New();
        FTDB_SET_ENTRY_ULONG(data, id, entry->id);
        FTDB_SET_ENTRY_ULONG_ARRAY(data, fids, entry->fids);
        PYLIST_ADD_PYOBJECT(sfm, data);
    }

    return sfm;
}

PyObject *libftdb_ftdb_get_single_item_init_data(struct init_data_item *item) {
    PyObject *py_item = PyDict_New();
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_item, id, item->id);
    FTDB_SET_ENTRY_STRING_ARRAY(py_item, name, item->name);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_item, size, item->size);
    if (item->size_dep) {
        PyObject *py_size_dep = PyDict_New();
        FTDB_SET_ENTRY_ULONG(py_size_dep, id, item->size_dep->id);
        FTDB_SET_ENTRY_INT64(py_size_dep, add, item->size_dep->add);
        FTDB_SET_ENTRY_PYOBJECT(py_item, size_dep, py_size_dep);
    }
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, nullterminated, item->nullterminated);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, tagged, item->tagged);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, fuzz, item->fuzz);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, pointer, item->pointer);
    FTDB_SET_ENTRY_INT64_OPTIONAL(py_item, max_value, item->max_value);
    FTDB_SET_ENTRY_INT64_OPTIONAL(py_item, min_value, item->min_value);
    FTDB_SET_ENTRY_INT64_OPTIONAL(py_item, value, item->value);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, user_name, item->user_name);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, protected, item->protected_var);
    FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(py_item, value_dep, item->value_dep);
    FTDB_SET_ENTRY_ULONG_OPTIONAL(py_item, fuzz_offset, item->fuzz_offset);
    FTDB_SET_ENTRY_STRING_OPTIONAL(py_item, force_type, item->force_type);
    FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(py_item, always_init, item->always_init);
    if (item->subitems) {
        PyObject *py_subitems = PyList_New(0);
        for (Py_ssize_t i = 0; i < item->subitems_count; ++i) {
            struct init_data_item *subitem = &item->subitems[i];
            PyObject *py_subitem = libftdb_ftdb_get_single_item_init_data(subitem);
            PYLIST_ADD_PYOBJECT(py_subitems, py_subitem);
        }
        FTDB_SET_ENTRY_PYOBJECT(py_item, subitems, py_subitems);
    }

    return py_item;
}

PyObject *libftdb_ftdb_get_init_data(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->init_data) {
        Py_RETURN_NONE;
    }

    PyObject *init_data = PyList_New(0);
    for (Py_ssize_t i = 0; i < __self->ftdb->init_data_count; ++i) {
        struct init_data_entry *entry = &__self->ftdb->init_data[i];
        PyObject *py_init_data_entry = PyDict_New();
        FTDB_SET_ENTRY_STRING(py_init_data_entry, name, entry->name);
        FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_init_data_entry, order, entry->order);
        FTDB_SET_ENTRY_STRING(py_init_data_entry, interface, entry->interface);
        PyObject *py_items = PyList_New(0);
        for (Py_ssize_t j = 0; j < entry->items_count; ++j) {
            struct init_data_item *item = &entry->items[j];
            PyObject *py_item = libftdb_ftdb_get_single_item_init_data(item);
            PYLIST_ADD_PYOBJECT(py_items, py_item);
        }
        FTDB_SET_ENTRY_PYOBJECT(py_init_data_entry, items, py_items);
        PYLIST_ADD_PYOBJECT(init_data, py_init_data_entry);
    }

    return init_data;
}

PyObject *libftdb_ftdb_get_known_data(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->known_data) {
        Py_RETURN_NONE;
    }

    PyObject *py_known_data_container = PyList_New(0);
    PyObject *py_known_data = PyDict_New();
    FTDB_SET_ENTRY_STRING(py_known_data, version, __self->ftdb->known_data->version);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data, func_ids, __self->ftdb->known_data->func_ids);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data, builtin_ids, __self->ftdb->known_data->builtin_ids);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data, asm_ids, __self->ftdb->known_data->asm_ids);
    FTDB_SET_ENTRY_STRING_ARRAY(py_known_data, lib_funcs, __self->ftdb->known_data->lib_funcs);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data, lib_funcs_ids, __self->ftdb->known_data->lib_funcs_ids);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data, always_inc_funcs_ids, __self->ftdb->known_data->always_inc_funcs_ids);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data, replacement_ids, __self->ftdb->known_data->replacement_ids);
    PYLIST_ADD_PYOBJECT(py_known_data_container, py_known_data);

    return py_known_data_container;
}

PyObject *libftdb_ftdb_get_BAS(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->BAS_data) {
        Py_RETURN_NONE;
    }

    PyObject *py_BAS = PyList_New(0);
    for (Py_ssize_t i = 0; i < __self->ftdb->BAS_data_count; ++i) {
        struct BAS_item *entry = &__self->ftdb->BAS_data[i];
        PyObject *py_BAS_entry = PyDict_New();
        FTDB_SET_ENTRY_STRING(py_BAS_entry, loc, entry->loc);
        FTDB_SET_ENTRY_STRING_ARRAY(py_BAS_entry, entries, entry->entries);
        PYLIST_ADD_PYOBJECT(py_BAS, py_BAS_entry);
    }

    return py_BAS;
}

PyObject *libftdb_ftdb_get_func_fptrs(PyObject *self, void *closure) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!__self->ftdb->func_fptrs_data) {
        Py_RETURN_NONE;
    }

    PyObject *py_func_fptrs = PyList_New(0);
    for (Py_ssize_t i = 0; i < __self->ftdb->func_fptrs_data_count; ++i) {
        struct func_fptrs_item *entry = &__self->ftdb->func_fptrs_data[i];
        PyObject *py_func_fptrs_entry = PyDict_New();
        FTDB_SET_ENTRY_ULONG(py_func_fptrs_entry, _id, entry->id);
        PyObject *py_entries = PyList_New(0);
        for (Py_ssize_t j = 0; j < entry->entries_count; ++j) {
            struct func_fptrs_entry *item = &entry->entries[j];
            PyObject *py_entry = PyTuple_New(2);
            PYTUPLE_SET_STR(py_entry, 0, item->fname);
            PyObject *py_ids = PyList_New(0);
            for (Py_ssize_t k = 0; k < item->ids_count; ++k) {
                PYLIST_ADD_ULONG(py_ids, item->ids[k]);
            }
            PYTUPLE_SET_PYOBJECT(py_entry, 1, py_ids);
            PYLIST_ADD_PYOBJECT(py_entries, py_entry);
        }
        FTDB_SET_ENTRY_PYOBJECT(py_func_fptrs_entry, entries, py_entries);
        PYLIST_ADD_PYOBJECT(py_func_fptrs, py_func_fptrs_entry);
    }

    return py_func_fptrs;
}

PyObject *libftdb_ftdb_mp_subscript(PyObject *self, PyObject *slice) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    static char errmsg[ERRMSG_BUFFER_SIZE];

    FTDB_MODULE_INIT_CHECK;

    if (!PyUnicode_Check(slice)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
        Py_RETURN_NONE;
    }

    const char *attr = PyString_get_c_str(slice);

    if (!strcmp(attr, "version")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_version(self, 0);
    } else if (!strcmp(attr, "module")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_module(self, 0);
    } else if (!strcmp(attr, "directory")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_directory(self, 0);
    } else if (!strcmp(attr, "release")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_release(self, 0);
    } else if (!strcmp(attr, "sources")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_sources_as_list(self, 0);
    } else if (!strcmp(attr, "source_info")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_sources_as_dict(self, 0);
    } else if (!strcmp(attr, "modules")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_modules_as_list(self, 0);
    } else if (!strcmp(attr, "module_info")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_modules_as_dict(self, 0);
    } else if (!strcmp(attr, "funcs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs(self, 0);
    } else if (!strcmp(attr, "funcdecls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcdecls(self, 0);
    } else if (!strcmp(attr, "unresolvedfuncs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_unresolvedfuncs_as_list(self, 0);
    } else if (!strcmp(attr, "globals")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_globals(self, 0);
    } else if (!strcmp(attr, "types")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_types(self, 0);
    } else if (!strcmp(attr, "fops")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_fops(self, 0);
    } else if (!strcmp(attr, "funcs_tree_calls_no_asm")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_calls_no_asm(self, 0);
    } else if (!strcmp(attr, "funcs_tree_calls_no_known")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_calls_no_known(self, 0);
    } else if (!strcmp(attr, "funcs_tree_calls_no_known_no_asm")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm(self, 0);
    } else if (!strcmp(attr, "funcs_tree_func_calls")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_func_calls(self, 0);
    } else if (!strcmp(attr, "funcs_tree_func_refs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_func_refs(self, 0);
    } else if (!strcmp(attr, "funcs_tree_funrefs_no_asm")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_funrefs_no_asm(self, 0);
    } else if (!strcmp(attr, "funcs_tree_funrefs_no_known")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_funrefs_no_known(self, 0);
    } else if (!strcmp(attr, "funcs_tree_funrefs_no_known_no_asm")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm(self, 0);
    } else if (!strcmp(attr, "globs_tree_globalrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_globs_tree_globalrefs(self, 0);
    } else if (!strcmp(attr, "types_tree_refs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_types_tree_refs(self, 0);
    } else if (!strcmp(attr, "types_tree_usedrefs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_types_tree_usedrefs(self, 0);
    } else if (!strcmp(attr, "static_funcs_map")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_static_funcs_map(self, 0);
    } else if (!strcmp(attr, "init_data")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_init_data(self, 0);
    } else if (!strcmp(attr, "known_data")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_known_data(self, 0);
    } else if (!strcmp(attr, "BAS")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_BAS(self, 0);
    } else if (!strcmp(attr, "func_fptrs")) {
        PYASSTR_DECREF(attr);
        return libftdb_ftdb_get_func_fptrs(self, 0);
    } else if (!strcmp(attr, "funcdecln")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->funcdecls_count);
    } else if (!strcmp(attr, "typen")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->types_count);
    } else if (!strcmp(attr, "sourcen")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->sourceindex_table_count);
    } else if (!strcmp(attr, "globaln")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->globals_count);
    } else if (!strcmp(attr, "unresolvedfuncn")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->unresolvedfuncs_count);
    } else if (!strcmp(attr, "funcn")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->funcs_count);
    } else if (!strcmp(attr, "fopn")) {
        PYASSTR_DECREF(attr);
        return PyLong_FromUnsignedLong(__self->ftdb->fops_count);
    }

    snprintf(errmsg, ERRMSG_BUFFER_SIZE, "Invalid attribute name: %s", attr);
    PyErr_SetString(libftdb_ftdbError, errmsg);
    PYASSTR_DECREF(attr);
    Py_RETURN_NONE;
}

int libftdb_ftdb_sq_contains(PyObject *self, PyObject *key) {
    libftdb_ftdb_object *__self = (libftdb_ftdb_object *)self;
    FTDB_MODULE_INIT_CHECK;

    if (!PyUnicode_Check(key)) {
        PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
        return 0;
    }

    const char *attr = PyString_get_c_str(key);

    if (!strcmp(attr, "version")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "module")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "directory")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "release")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "sources")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->sourceindex_table;
    } else if (!strcmp(attr, "source_info")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->sourceindex_table;
    } else if (!strcmp(attr, "modules")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->moduleindex_table;
    } else if (!strcmp(attr, "module_info")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->moduleindex_table;
    } else if (!strcmp(attr, "funcs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "funcdecls")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "unresolvedfuncs")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "globals")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "types")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "fops")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "funcs_tree_calls_no_asm")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_calls_no_asm;
    } else if (!strcmp(attr, "funcs_tree_calls_no_known")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_calls_no_known;
    } else if (!strcmp(attr, "funcs_tree_calls_no_known_no_asm")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_calls_no_known_no_asm;
    } else if (!strcmp(attr, "funcs_tree_func_calls")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_func_calls;
    } else if (!strcmp(attr, "funcs_tree_func_refs")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_func_refs;
    } else if (!strcmp(attr, "funcs_tree_funrefs_no_asm")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_funrefs_no_asm;
    } else if (!strcmp(attr, "funcs_tree_funrefs_no_known")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_funrefs_no_known;
    } else if (!strcmp(attr, "funcs_tree_funrefs_no_known_no_asm")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->funcs_tree_funrefs_no_known_no_asm;
    } else if (!strcmp(attr, "globs_tree_globalrefs")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->globs_tree_globalrefs;
    } else if (!strcmp(attr, "types_tree_refs")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->types_tree_refs;
    } else if (!strcmp(attr, "types_tree_usedrefs")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->types_tree_usedrefs;
    } else if (!strcmp(attr, "static_funcs_map")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->static_funcs_map;
    } else if (!strcmp(attr, "init_data")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->init_data;
    } else if (!strcmp(attr, "known_data")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->known_data;
    } else if (!strcmp(attr, "BAS")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->BAS_data;
    } else if (!strcmp(attr, "func_fptrs")) {
        PYASSTR_DECREF(attr);
        return !!__self->ftdb->func_fptrs_data;
    } else if (!strcmp(attr, "funcdecln")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "typen")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "sourcen")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "globaln")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "unresolvedfuncn")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "funcn")) {
        PYASSTR_DECREF(attr);
        return 1;
    } else if (!strcmp(attr, "fopn")) {
        PYASSTR_DECREF(attr);
        return 1;
    }

    PYASSTR_DECREF(attr);
    return 0;
}

PyObject *libftdb_ftdb_get_BAS_item_by_loc(libftdb_ftdb_object *self, PyObject *args, PyObject *kwargs) {
    const char *loc;
    if (!PyArg_ParseTuple(args, "s", &loc))
        return NULL;

    struct stringRef_entryMap_node *node = stringRef_entryMap_search(&self->ftdb->BAS_data_index, loc);
    if (!node) {
        Py_RETURN_NONE;
    }

    struct BAS_item *entry = (struct BAS_item *)node->entry;
    PyObject *py_BAS_entry = PyDict_New();
    FTDB_SET_ENTRY_STRING(py_BAS_entry, loc, entry->loc);
    FTDB_SET_ENTRY_STRING_ARRAY(py_BAS_entry, entries, entry->entries);
    return py_BAS_entry;
}

PyObject *libftdb_ftdb_get_func_map_entry__by_id(libftdb_ftdb_object *self, PyObject *args, PyObject *kwargs) {
    unsigned long id;
    if (!PyArg_ParseTuple(args, "k", &id))
        return NULL;

    struct ulong_entryMap_node *node = ulong_entryMap_search(&self->ftdb->static_funcs_map_index, id);
    if (!node) {
        Py_RETURN_NONE;
    }

    struct func_map_entry *entry = (struct func_map_entry *)node->entry;
    PyObject *py_func_map_entry = PyDict_New();
    FTDB_SET_ENTRY_ULONG(py_func_map_entry, id, entry->id);
    FTDB_SET_ENTRY_ULONG_ARRAY(py_func_map_entry, fids, entry->fids);
    return py_func_map_entry;
}

/* TODO: memory leaks */
static void libftdb_create_ftdb_func_entry(PyObject *self, PyObject *func_entry, struct ftdb_func_entry *new_entry) {
    new_entry->name = FTDB_ENTRY_STRING(func_entry, name);
    new_entry->__namespace = FTDB_ENTRY_STRING_OPTIONAL(func_entry, namespace);
    new_entry->id = FTDB_ENTRY_ULONG(func_entry, id);
    new_entry->fid = FTDB_ENTRY_ULONG(func_entry, fid);
    new_entry->fids_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, fids);
    new_entry->fids = FTDB_ENTRY_ULONG_ARRAY(func_entry, fids);
    new_entry->mids_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(func_entry, mids);
    new_entry->mids = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(func_entry, mids);
    new_entry->nargs = FTDB_ENTRY_ULONG(func_entry, nargs);
    new_entry->isvariadic = FTDB_ENTRY_BOOL(func_entry, variadic);
    new_entry->firstNonDeclStmt = FTDB_ENTRY_STRING(func_entry, firstNonDeclStmt);
    new_entry->isinline = FTDB_ENTRY_BOOL_OPTIONAL(func_entry, inline);
    new_entry->istemplate = FTDB_ENTRY_BOOL_OPTIONAL(func_entry, template);
    new_entry->classOuterFn = FTDB_ENTRY_STRING_OPTIONAL(func_entry, classOuterFn);
    new_entry->linkage = FTDB_ENTRY_ENUM_FROM_STRING(func_entry, linkage, functionLinkage);
    new_entry->ismember = FTDB_ENTRY_BOOL_OPTIONAL(func_entry, member);
    new_entry->__class = FTDB_ENTRY_STRING_OPTIONAL(func_entry, class);
    new_entry->classid = FTDB_ENTRY_ULONG_OPTIONAL(func_entry, classid);
    new_entry->attributes_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, attributes);
    new_entry->attributes = FTDB_ENTRY_STRING_ARRAY(func_entry, attributes);
    new_entry->hash = FTDB_ENTRY_STRING(func_entry, hash);
    new_entry->cshash = FTDB_ENTRY_STRING(func_entry, cshash);
    new_entry->template_parameters = FTDB_ENTRY_STRING_OPTIONAL(func_entry, template_parameters);
    new_entry->body = FTDB_ENTRY_STRING(func_entry, body);
    new_entry->unpreprocessed_body = FTDB_ENTRY_STRING(func_entry, unpreprocessed_body);
    new_entry->declbody = FTDB_ENTRY_STRING(func_entry, declbody);
    new_entry->signature = FTDB_ENTRY_STRING(func_entry, signature);
    new_entry->declhash = FTDB_ENTRY_STRING(func_entry, declhash);
    new_entry->location = FTDB_ENTRY_STRING(func_entry, location);
    new_entry->start_loc = FTDB_ENTRY_STRING(func_entry, start_loc);
    new_entry->end_loc = FTDB_ENTRY_STRING(func_entry, end_loc);
    new_entry->refcount = FTDB_ENTRY_ULONG(func_entry, refcount);

    PyObject *key_literals = PyUnicode_FromString("literals");
    PyObject *literals = PyDict_GetItem(func_entry, key_literals);
    Py_DecRef(key_literals);
    new_entry->integer_literals = FTDB_ENTRY_INT64_ARRAY(literals, integer);
    new_entry->integer_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, integer);
    new_entry->character_literals = FTDB_ENTRY_UINT_ARRAY(literals, character);
    new_entry->character_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, character);
    new_entry->floating_literals = FTDB_ENTRY_FLOAT_ARRAY(literals, floating);
    new_entry->floating_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, floating);
    new_entry->string_literals = FTDB_ENTRY_STRING_ARRAY(literals, string);
    new_entry->string_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, string);
    new_entry->declcount = FTDB_ENTRY_ULONG(func_entry, declcount);

    PyObject *key_taint = PyUnicode_FromString("taint");
    PyObject *taint = PyDict_GetItem(func_entry, key_taint);
    Py_DecRef(key_taint);
    PyObject *taint_keys = PyDict_Keys(taint);
    new_entry->taint_count = PyDict_Size(taint);
    new_entry->taint = calloc(new_entry->taint_count, sizeof(struct taint_data));
    for (Py_ssize_t i = 0; i < PyList_Size(taint_keys); ++i) {
        PyObject *py_int_key = PyLong_FromString((char *)PyString_get_c_str(PyList_GetItem(taint_keys, i)), 0, 10);
        if (py_int_key) {
            PyObject *taint_tuple_list = PyDict_GetItem(taint, PyList_GetItem(taint_keys, i));
            unsigned long integer_key = PyLong_AsUnsignedLong(py_int_key);
            struct taint_data *taint_data = &new_entry->taint[integer_key];
            taint_data->taint_list_count = PyList_Size(taint_tuple_list);
            taint_data->taint_list = calloc(taint_data->taint_list_count, sizeof(struct taint_element));
            for (Py_ssize_t j = 0; j < PyList_Size(taint_tuple_list); ++j) {
                PyObject *taint_tuple_as_list = PyList_GetItem(taint_tuple_list, j);
                taint_data->taint_list[j].taint_level = PyLong_AsUnsignedLong(PyList_GetItem(taint_tuple_as_list, 0));
                taint_data->taint_list[j].varid = PyLong_AsUnsignedLong(PyList_GetItem(taint_tuple_as_list, 1));
            }
        }
    }
    Py_DecRef(taint_keys);

    new_entry->calls = FTDB_ENTRY_ULONG_ARRAY(func_entry, calls);
    new_entry->calls_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, calls);

    PyObject *key_call_info = PyUnicode_FromString("call_info");
    PyObject *call_info = PyDict_GetItem(func_entry, key_call_info);
    Py_DecRef(key_call_info);
    new_entry->call_info = calloc(new_entry->calls_count, sizeof(struct call_info));
    for (Py_ssize_t i = 0; i < PyList_Size(call_info); ++i) {
        PyObject *py_single_call_info = PyList_GetItem(call_info, i);
        struct call_info *single_call_info = &new_entry->call_info[i];
        single_call_info->start = FTDB_ENTRY_STRING(py_single_call_info, start);
        single_call_info->end = FTDB_ENTRY_STRING(py_single_call_info, end);
        single_call_info->ord = FTDB_ENTRY_ULONG(py_single_call_info, ord);
        single_call_info->expr = FTDB_ENTRY_STRING(py_single_call_info, expr);
        single_call_info->loc = FTDB_ENTRY_STRING(py_single_call_info, loc);
        single_call_info->csid = FTDB_ENTRY_INT64_OPTIONAL(py_single_call_info, csid);
        single_call_info->args = FTDB_ENTRY_ULONG_ARRAY(py_single_call_info, args);
        single_call_info->args_count = FTDB_ENTRY_ARRAY_SIZE(py_single_call_info, args);
    }

    PyObject *key_callrefs = PyUnicode_FromString("callrefs");
    PyObject *callrefs = PyDict_GetItem(func_entry, key_callrefs);
    Py_DecRef(key_callrefs);
    new_entry->callrefs = calloc(new_entry->calls_count, sizeof(struct callref_info));
    for (Py_ssize_t i = 0; i < PyList_Size(callrefs); ++i) {
        PyObject *py_single_callref = PyList_GetItem(callrefs, i);
        struct callref_info *single_callref = &new_entry->callrefs[i];
        single_callref->callarg_count = PyList_Size(py_single_callref);
        single_callref->callarg = calloc(single_callref->callarg_count, sizeof(struct callref_data));
        for (unsigned long j = 0; j < single_callref->callarg_count; ++j) {
            PyObject *py_callref_data = PyList_GetItem(py_single_callref, j);
            single_callref->callarg[j].type = FTDB_ENTRY_ENUM_FROM_STRING(py_callref_data, type, exprType);
            single_callref->callarg[j].pos = FTDB_ENTRY_ULONG(py_callref_data, pos);
            if (single_callref->callarg[j].type == EXPR_STRINGLITERAL) {
                single_callref->callarg[j].string_literal = FTDB_ENTRY_STRING_OPTIONAL(py_callref_data, id);
            } else if (single_callref->callarg[j].type == EXPR_FLOATLITERAL) {
                single_callref->callarg[j].floating_literal = FTDB_ENTRY_FLOAT_OPTIONAL(py_callref_data, id);
            } else if (single_callref->callarg[j].type == EXPR_INTEGERLITERAL) {
                single_callref->callarg[j].integer_literal = FTDB_ENTRY_INT64_OPTIONAL(py_callref_data, id);
            } else if (single_callref->callarg[j].type == EXPR_CHARLITERAL) {
                single_callref->callarg[j].character_literal = FTDB_ENTRY_INT_OPTIONAL(py_callref_data, id);
            } else {
                single_callref->callarg[j].id = FTDB_ENTRY_ULONG_OPTIONAL(py_callref_data, id);
            }
        }
    }

    PyObject *key_refcalls = PyUnicode_FromString("refcalls");
    PyObject *refcalls = PyDict_GetItem(func_entry, key_refcalls);
    Py_DecRef(key_refcalls);
    new_entry->refcalls_count = PyList_Size(refcalls);
    new_entry->refcalls = calloc(new_entry->refcalls_count, sizeof(struct refcall));
    for (Py_ssize_t i = 0; i < new_entry->refcalls_count; ++i) {
        PyObject *py_refcall_tuple = PyList_GetItem(refcalls, i);
        new_entry->refcalls[i].fid = PyLong_AsLong(PyList_GetItem(py_refcall_tuple, 0));
        if (PyList_Size(py_refcall_tuple) > 1) {
            new_entry->refcalls[i].ismembercall = 1;
            new_entry->refcalls[i].cid = PyLong_AsLong(PyList_GetItem(py_refcall_tuple, 1));
            new_entry->refcalls[i].field_index = PyLong_AsLong(PyList_GetItem(py_refcall_tuple, 2));
        }
    }

    PyObject *key_refcall_info = PyUnicode_FromString("refcall_info");
    PyObject *refcall_info = PyDict_GetItem(func_entry, key_refcall_info);
    Py_DecRef(key_refcall_info);
    new_entry->refcall_info = calloc(new_entry->refcalls_count, sizeof(struct call_info));
    for (Py_ssize_t i = 0; i < PyList_Size(refcall_info); ++i) {
        PyObject *py_single_refcall_info = PyList_GetItem(refcall_info, i);
        struct call_info *single_refcall_info = &new_entry->refcall_info[i];
        single_refcall_info->start = FTDB_ENTRY_STRING(py_single_refcall_info, start);
        single_refcall_info->end = FTDB_ENTRY_STRING(py_single_refcall_info, end);
        single_refcall_info->ord = FTDB_ENTRY_ULONG(py_single_refcall_info, ord);
        single_refcall_info->expr = FTDB_ENTRY_STRING(py_single_refcall_info, expr);
        single_refcall_info->loc = FTDB_ENTRY_STRING_OPTIONAL(py_single_refcall_info, loc);
        single_refcall_info->csid = FTDB_ENTRY_INT64_OPTIONAL(py_single_refcall_info, csid);
        single_refcall_info->args = FTDB_ENTRY_ULONG_ARRAY(py_single_refcall_info, args);
        single_refcall_info->args_count = FTDB_ENTRY_ARRAY_SIZE(py_single_refcall_info, args);
    }
    PyObject *key_refcallrefs = PyUnicode_FromString("refcallrefs");
    PyObject *refcallrefs = PyDict_GetItem(func_entry, key_refcallrefs);
    Py_DecRef(key_refcallrefs);
    new_entry->refcallrefs = calloc(new_entry->refcalls_count, sizeof(struct callref_info));
    for (Py_ssize_t i = 0; i < PyList_Size(refcallrefs); ++i) {
        PyObject *py_single_refcallref = PyList_GetItem(refcallrefs, i);
        struct callref_info *single_refcallref = &new_entry->refcallrefs[i];
        single_refcallref->callarg_count = PyList_Size(py_single_refcallref);
        single_refcallref->callarg = calloc(single_refcallref->callarg_count, sizeof(struct callref_data));
        for (unsigned long j = 0; j < single_refcallref->callarg_count; ++j) {
            PyObject *py_refcallref_data = PyList_GetItem(py_single_refcallref, j);
            single_refcallref->callarg[j].type = FTDB_ENTRY_ENUM_FROM_STRING(py_refcallref_data, type, exprType);
            single_refcallref->callarg[j].pos = FTDB_ENTRY_ULONG(py_refcallref_data, pos);
            if (single_refcallref->callarg[j].type == EXPR_STRINGLITERAL) {
                single_refcallref->callarg[j].string_literal = FTDB_ENTRY_STRING_OPTIONAL(py_refcallref_data, id);
            } else if (single_refcallref->callarg[j].type == EXPR_FLOATLITERAL) {
                single_refcallref->callarg[j].floating_literal = FTDB_ENTRY_FLOAT_OPTIONAL(py_refcallref_data, id);
            } else if (single_refcallref->callarg[j].type == EXPR_INTEGERLITERAL) {
                single_refcallref->callarg[j].integer_literal = FTDB_ENTRY_INT64_OPTIONAL(py_refcallref_data, id);
            } else if (single_refcallref->callarg[j].type == EXPR_CHARLITERAL) {
                single_refcallref->callarg[j].character_literal = FTDB_ENTRY_INT_OPTIONAL(py_refcallref_data, id);
            } else {
                single_refcallref->callarg[j].id = FTDB_ENTRY_ULONG_OPTIONAL(py_refcallref_data, id);
            }
        }
    }

    PyObject *key_switches = PyUnicode_FromString("switches");
    PyObject *switches = PyDict_GetItem(func_entry, key_switches);
    Py_DecRef(key_switches);
    new_entry->switches_count = PyList_Size(switches);
    new_entry->switches = calloc(new_entry->switches_count, sizeof(struct switch_info));
    for (Py_ssize_t i = 0; i < PyList_Size(switches); ++i) {
        PyObject *py_switch_info = PyList_GetItem(switches, i);
        struct switch_info *switch_info = &new_entry->switches[i];
        switch_info->condition = FTDB_ENTRY_STRING(py_switch_info, condition);
        switch_info->cases_count = FTDB_ENTRY_ARRAY_SIZE(py_switch_info, cases);
        switch_info->cases = calloc(switch_info->cases_count, sizeof(struct case_info));
        PyObject *key_cases = PyUnicode_FromString("cases");
        PyObject *cases = PyDict_GetItem(py_switch_info, key_cases);
        Py_DecRef(key_cases);
        for (Py_ssize_t j = 0; j < PyList_Size(cases); ++j) {
            PyObject *py_case_info = PyList_GetItem(cases, j);
            struct case_info *case_info = &switch_info->cases[j];
            case_info->lhs.expr_value = PyLong_AsLong(PyList_GetItem(py_case_info, 0));
            case_info->lhs.enum_code = PyString_get_c_str(PyList_GetItem(py_case_info, 1));
            case_info->lhs.macro_code = PyString_get_c_str(PyList_GetItem(py_case_info, 2));
            case_info->lhs.raw_code = PyString_get_c_str(PyList_GetItem(py_case_info, 3));
            if (PyList_Size(py_case_info) > 4) {
                case_info->rhs = calloc(1, sizeof(struct case_data));
                case_info->rhs->expr_value = PyLong_AsLong(PyList_GetItem(py_case_info, 4));
                case_info->rhs->enum_code = PyString_get_c_str(PyList_GetItem(py_case_info, 5));
                case_info->rhs->macro_code = PyString_get_c_str(PyList_GetItem(py_case_info, 6));
                case_info->rhs->raw_code = PyString_get_c_str(PyList_GetItem(py_case_info, 7));
            }
        }
    }

    PyObject *key_csmap = PyUnicode_FromString("csmap");
    PyObject *csmap = PyDict_GetItem(func_entry, key_csmap);
    Py_DecRef(key_csmap);
    new_entry->csmap_count = PyList_Size(csmap);
    new_entry->csmap = calloc(new_entry->csmap_count, sizeof(struct csitem));
    for (Py_ssize_t i = 0; i < PyList_Size(csmap); ++i) {
        PyObject *py_csitem = PyList_GetItem(csmap, i);
        struct csitem *csitem = &new_entry->csmap[i];
        csitem->pid = FTDB_ENTRY_INT64(py_csitem, pid);
        csitem->id = FTDB_ENTRY_ULONG(py_csitem, id);
        csitem->cf = FTDB_ENTRY_STRING(py_csitem, cf);
    }

    PyObject *key_macro_expansions = PyUnicode_FromString("macro_expansions");
    if (PyDict_Contains(func_entry, key_macro_expansions)) {
        PyObject *macro_expansions = PyDict_GetItem(func_entry, key_macro_expansions);
        new_entry->macro_expansions_count = PyList_Size(macro_expansions);
        new_entry->macro_expansions = calloc(new_entry->macro_expansions_count, sizeof(struct mexp_info));
        for (Py_ssize_t i = 0; i < PyList_Size(macro_expansions); ++i) {
            PyObject *py_mexp_info = PyList_GetItem(macro_expansions, i);
            struct mexp_info *mexp_info = &new_entry->macro_expansions[i];
            mexp_info->pos = FTDB_ENTRY_ULONG(py_mexp_info, pos);
            mexp_info->len = FTDB_ENTRY_ULONG(py_mexp_info, len);
            mexp_info->text = FTDB_ENTRY_STRING(py_mexp_info, text);
        }
    }
    Py_DecRef(key_macro_expansions);

    PyObject *key_locals = PyUnicode_FromString("locals");
    PyObject *locals = PyDict_GetItem(func_entry, key_locals);
    Py_DecRef(key_locals);
    new_entry->locals_count = PyList_Size(locals);
    new_entry->locals = calloc(new_entry->locals_count, sizeof(struct local_info));
    for (Py_ssize_t i = 0; i < PyList_Size(locals); ++i) {
        PyObject *py_local_info = PyList_GetItem(locals, i);
        struct local_info *local_info = &new_entry->locals[i];
        local_info->id = FTDB_ENTRY_ULONG(py_local_info, id);
        local_info->name = FTDB_ENTRY_STRING(py_local_info, name);
        local_info->isparm = FTDB_ENTRY_BOOL(py_local_info, parm);
        local_info->type = FTDB_ENTRY_ULONG(py_local_info, type);
        local_info->isstatic = FTDB_ENTRY_BOOL(py_local_info, static);
        local_info->isused = FTDB_ENTRY_BOOL(py_local_info, used);
        local_info->location = FTDB_ENTRY_STRING(py_local_info, location);
        local_info->csid = FTDB_ENTRY_INT64_OPTIONAL(py_local_info, csid);
    }

    PyObject *key_derefs = PyUnicode_FromString("derefs");
    PyObject *derefs = PyDict_GetItem(func_entry, key_derefs);
    Py_DecRef(key_derefs);
    new_entry->derefs_count = PyList_Size(derefs);
    new_entry->derefs = calloc(new_entry->derefs_count, sizeof(struct deref_info));
    for (Py_ssize_t i = 0; i < PyList_Size(derefs); ++i) {
        PyObject *py_deref_info = PyList_GetItem(derefs, i);
        struct deref_info *deref_info = &new_entry->derefs[i];
        deref_info->kind = FTDB_ENTRY_ENUM_FROM_STRING(py_deref_info, kind, exprType);
        deref_info->offset = FTDB_ENTRY_INT64_OPTIONAL(py_deref_info, offset);
        deref_info->basecnt = FTDB_ENTRY_ULONG_OPTIONAL(py_deref_info, basecnt);
        deref_info->expr = FTDB_ENTRY_STRING(py_deref_info, expr);
        deref_info->csid = FTDB_ENTRY_INT64(py_deref_info, csid);
        deref_info->ord_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info, ord);
        deref_info->ord = FTDB_ENTRY_ULONG_ARRAY(py_deref_info, ord);
        if ((deref_info->kind == EXPR_MEMBER) || (deref_info->kind == EXPR_OFFSETOF)) {
            deref_info->member_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info, member);
            deref_info->member = FTDB_ENTRY_INT64_ARRAY(py_deref_info, member);
            deref_info->type_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info, type);
            deref_info->type = FTDB_ENTRY_ULONG_ARRAY(py_deref_info, type);
        }
        if (deref_info->kind == EXPR_MEMBER) {
            deref_info->access_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info, access);
            deref_info->access = FTDB_ENTRY_ULONG_ARRAY(py_deref_info, access);
            deref_info->shift_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info, shift);
            deref_info->shift = FTDB_ENTRY_INT64_ARRAY(py_deref_info, shift);
            PyObject *key_mcall = PyUnicode_FromString("mcall");
            if (PyDict_Contains(py_deref_info, key_mcall)) {
                PyObject *mcall = PyDict_GetItem(py_deref_info, key_mcall);
                deref_info->mcall_count = PyList_Size(mcall);
                deref_info->mcall = calloc(deref_info->mcall_count, sizeof(int64_t));
                for (unsigned long j = 0; j < deref_info->mcall_count; ++j) {
                    deref_info->mcall[j] = PyLong_AsLong(PyList_GetItem(mcall, j));
                }
            }
            Py_DecRef(key_mcall);
        }
        PyObject *key_offsetrefs = PyUnicode_FromString("offsetrefs");
        PyObject *offsetrefs = PyDict_GetItem(py_deref_info, key_offsetrefs);
        Py_DecRef(key_offsetrefs);
        deref_info->offsetrefs_count = PyList_Size(offsetrefs);
        deref_info->offsetrefs = calloc(deref_info->offsetrefs_count, sizeof(struct offsetref_info));
        for (unsigned long j = 0; j < deref_info->offsetrefs_count; ++j) {
            PyObject *py_offsetref_info = PyList_GetItem(offsetrefs, j);
            struct offsetref_info *offsetref_info = &deref_info->offsetrefs[j];
            offsetref_info->kind = FTDB_ENTRY_ENUM_FROM_STRING(py_offsetref_info, kind, exprType);
            if (offsetref_info->kind == EXPR_STRING) {
                offsetref_info->id_string = FTDB_ENTRY_STRING(py_offsetref_info, id);
            } else if (offsetref_info->kind == EXPR_FLOATING) {
                offsetref_info->id_float = FTDB_ENTRY_FLOAT(py_offsetref_info, id);
            } else {
                offsetref_info->id_integer = FTDB_ENTRY_INT64(py_offsetref_info, id);
            }
            offsetref_info->mi = FTDB_ENTRY_ULONG_OPTIONAL(py_offsetref_info, mi);
            offsetref_info->di = FTDB_ENTRY_ULONG_OPTIONAL(py_offsetref_info, di);
            offsetref_info->cast = FTDB_ENTRY_ULONG_OPTIONAL(py_offsetref_info, cast);
        }
    }
    PyObject *key_ifs = PyUnicode_FromString("ifs");
    PyObject *ifs = PyDict_GetItem(func_entry, key_ifs);
    Py_DecRef(key_ifs);
    new_entry->ifs_count = PyList_Size(ifs);
    new_entry->ifs = calloc(new_entry->ifs_count, sizeof(struct if_info));
    for (Py_ssize_t i = 0; i < PyList_Size(ifs); ++i) {
        PyObject *py_if_info = PyList_GetItem(ifs, i);
        struct if_info *if_info = &new_entry->ifs[i];
        if_info->csid = FTDB_ENTRY_INT64(py_if_info, csid);
        PyObject *key_refs = PyUnicode_FromString("refs");
        PyObject *if_refs = PyDict_GetItem(py_if_info, key_refs);
        Py_DecRef(key_refs);
        if_info->refs_count = PyList_Size(if_refs);
        if_info->refs = calloc(if_info->refs_count, sizeof(struct ifref_info));
        for (unsigned long j = 0; j < if_info->refs_count; ++j) {
            PyObject *py_ifref_info = PyList_GetItem(if_refs, j);
            struct ifref_info *ifref_info = &if_info->refs[j];
            ifref_info->type = FTDB_ENTRY_ENUM_FROM_STRING(py_ifref_info, type, exprType);
            if (ifref_info->type == EXPR_STRINGLITERAL) {
                ifref_info->string_literal = FTDB_ENTRY_STRING_OPTIONAL(py_ifref_info, id);
            } else if (ifref_info->type == EXPR_FLOATLITERAL) {
                ifref_info->floating_literal = FTDB_ENTRY_FLOAT_OPTIONAL(py_ifref_info, id);
            } else if (ifref_info->type == EXPR_INTEGERLITERAL) {
                ifref_info->integer_literal = FTDB_ENTRY_INT64_OPTIONAL(py_ifref_info, id);
            } else if (ifref_info->type == EXPR_CHARLITERAL) {
                ifref_info->character_literal = FTDB_ENTRY_INT_OPTIONAL(py_ifref_info, id);
            } else {
                ifref_info->id = FTDB_ENTRY_ULONG_OPTIONAL(py_ifref_info, id);
            }
        }
    }

    PyObject *key_asm = PyUnicode_FromString("asm");
    PyObject *asms = PyDict_GetItem(func_entry, key_asm);
    Py_DecRef(key_asm);
    new_entry->asms_count = PyList_Size(asms);
    new_entry->asms = calloc(new_entry->asms_count, sizeof(struct asm_info));
    for (Py_ssize_t i = 0; i < PyList_Size(asms); ++i) {
        PyObject *py_asm_info = PyList_GetItem(asms, i);
        struct asm_info *asm_info = &new_entry->asms[i];
        asm_info->csid = FTDB_ENTRY_INT64(py_asm_info, csid);
        asm_info->str = FTDB_ENTRY_STRING(py_asm_info, str);
    }

    new_entry->globalrefs_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, globalrefs);
    new_entry->globalrefs = FTDB_ENTRY_ULONG_ARRAY(func_entry, globalrefs);

    PyObject *key_globalrefInfo = PyUnicode_FromString("globalrefInfo");
    PyObject *globalrefInfo = PyDict_GetItem(func_entry, key_globalrefInfo);
    Py_DecRef(key_globalrefInfo);
    new_entry->globalrefInfo = calloc(new_entry->globalrefs_count, sizeof(struct globalref_data));
    for (Py_ssize_t i = 0; i < PyList_Size(globalrefInfo); ++i) {
        PyObject *py_globalref_data = PyList_GetItem(globalrefInfo, i);
        struct globalref_data *globalref_data = &new_entry->globalrefInfo[i];
        globalref_data->refs_count = PyList_Size(py_globalref_data);
        globalref_data->refs = calloc(globalref_data->refs_count, sizeof(struct globalref_info));
        for (Py_ssize_t j = 0; j < PyList_Size(py_globalref_data); ++j) {
            PyObject *py_globalref_info = PyList_GetItem(py_globalref_data, j);
            struct globalref_info *globalref_info = &globalref_data->refs[j];
            globalref_info->start = FTDB_ENTRY_STRING(py_globalref_info, start);
            globalref_info->end = FTDB_ENTRY_STRING(py_globalref_info, end);
        }
    }

    new_entry->funrefs_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, funrefs);
    new_entry->funrefs = FTDB_ENTRY_ULONG_ARRAY(func_entry, funrefs);
    new_entry->refs_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, refs);
    new_entry->refs = FTDB_ENTRY_ULONG_ARRAY(func_entry, refs);
    new_entry->decls_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, decls);
    new_entry->decls = FTDB_ENTRY_ULONG_ARRAY(func_entry, decls);
    new_entry->types_count = FTDB_ENTRY_ARRAY_SIZE(func_entry, types);
    new_entry->types = FTDB_ENTRY_ULONG_ARRAY(func_entry, types);
}

static void libftdb_create_ftdb_funcdecl_entry(PyObject *self, PyObject *funcdecl_entry, struct ftdb_funcdecl_entry *new_entry) {
    new_entry->name = FTDB_ENTRY_STRING(funcdecl_entry, name);
    new_entry->__namespace = FTDB_ENTRY_STRING_OPTIONAL(funcdecl_entry, namespace);
    new_entry->id = FTDB_ENTRY_ULONG(funcdecl_entry, id);
    new_entry->fid = FTDB_ENTRY_ULONG(funcdecl_entry, fid);
    new_entry->nargs = FTDB_ENTRY_ULONG(funcdecl_entry, nargs);
    new_entry->isvariadic = FTDB_ENTRY_BOOL(funcdecl_entry, variadic);
    new_entry->istemplate = FTDB_ENTRY_BOOL_OPTIONAL(funcdecl_entry, template);
    new_entry->linkage = FTDB_ENTRY_ENUM_FROM_STRING(funcdecl_entry, linkage, functionLinkage);
    new_entry->ismember = FTDB_ENTRY_BOOL_OPTIONAL(funcdecl_entry, member);
    new_entry->__class = FTDB_ENTRY_STRING_OPTIONAL(funcdecl_entry, class);
    new_entry->classid = FTDB_ENTRY_ULONG_OPTIONAL(funcdecl_entry, classid);
    new_entry->template_parameters = FTDB_ENTRY_STRING_OPTIONAL(funcdecl_entry, template_parameters);
    new_entry->decl = FTDB_ENTRY_STRING(funcdecl_entry, decl);
    new_entry->signature = FTDB_ENTRY_STRING(funcdecl_entry, signature);
    new_entry->declhash = FTDB_ENTRY_STRING(funcdecl_entry, declhash);
    new_entry->location = FTDB_ENTRY_STRING(funcdecl_entry, location);
    new_entry->refcount = FTDB_ENTRY_ULONG(funcdecl_entry, refcount);
    new_entry->types_count = FTDB_ENTRY_ARRAY_SIZE(funcdecl_entry, types);
    new_entry->types = FTDB_ENTRY_ULONG_ARRAY(funcdecl_entry, types);
}

static void libftdb_create_ftdb_global_entry(PyObject *self, PyObject *global_entry, struct ftdb_global_entry *new_entry) {
    new_entry->name = FTDB_ENTRY_STRING(global_entry, name);
    new_entry->hash = FTDB_ENTRY_STRING(global_entry, hash);
    new_entry->id = FTDB_ENTRY_ULONG(global_entry, id);
    new_entry->def = FTDB_ENTRY_STRING(global_entry, def);
    new_entry->globalrefs_count = FTDB_ENTRY_ARRAY_SIZE(global_entry, globalrefs);
    new_entry->globalrefs = FTDB_ENTRY_ULONG_ARRAY(global_entry, globalrefs);
    new_entry->refs_count = FTDB_ENTRY_ARRAY_SIZE(global_entry, refs);
    new_entry->refs = FTDB_ENTRY_ULONG_ARRAY(global_entry, refs);
    new_entry->funrefs_count = FTDB_ENTRY_ARRAY_SIZE(global_entry, funrefs);
    new_entry->funrefs = FTDB_ENTRY_ULONG_ARRAY(global_entry, funrefs);
    new_entry->decls_count = FTDB_ENTRY_ARRAY_SIZE(global_entry, decls);
    new_entry->decls = FTDB_ENTRY_ULONG_ARRAY(global_entry, decls);
    new_entry->fid = FTDB_ENTRY_ULONG(global_entry, fid);
    new_entry->mids_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(global_entry, mids);
    new_entry->mids = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(global_entry, mids);
    new_entry->type = FTDB_ENTRY_ULONG(global_entry, type);
    new_entry->linkage = FTDB_ENTRY_ENUM_FROM_STRING(global_entry, linkage, functionLinkage);
    new_entry->location = FTDB_ENTRY_STRING(global_entry, location);
    new_entry->deftype = FTDB_ENTRY_ENUM_FROM_ULONG(global_entry, deftype, globalDefType);
    new_entry->hasinit = FTDB_ENTRY_INT(global_entry, hasinit);
    new_entry->init = FTDB_ENTRY_STRING(global_entry, init);

    PyObject *key_literals = PyUnicode_FromString("literals");
    PyObject *literals = PyDict_GetItem(global_entry, key_literals);
    Py_DecRef(key_literals);
    new_entry->integer_literals = FTDB_ENTRY_INT64_ARRAY(literals, integer);
    new_entry->integer_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, integer);
    new_entry->character_literals = FTDB_ENTRY_UINT_ARRAY(literals, character);
    new_entry->character_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, character);
    new_entry->floating_literals = FTDB_ENTRY_FLOAT_ARRAY(literals, floating);
    new_entry->floating_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, floating);
    new_entry->string_literals = FTDB_ENTRY_STRING_ARRAY(literals, string);
    new_entry->string_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals, string);
}

static int bitfield_compare(const void *a, const void *b) {
    if (((struct bitfield *)a)->index < ((struct bitfield *)b)->index)
        return -1;
    if (((struct bitfield *)a)->index > ((struct bitfield *)b)->index)
        return 1;

    return 0;
}

static void libftdb_create_ftdb_type_entry(PyObject *self, PyObject *type_entry, struct ftdb_type_entry *new_entry) {
    new_entry->id = FTDB_ENTRY_ULONG(type_entry, id);
    new_entry->fid = FTDB_ENTRY_ULONG(type_entry, fid);
    new_entry->hash = FTDB_ENTRY_STRING(type_entry, hash);
    new_entry->class_name = FTDB_ENTRY_STRING(type_entry, class);
    new_entry->__class = FTDB_ENTRY_ENUM_FROM_STRING(type_entry, class, TypeClass);
    new_entry->qualifiers = FTDB_ENTRY_STRING(type_entry, qualifiers);
    new_entry->size = FTDB_ENTRY_ULONG(type_entry, size);
    new_entry->str = FTDB_ENTRY_STRING(type_entry, str);
    new_entry->refcount = FTDB_ENTRY_ULONG(type_entry, refcount);
    new_entry->refs_count = FTDB_ENTRY_ARRAY_SIZE(type_entry, refs);
    new_entry->refs = FTDB_ENTRY_ULONG_ARRAY(type_entry, refs);
    new_entry->usedrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, usedrefs);
    new_entry->usedrefs = FTDB_ENTRY_INT64_ARRAY_OPTIONAL(type_entry, usedrefs);
    new_entry->decls_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, decls);
    new_entry->decls = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry, decls);
    new_entry->def = FTDB_ENTRY_STRING_OPTIONAL(type_entry, def);
    new_entry->memberoffsets_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, memberoffsets);
    new_entry->memberoffsets = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry, memberoffsets);
    new_entry->attrrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, attrrefs);
    new_entry->attrrefs = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry, attrrefs);
    new_entry->attrnum = FTDB_ENTRY_ULONG_OPTIONAL(type_entry, attrnum);
    new_entry->name = FTDB_ENTRY_STRING_OPTIONAL(type_entry, name);
    new_entry->isdependent = FTDB_ENTRY_BOOL_OPTIONAL(type_entry, dependent);
    new_entry->globalrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, globalrefs);
    new_entry->globalrefs = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry, globalrefs);
    new_entry->enumvalues_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, values);
    new_entry->enumvalues = FTDB_ENTRY_INT64_ARRAY_OPTIONAL(type_entry, values);
    new_entry->outerfn = FTDB_ENTRY_STRING_OPTIONAL(type_entry, outerfn);
    new_entry->outerfnid = FTDB_ENTRY_ULONG_OPTIONAL(type_entry, outerfnid);
    new_entry->isimplicit = FTDB_ENTRY_BOOL_OPTIONAL(type_entry, implicit);
    new_entry->isunion = FTDB_ENTRY_BOOL_OPTIONAL(type_entry, union);
    new_entry->refnames_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, refnames);
    new_entry->refnames = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(type_entry, refnames);
    new_entry->location = FTDB_ENTRY_STRING_OPTIONAL(type_entry, location);
    new_entry->isvariadic = FTDB_ENTRY_BOOL_OPTIONAL(type_entry, variadic);
    new_entry->funrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, funrefs);
    new_entry->funrefs = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry, funrefs);
    new_entry->useddef_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, useddef);
    new_entry->useddef = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(type_entry, useddef);
    new_entry->defhead = FTDB_ENTRY_STRING_OPTIONAL(type_entry, defhead);
    new_entry->identifiers_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, identifiers);
    new_entry->identifiers = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(type_entry, identifiers);
    new_entry->methods_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry, methods);
    new_entry->methods = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry, methods);

    PyObject *key_bitfields = PyUnicode_FromString("bitfields");
    if (PyDict_Contains(type_entry, key_bitfields)) {
        PyObject *bitfields = PyDict_GetItem(type_entry, key_bitfields);
        PyObject *bitfields_indexes = PyDict_Keys(bitfields);
        new_entry->bitfields_count = PyList_Size(bitfields_indexes);
        new_entry->bitfields = malloc(new_entry->bitfields_count * sizeof(struct bitfield));
        for (Py_ssize_t i = 0; i < new_entry->bitfields_count; ++i) {
            PyObject *bf_index = PyList_GetItem(bitfields_indexes, i);
            PyObject *bf_int_index = PyLong_FromString((char *)PyString_get_c_str(bf_index), 0, 10);
            PyObject *bf_value = PyDict_GetItem(bitfields, bf_index);
            new_entry->bitfields[i].index = PyLong_AsUnsignedLong(bf_int_index);
            new_entry->bitfields[i].bitwidth = PyLong_AsUnsignedLong(bf_value);
        }
        Py_DecRef(bitfields_indexes);
        qsort(new_entry->bitfields, new_entry->bitfields_count, sizeof(struct bitfield), bitfield_compare);
    }
    Py_DecRef(key_bitfields);
}

static void libftdb_create_ftdb_fops_entry(PyObject *self, PyObject *fops_entry, struct ftdb_fops_entry *new_entry) {
    new_entry->kind = FTDB_ENTRY_ENUM_FROM_STRING(fops_entry, kind, fopsKind);
    new_entry->type_id = FTDB_ENTRY_ULONG(fops_entry, type);
    new_entry->var_id = FTDB_ENTRY_ULONG(fops_entry, var);
    new_entry->func_id = FTDB_ENTRY_ULONG(fops_entry, func);
    new_entry->location = FTDB_ENTRY_STRING(fops_entry, loc);
    PyObject *key_members = PyUnicode_FromString("members");
    PyObject *py_members = PyDict_GetItem(fops_entry, key_members);
    Py_DecRef(key_members);
    new_entry->members = calloc(PyDict_Size(py_members), sizeof(struct ftdb_fops_member_entry));
    new_entry->members_count = PyDict_Size(py_members);
    PyObject *member_id, *func_ids;
    Py_ssize_t pos = 0, i = 0;
    while (PyDict_Next(py_members, &pos, &member_id, &func_ids)) {
        const char *member_id_str = PyString_get_c_str(member_id);
        PyObject *member_id_int = PyLong_FromString(member_id_str, 0, 10);
        new_entry->members[i].member_id = PyLong_AsUnsignedLong(member_id_int);
        new_entry->members[i].func_ids = calloc(PyList_Size(func_ids), sizeof(unsigned long));
        new_entry->members[i].func_ids_count = PyList_Size(func_ids);
        for (Py_ssize_t j = 0; j < PyList_Size(func_ids); ++j) {
            PyObject *py_func_id = PyList_GetItem(func_ids, j);
            new_entry->members[i].func_ids[j] = PyLong_AsUnsignedLong(py_func_id);
        }
        ++i;
    }
}

static void fill_matrix_data_entry(PyObject *matrix_data_entry, struct matrix_data *new_entry) {
    for (Py_ssize_t i = 0; i < PyList_Size(matrix_data_entry); ++i) {
        PyObject *entry = PyList_GetItem(matrix_data_entry, i);
        PyObject *key_name = PyUnicode_FromString("name");
        PyObject *name = PyDict_GetItem(entry, key_name);
        Py_DecRef(key_name);
        if (!strcmp(PyString_get_c_str(name), "data")) {
            new_entry->data_count = FTDB_ENTRY_ARRAY_SIZE(entry, data);
            new_entry->data = FTDB_ENTRY_ULONG_ARRAY(entry, data);
        } else if (!strcmp(PyString_get_c_str(name), "row_ind")) {
            new_entry->row_ind_count = FTDB_ENTRY_ARRAY_SIZE(entry, data);
            new_entry->row_ind = FTDB_ENTRY_ULONG_ARRAY(entry, data);
        } else if (!strcmp(PyString_get_c_str(name), "col_ind")) {
            new_entry->col_ind_count = FTDB_ENTRY_ARRAY_SIZE(entry, data);
            new_entry->col_ind = FTDB_ENTRY_ULONG_ARRAY(entry, data);
        } else if (!strcmp(PyString_get_c_str(name), "matrix_size")) {
            new_entry->matrix_size = FTDB_ENTRY_ULONG(entry, data);
        }
    }
}

static void fill_func_map_entry_entry(PyObject *func_map_entry, struct func_map_entry *new_entry) {
    new_entry->id = FTDB_ENTRY_ULONG(func_map_entry, id);
    new_entry->fids_count = FTDB_ENTRY_ARRAY_SIZE(func_map_entry, fids);
    new_entry->fids = FTDB_ENTRY_ULONG_ARRAY(func_map_entry, fids);
}

static void fill_size_dep_item_entry(PyObject *size_dep_item, struct size_dep_item *new_entry) {
    new_entry->id = FTDB_ENTRY_ULONG(size_dep_item, id);
    new_entry->add = FTDB_ENTRY_INT64(size_dep_item, add);
}

static void fill_init_data_item_entry(PyObject *data_item_entry, struct init_data_item *new_entry) {
    new_entry->id = FTDB_ENTRY_ULONG_OPTIONAL(data_item_entry, id);
    new_entry->name_count = FTDB_ENTRY_ARRAY_SIZE(data_item_entry, name);
    new_entry->name = FTDB_ENTRY_STRING_ARRAY(data_item_entry, name);
    new_entry->size = FTDB_ENTRY_ULONG_OPTIONAL(data_item_entry, size);
    new_entry->size_dep = FTDB_ENTRY_TYPE_OPTIONAL(data_item_entry, size_dep, size_dep_item);
    new_entry->nullterminated = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, nullterminated);
    new_entry->tagged = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, tagged);
    new_entry->fuzz = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, fuzz);
    new_entry->pointer = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, pointer);
    new_entry->min_value = FTDB_ENTRY_INT64_OPTIONAL(data_item_entry, min_value);
    new_entry->max_value = FTDB_ENTRY_INT64_OPTIONAL(data_item_entry, max_value);
    new_entry->value = FTDB_ENTRY_INT64_OPTIONAL(data_item_entry, value);
    new_entry->user_name = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, user_name);
    new_entry->protected_var = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, protected);
    new_entry->value_dep_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(data_item_entry, value_dep);
    new_entry->value_dep = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(data_item_entry, value_dep);
    new_entry->fuzz_offset = FTDB_ENTRY_ULONG_OPTIONAL(data_item_entry, fuzz_offset);
    new_entry->force_type = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry, force_type);
    new_entry->always_init_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(data_item_entry, always_init);
    new_entry->always_init = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(data_item_entry, always_init);
    new_entry->subitems_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(data_item_entry, subitems);
    new_entry->subitems = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(data_item_entry, subitems, init_data_item, new_entry->subitems_count);
}

static void fill_init_data_entry_entry(PyObject *init_data_entry, struct init_data_entry *new_entry) {
    new_entry->name = FTDB_ENTRY_STRING(init_data_entry, name);
    new_entry->order_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(init_data_entry, order);
    new_entry->order = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(init_data_entry, order);
    new_entry->interface = FTDB_ENTRY_STRING(init_data_entry, interface);
    new_entry->items_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(init_data_entry, items);
    new_entry->items = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(init_data_entry, items, init_data_item, new_entry->items_count);
}

static void fill_known_data_entry_entry(PyObject *known_data_entry, struct known_data_entry *new_entry) {
    new_entry->version = FTDB_ENTRY_STRING(known_data_entry, version);
    new_entry->func_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, func_ids);
    new_entry->func_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry, func_ids);
    new_entry->builtin_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, builtin_ids);
    new_entry->builtin_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry, builtin_ids);
    new_entry->asm_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, asm_ids);
    new_entry->asm_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry, asm_ids);
    new_entry->lib_funcs_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, lib_funcs);
    new_entry->lib_funcs = FTDB_ENTRY_STRING_ARRAY(known_data_entry, lib_funcs);
    new_entry->lib_funcs_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, lib_funcs_ids);
    new_entry->lib_funcs_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry, lib_funcs_ids);
    new_entry->always_inc_funcs_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, always_inc_funcs_ids);
    new_entry->always_inc_funcs_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry, always_inc_funcs_ids);
    new_entry->replacement_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry, replacement_ids);
    new_entry->replacement_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry, replacement_ids);
}

static void fill_BAS_item_entry(PyObject *BAS_item_entry, struct BAS_item *new_entry) {
    new_entry->loc = FTDB_ENTRY_STRING(BAS_item_entry, loc);
    new_entry->entries_count = FTDB_ENTRY_ARRAY_SIZE(BAS_item_entry, entries);
    new_entry->entries = FTDB_ENTRY_STRING_ARRAY(BAS_item_entry, entries);
}

static void fill_func_fptrs_entry_entry(PyObject *func_fptrs_item_tuple, struct func_fptrs_entry *new_entry) {
    new_entry->fname = PyString_get_c_str(PyTuple_GetItem(func_fptrs_item_tuple, 0));
    PyObject *py_ids = PyTuple_GetItem(func_fptrs_item_tuple, 1);
    new_entry->ids_count = PyList_Size(PyTuple_GetItem(func_fptrs_item_tuple, 1));
    new_entry->ids = calloc(new_entry->ids_count, sizeof(unsigned long));
    for (unsigned long i = 0; i < new_entry->ids_count; ++i) {
        new_entry->ids[i] = PyLong_AsUnsignedLong(PyList_GetItem(py_ids, i));
    }
}

static void fill_func_fptrs_item_entry(PyObject *func_fptrs_item_entry, struct func_fptrs_item *new_entry) {
    new_entry->id = FTDB_ENTRY_ULONG(func_fptrs_item_entry, _id);
    new_entry->entries_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(func_fptrs_item_entry, entries);
    new_entry->entries = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(func_fptrs_item_entry, entries, func_fptrs_entry, new_entry->entries_count);
}

static void destroy_ftdb(struct ftdb *ftdb) {
    // TODO
}

PyObject *libftdb_create_ftdb(PyObject *self, PyObject *args, PyObject *kwargs) {
    PyObject *dbJSON = PyTuple_GetItem(args, 0);
    PyObject *dbfn = PyTuple_GetItem(args, 1);
    int show_stats = 0, verbose_mode = 0, debug_mode = 0;
    if (PyTuple_Size(args) > 2) {
        PyObject *show_stats_arg = PyTuple_GetItem(args, 2);
        if (show_stats_arg == Py_True) {
            show_stats = 1;
        }
    }

    PyObject *py_debug = PyUnicode_FromString("verbose");
    PyObject *py_quiet = PyUnicode_FromString("debug");
    if (kwargs) {
        if (PyDict_Contains(kwargs, py_debug))
            verbose_mode = !!PyLong_AsLong(PyDict_GetItem(kwargs, py_debug));
        if (PyDict_Contains(kwargs, py_quiet))
            debug_mode = !!PyLong_AsLong(PyDict_GetItem(kwargs, py_quiet));
    }
    Py_DecRef(py_debug);
    Py_DecRef(py_quiet);

    struct ftdb ftdb = {0};
    ftdb.db_magic = FTDB_MAGIC_NUMBER;
    ftdb.db_version = FTDB_VERSION;

    PyObject *key_funcs = PyUnicode_FromString("funcs");
    PyObject *funcs = PyDict_GetItem(dbJSON, key_funcs);
    Py_DecRef(key_funcs);
    ftdb.funcs = calloc(PyList_Size(funcs), sizeof(struct ftdb_func_entry));
    ftdb.funcs_count = PyList_Size(funcs);
    for (Py_ssize_t i = 0; i < PyList_Size(funcs); ++i) {
        libftdb_create_ftdb_func_entry(self, PyList_GetItem(funcs, i), &ftdb.funcs[i]);
        ftdb.funcs[i].__index = i;
        if (PyErr_Occurred()) {
            printf("Exception while processing function entry at index %ld\n", i);
            PyErr_PrintEx(0);
            PyErr_Clear();
        }
    }

    PyObject *key_funcdecls = PyUnicode_FromString("funcdecls");
    PyObject *funcdecls = PyDict_GetItem(dbJSON, key_funcdecls);
    Py_DecRef(key_funcdecls);
    ftdb.funcdecls = calloc(PyList_Size(funcdecls), sizeof(struct ftdb_funcdecl_entry));
    ftdb.funcdecls_count = PyList_Size(funcdecls);
    for (Py_ssize_t i = 0; i < PyList_Size(funcdecls); ++i) {
        libftdb_create_ftdb_funcdecl_entry(self, PyList_GetItem(funcdecls, i), &ftdb.funcdecls[i]);
        ftdb.funcdecls[i].__index = i;
    }

    PyObject *key_unresolvedfuncs = PyUnicode_FromString("unresolvedfuncs");
    PyObject *unresolvedfuncs = PyDict_GetItem(dbJSON, key_unresolvedfuncs);
    Py_DecRef(key_unresolvedfuncs);
    ftdb.unresolvedfuncs = calloc(PyList_Size(unresolvedfuncs), sizeof(struct ftdb_unresolvedfunc_entry));
    ftdb.unresolvedfuncs_count = PyList_Size(unresolvedfuncs);
    for (Py_ssize_t i = 0; i < PyList_Size(unresolvedfuncs); ++i) {
        PyObject *py_unresolvedfunc_entry = PyList_GetItem(unresolvedfuncs, i);
        struct ftdb_unresolvedfunc_entry *unresolvedfunc_entry = &ftdb.unresolvedfuncs[i];
        unresolvedfunc_entry->name = FTDB_ENTRY_STRING(py_unresolvedfunc_entry, name);
        unresolvedfunc_entry->id = FTDB_ENTRY_ULONG(py_unresolvedfunc_entry, id);
    }

    PyObject *key_globals = PyUnicode_FromString("globals");
    PyObject *globals = PyDict_GetItem(dbJSON, key_globals);
    Py_DecRef(key_globals);
    ftdb.globals = calloc(PyList_Size(globals), sizeof(struct ftdb_global_entry));
    ftdb.globals_count = PyList_Size(globals);
    for (Py_ssize_t i = 0; i < PyList_Size(globals); ++i) {
        libftdb_create_ftdb_global_entry(self, PyList_GetItem(globals, i), &ftdb.globals[i]);
        ftdb.globals[i].__index = i;
    }

    PyObject *key_types = PyUnicode_FromString("types");
    PyObject *types = PyDict_GetItem(dbJSON, key_types);
    Py_DecRef(key_types);
    ftdb.types = calloc(PyList_Size(types), sizeof(struct ftdb_type_entry));
    ftdb.types_count = PyList_Size(types);
    for (Py_ssize_t i = 0; i < PyList_Size(types); ++i) {
        libftdb_create_ftdb_type_entry(self, PyList_GetItem(types, i), &ftdb.types[i]);
        ftdb.types[i].__index = i;
    }

    PyObject *key_fops = PyUnicode_FromString("fops");
    PyObject *fops = PyDict_GetItem(dbJSON, key_fops);
    Py_DecRef(key_fops);
    ftdb.fops = calloc(PyList_Size(fops), sizeof(struct ftdb_fops_entry));
    ftdb.fops_count = PyList_Size(fops);
    for (Py_ssize_t i = 0; i < PyList_Size(fops); ++i) {
        libftdb_create_ftdb_fops_entry(self, PyList_GetItem(fops, i), &ftdb.fops[i]);
        ftdb.fops[i].__index = i;
    }

    PyObject *key_source_info = PyUnicode_FromString("source_info");
    PyObject *key_sources = PyUnicode_FromString("sources");
    if (PyDict_Contains(dbJSON, key_source_info)) {
        PyObject *sources = PyDict_GetItem(dbJSON, key_source_info);
        ftdb.sourceindex_table = calloc(PyList_Size(sources), sizeof(const char *));
        ftdb.sourceindex_table_count = PyList_Size(sources);
        for (Py_ssize_t i = 0; i < PyList_Size(sources); ++i) {
            PyObject *single_source_map = PyList_GetItem(sources, i);
            PyObject *py_source_path = FTDB_ENTRY_PYOBJECT(single_source_map, name);
            PyObject *py_source_id = FTDB_ENTRY_PYOBJECT(single_source_map, id);
            stringRefMap_insert(&ftdb.sourcemap, PyString_get_c_str(py_source_path), PyLong_AsUnsignedLong(py_source_id));
            ftdb.sourceindex_table[PyLong_AsUnsignedLong(py_source_id)] = PyString_get_c_str(py_source_path);
        }
    } else if (PyDict_Contains(dbJSON, key_sources)) {
        PyObject *sources = PyDict_GetItem(dbJSON, key_sources);
        ftdb.sourceindex_table = calloc(PyList_Size(sources), sizeof(const char *));
        ftdb.sourceindex_table_count = PyList_Size(sources);
        for (Py_ssize_t i = 0; i < PyList_Size(sources); ++i) {
            PyObject *single_source_legacy_map = PyList_GetItem(sources, i);
            PyObject *sslm_keys = PyDict_Keys(single_source_legacy_map);
            PyObject *py_source_path = PyList_GetItem(sslm_keys, 0);
            PyObject *py_source_id = PyDict_GetItem(single_source_legacy_map, py_source_path);
            Py_DecRef(sslm_keys);
            stringRefMap_insert(&ftdb.sourcemap, PyString_get_c_str(py_source_path), PyLong_AsUnsignedLong(py_source_id));
            ftdb.sourceindex_table[PyLong_AsUnsignedLong(py_source_id)] = PyString_get_c_str(py_source_path);
        }
    } else {
        PyErr_SetString(libftdb_ftdbError, "ERROR: missing ftdb table \"source_info\"/\"sources\"");
        Py_DecRef(key_source_info);
        Py_DecRef(key_sources);
        Py_RETURN_NONE;
    }
    Py_DecRef(key_source_info);
    Py_DecRef(key_sources);

    PyObject *key_module_info = PyUnicode_FromString("module_info");
    PyObject *key_modules = PyUnicode_FromString("modules");
    if (PyDict_Contains(dbJSON, key_module_info)) {
        PyObject *modules = PyDict_GetItem(dbJSON, key_module_info);
        ftdb.moduleindex_table = calloc(PyList_Size(modules), sizeof(const char *));
        ftdb.moduleindex_table_count = PyList_Size(modules);
        for (Py_ssize_t i = 0; i < PyList_Size(modules); ++i) {
            PyObject *single_module_map = PyList_GetItem(modules, i);
            PyObject *py_module_path = FTDB_ENTRY_PYOBJECT(single_module_map, name);
            PyObject *py_module_id = FTDB_ENTRY_PYOBJECT(single_module_map, id);
            stringRefMap_insert(&ftdb.modulemap, PyString_get_c_str(py_module_path), PyLong_AsUnsignedLong(py_module_id));
            ftdb.moduleindex_table[PyLong_AsUnsignedLong(py_module_id)] = PyString_get_c_str(py_module_path);
        }
    } else if (PyDict_Contains(dbJSON, key_modules)) {
        PyObject *modules = PyDict_GetItem(dbJSON, key_modules);
        ftdb.moduleindex_table = calloc(PyList_Size(modules), sizeof(const char *));
        ftdb.moduleindex_table_count = PyList_Size(modules);
        for (Py_ssize_t i = 0; i < PyList_Size(modules); ++i) {
            PyObject *single_module_legacy_map = PyList_GetItem(modules, i);
            PyObject *smlm_keys = PyDict_Keys(single_module_legacy_map);
            PyObject *py_module_path = PyList_GetItem(smlm_keys, 0);
            PyObject *py_module_id = PyDict_GetItem(single_module_legacy_map, py_module_path);
            Py_DecRef(smlm_keys);
            stringRefMap_insert(&ftdb.modulemap, PyString_get_c_str(py_module_path), PyLong_AsUnsignedLong(py_module_id));
            ftdb.moduleindex_table[PyLong_AsUnsignedLong(py_module_id)] = PyString_get_c_str(py_module_path);
        }
    }
    Py_DecRef(key_module_info);
    Py_DecRef(key_modules);

    ftdb.version = FTDB_ENTRY_STRING(dbJSON, version);
    ftdb.module = FTDB_ENTRY_STRING(dbJSON, module);
    ftdb.directory = FTDB_ENTRY_STRING(dbJSON, directory);
    ftdb.release = FTDB_ENTRY_STRING(dbJSON, release);

    ftdb.funcs_tree_calls_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_calls_no_asm, matrix_data);
    ftdb.funcs_tree_calls_no_known = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_calls_no_known, matrix_data);
    ftdb.funcs_tree_calls_no_known_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_calls_no_known_no_asm, matrix_data);
    ftdb.funcs_tree_func_calls = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_func_calls, matrix_data);
    ftdb.funcs_tree_func_refs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_func_refs, matrix_data);
    ftdb.funcs_tree_funrefs_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_funrefs_no_asm, matrix_data);
    ftdb.funcs_tree_funrefs_no_known = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_funrefs_no_known, matrix_data);
    ftdb.funcs_tree_funrefs_no_known_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, funcs_tree_funrefs_no_known_no_asm, matrix_data);
    ftdb.globs_tree_globalrefs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, globs_tree_globalrefs, matrix_data);
    ftdb.types_tree_refs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, types_tree_refs, matrix_data);
    ftdb.types_tree_usedrefs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON, types_tree_usedrefs, matrix_data);
    ftdb.static_funcs_map_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON, static_funcs_map);
    ftdb.static_funcs_map = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON, static_funcs_map, func_map_entry, ftdb.static_funcs_map_count);
    ftdb.init_data_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON, init_data);
    ftdb.init_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON, init_data, init_data_entry, ftdb.init_data_count);
    ftdb.known_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON, known_data, known_data_entry, 1);
    ftdb.BAS_data_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON, BAS);
    ftdb.BAS_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON, BAS, BAS_item, ftdb.BAS_data_count);
    ftdb.func_fptrs_data_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON, func_fptrs);
    ftdb.func_fptrs_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON, func_fptrs, func_fptrs_item, ftdb.func_fptrs_data_count);

    int ok = ftdb_maps(&ftdb, show_stats);
    (void)ok;

    for (unsigned long i = 0; i < ftdb.static_funcs_map_count; ++i) {
        struct func_map_entry *entry = &ftdb.static_funcs_map[i];
        ulong_entryMap_insert(&ftdb.static_funcs_map_index, entry->id, entry);
    }
    printf("static_funcs_map_index keys: %zu\n", ulong_entryMap_count(&ftdb.static_funcs_map_index));

    for (unsigned long i = 0; i < ftdb.BAS_data_count; ++i) {
        struct BAS_item *item = &ftdb.BAS_data[i];
        stringRef_entryMap_insert(&ftdb.BAS_data_index, item->loc, item);
    }
    printf("BAS_data_index keys: %zu\n", stringRef_entryMap_count(&ftdb.BAS_data_index));

    printf("funcs count: %ld\n", PyList_Size(funcs));
    printf("funcs entry count: %zu\n", ftdb.funcs_count);
    printf("funcdecls count: %ld\n", PyList_Size(funcdecls));
    printf("funcdecls entry count: %zu\n", ftdb.funcdecls_count);
    printf("unresolvedfuncs count: %ld\n", PyList_Size(unresolvedfuncs));
    printf("unresolvedfuncs entry count: %zu\n", ftdb.unresolvedfuncs_count);
    printf("globals count: %ld\n", PyList_Size(globals));
    printf("globals entry count: %zu\n", ftdb.globals_count);
    printf("types count: %ld\n", PyList_Size(types));
    printf("types entry count: %zu\n", ftdb.types_count);
    printf("fops count: %ld\n", PyList_Size(fops));
    printf("fops entry count: %zu\n", ftdb.fops_count);
    if (ftdb.funcs_tree_calls_no_asm)
        printf("funcs_tree_calls_no_asm: OK\n");
    if (ftdb.funcs_tree_calls_no_known)
        printf("funcs_tree_calls_no_known: OK\n");
    if (ftdb.funcs_tree_calls_no_known_no_asm)
        printf("funcs_tree_calls_no_known_no_asm: OK\n");
    if (ftdb.funcs_tree_func_calls)
        printf("funcs_tree_func_calls: OK\n");
    if (ftdb.funcs_tree_func_refs)
        printf("funcs_tree_func_refs: OK\n");
    if (ftdb.funcs_tree_funrefs_no_asm)
        printf("funcs_tree_funrefs_no_asm: OK\n");
    if (ftdb.funcs_tree_funrefs_no_known)
        printf("funcs_tree_funrefs_no_known: OK\n");
    if (ftdb.funcs_tree_funrefs_no_known_no_asm)
        printf("funcs_tree_funrefs_no_known_no_asm: OK\n");
    if (ftdb.globs_tree_globalrefs)
        printf("globs_tree_globalrefs: OK\n");
    if (ftdb.types_tree_refs)
        printf("types_tree_refs: OK\n");
    if (ftdb.types_tree_usedrefs)
        printf("types_tree_usedrefs: OK\n");
    if (ftdb.static_funcs_map)
        printf("static_funcs_map [%lu]: OK\n", ftdb.static_funcs_map_count);
    if (ftdb.init_data)
        printf("init_data [%lu]: OK\n", ftdb.init_data_count);
    if (ftdb.known_data)
        printf("known_data: OK\n");
    if (ftdb.BAS_data)
        printf("BAS_data [%lu]: OK\n", ftdb.BAS_data_count);
    if (ftdb.func_fptrs_data)
        printf("func_fptrs_data [%lu]: OK\n", ftdb.func_fptrs_data_count);

    const char *dbfn_s = PyString_get_c_str(dbfn);
    struct uflat *uflat = uflat_init(dbfn_s);
    PYASSTR_DECREF(dbfn_s);
    if (UFLAT_IS_ERR(uflat)) {
        printf("uflat_init(): %s\n", strerror(UFLAT_PTR_ERR(uflat)));
        Py_RETURN_FALSE;
    }

    int rv = uflat_set_option(uflat, UFLAT_OPT_OUTPUT_SIZE, 50ULL * 1024 * 1024 * 1024);
    if (rv) {
        printf("uflat_set_option(OUTPUT_SIZE): %d\n", rv);
        goto uflat_error_exit;
    }

    rv = uflat_set_option(uflat, UFLAT_OPT_SKIP_MEM_FRAGMENTS, 1);
    if (rv) {
        printf("uflat_set_option(SKIP_MEM_FRAGMENTS): %d\n", rv);
        goto uflat_error_exit;
    }

    uflat_set_option(uflat, UFLAT_OPT_SKIP_MEM_COPY, 1);

    if (verbose_mode)
        uflat_set_option(uflat, UFLAT_OPT_VERBOSE, 1);
    if (debug_mode)
        uflat_set_option(uflat, UFLAT_OPT_DEBUG, 1);

    FOR_ROOT_POINTER(&ftdb,
                     FLATTEN_STRUCT(ftdb, &ftdb);
    );

    int err = uflat_write(uflat);
    if (err != 0) {
        printf("flatten_write(): %s\n", strerror(err));
        goto uflat_error_exit;
    }

    uflat_fini(uflat);
    destroy_ftdb(&ftdb);
    Py_RETURN_NONE;

uflat_error_exit:
    uflat_fini(uflat);
    destroy_ftdb(&ftdb);
    Py_RETURN_FALSE;
}

PyMethodDef libftdb_methods[] = {
    {"create_ftdb", (PyCFunction)libftdb_create_ftdb, METH_VARARGS | METH_KEYWORDS, "Create cached version of Function/Type database file"},
    {"parse_c_fmt_string",  libftdb_parse_c_fmt_string, METH_VARARGS, "Parse C format string and returns a list of types of detected parameters"},
    {NULL, NULL, 0, NULL}
};

struct PyModuleDef libftdb_module = {
    PyModuleDef_HEAD_INIT,
    "libftdb",
    "Function/Type database Python tooling",
    -1,
    libftdb_methods,
};

PyMethodDef libftdb_ftdb_methods[] = {
    {"load", (PyCFunction)libftdb_ftdb_load, METH_VARARGS | METH_KEYWORDS, "Load the database cache file"},
    {"get_BAS_item_by_loc", (PyCFunction)libftdb_ftdb_get_BAS_item_by_loc, METH_VARARGS | METH_KEYWORDS, "Get the BAS_item based on its location"},
    {"get_func_map_entry_by_id", (PyCFunction)libftdb_ftdb_get_func_map_entry__by_id, METH_VARARGS | METH_KEYWORDS, "Get the func_map_entry based on its id"},
    {NULL, NULL, 0, NULL}
};

PyMappingMethods libftdb_ftdb_mapping_methods = {
    .mp_subscript = libftdb_ftdb_mp_subscript,
};

PyGetSetDef libftdb_ftdb_getset[] = {
    {"__refcount__", libftdb_ftdb_get_refcount, 0, "ftdb object reference count", 0},
    {"version", libftdb_ftdb_get_version, 0, "ftdb object version", 0},
    {"module", libftdb_ftdb_get_module, 0, "ftdb object module", 0},
    {"directory", libftdb_ftdb_get_directory, 0, "ftdb object directory", 0},
    {"release", libftdb_ftdb_get_release, 0, "ftdb object release", 0},
    {"sources", libftdb_ftdb_get_sources, 0, "ftdb sources object", 0},
    {"source_info", libftdb_ftdb_get_sources_as_dict, 0, "ftdb source info object", 0},
    {"modules", libftdb_ftdb_get_modules, 0, "ftdb modules object", 0},
    {"module_info", libftdb_ftdb_get_modules_as_dict, 0, "ftdb source info object", 0},
    {"funcs", libftdb_ftdb_get_funcs, 0, "ftdb funcs object", 0},
    {"funcdecls", libftdb_ftdb_get_funcdecls, 0, "ftdb funcdecls object", 0},
    {"unresolvedfuncs", libftdb_ftdb_get_unresolvedfuncs, 0, "ftdb unresolvedfuncs object", 0},
    {"globals", libftdb_ftdb_get_globals, 0, "ftdb globals object", 0},
    {"types", libftdb_ftdb_get_types, 0, "ftdb types object", 0},
    {"fops", libftdb_ftdb_get_fops, 0, "ftdb fops object", 0},
    {"funcs_tree_calls_no_asm", libftdb_ftdb_get_funcs_tree_calls_no_asm, 0, "ftdb funcs_tree_calls_no_asm object", 0},
    {"funcs_tree_calls_no_known", libftdb_ftdb_get_funcs_tree_calls_no_known, 0, "ftdb funcs_tree_calls_no_known object", 0},
    {"funcs_tree_calls_no_known_no_asm", libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm, 0, "ftdb funcs_tree_calls_no_known_no_asm object", 0},
    {"funcs_tree_func_calls", libftdb_ftdb_get_funcs_tree_func_calls, 0, "ftdb funcs_tree_func_calls object", 0},
    {"funcs_tree_func_refs", libftdb_ftdb_get_funcs_tree_func_refs, 0, "ftdb funcs_tree_func_refs object", 0},
    {"funcs_tree_funrefs_no_asm", libftdb_ftdb_get_funcs_tree_funrefs_no_asm, 0, "ftdb funcs_tree_funrefs_no_asm object", 0},
    {"funcs_tree_funrefs_no_known", libftdb_ftdb_get_funcs_tree_funrefs_no_known, 0, "ftdb funcs_tree_funrefs_no_known object", 0},
    {"funcs_tree_funrefs_no_known_no_asm", libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm, 0, "ftdb funcs_tree_funrefs_no_known_no_asm object", 0},
    {"globs_tree_globalrefs", libftdb_ftdb_get_globs_tree_globalrefs, 0, "ftdb globs_tree_globalrefs object", 0},
    {"types_tree_refs", libftdb_ftdb_get_types_tree_refs, 0, "ftdb types_tree_refs object", 0},
    {"types_tree_usedrefs", libftdb_ftdb_get_types_tree_usedrefs, 0, "ftdb types_tree_usedrefs object", 0},
    {"static_funcs_map", libftdb_ftdb_get_static_funcs_map, 0, "ftdb static_funcs_map object", 0},
    {"init_data", libftdb_ftdb_get_init_data, 0, "ftdb init_data object", 0},
    {"known_data", libftdb_ftdb_get_known_data, 0, "ftdb known_data object", 0},
    {"BAS", libftdb_ftdb_get_BAS, 0, "ftdb BAS object", 0},
    {"func_fptrs", libftdb_ftdb_get_func_fptrs, 0, "ftdb func_fptrs object", 0},
    {0, 0, 0, 0, 0},
};

PyNumberMethods libftdb_ftdb_number_methods = {
    .nb_bool = libftdb_ftdb_bool,
};

PySequenceMethods libftdb_ftdb_sequence_methods = {
    .sq_contains = libftdb_ftdb_sq_contains
};

PyTypeObject libftdb_ftdbType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "libftdb.ftdb",
    .tp_basicsize = sizeof(libftdb_ftdb_object),
    .tp_dealloc = (destructor)libftdb_ftdb_dealloc,
    .tp_repr = (reprfunc)libftdb_ftdb_repr,
    .tp_as_number = &libftdb_ftdb_number_methods,
    .tp_as_sequence = &libftdb_ftdb_sequence_methods,
    .tp_as_mapping = &libftdb_ftdb_mapping_methods,
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, /* is this needed? */
    .tp_doc = "libftdb ftdb object",
    .tp_methods = libftdb_ftdb_methods,
    .tp_getset = libftdb_ftdb_getset,
    .tp_init = (initproc)libftdb_ftdb_init,
    .tp_new = libftdb_ftdb_new,
};

PyMODINIT_FUNC
PyInit_libftdb(void) {
    PyObject *m;

    m = PyModule_Create(&libftdb_module);
    if (m == 0)
        return 0;

    if (PyType_Ready(&libftdb_ftdbType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbType);

    if (PyType_Ready(&libftdb_ftdbSourcesType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbSourcesType);

    if (PyType_Ready(&libftdb_ftdbModulesType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbModulesType);

    if (PyType_Ready(&libftdb_ftdbFuncsType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncsType);

    if (PyType_Ready(&libftdb_ftdbFuncdeclsType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncdeclsType);

    if (PyType_Ready(&libftdb_ftdbUnresolvedfuncsType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbUnresolvedfuncsType);

    if (PyType_Ready(&libftdb_ftdbGlobalsType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbGlobalsType);

    if (PyType_Ready(&libftdb_ftdbTypesType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbTypesType);

    if (PyType_Ready(&libftdb_ftdbFopsType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFopsType);

    if (PyType_Ready(&libftdb_ftdbSourcesIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbSourcesIterType);

    if (PyType_Ready(&libftdb_ftdbModulesIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbModulesIterType);

    if (PyType_Ready(&libftdb_ftdbFuncsIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncsIterType);

    if (PyType_Ready(&libftdb_ftdbFuncdeclsIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncdeclsIterType);

    if (PyType_Ready(&libftdb_ftdbUnresolvedfuncsIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbUnresolvedfuncsIterType);

    if (PyType_Ready(&libftdb_ftdbGlobalsIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbGlobalsIterType);

    if (PyType_Ready(&libftdb_ftdbTypesIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbTypesIterType);

    if (PyType_Ready(&libftdb_ftdbFopsIterType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFopsIterType);

    if (PyType_Ready(&libftdb_ftdbFuncsEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncsEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncdeclsEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncdeclsEntryType);

    if (PyType_Ready(&libftdb_ftdbGlobalEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbGlobalEntryType);

    if (PyType_Ready(&libftdb_ftdbTypeEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbTypeEntryType);

    if (PyType_Ready(&libftdb_ftdbFopsEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFopsEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncCallInfoEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncCallInfoEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncSwitchInfoEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncSwitchInfoEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncIfInfoEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncIfInfoEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncLocalInfoEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncLocalInfoEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncDerefInfoEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncDerefInfoEntryType);

    if (PyType_Ready(&libftdb_ftdbFuncOffsetrefInfoEntryType) < 0)
        return 0;
    Py_XINCREF(&libftdb_ftdbFuncOffsetrefInfoEntryType);

    if (PyModule_AddObject(m, "ftdb", (PyObject *)&libftdb_ftdbType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbModules", (PyObject *)&libftdb_ftdbModulesType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbModulesIter", (PyObject *)&libftdb_ftdbModulesIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbSources", (PyObject *)&libftdb_ftdbSourcesType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbSourcesIter", (PyObject *)&libftdb_ftdbSourcesIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncs", (PyObject *)&libftdb_ftdbFuncsType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncsIter", (PyObject *)&libftdb_ftdbFuncsIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncEntry", (PyObject *)&libftdb_ftdbFuncsEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncCallInfoEntry", (PyObject *)&libftdb_ftdbFuncCallInfoEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncSwitchInfoEntry", (PyObject *)&libftdb_ftdbFuncSwitchInfoEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncIfInfoEntry", (PyObject *)&libftdb_ftdbFuncIfInfoEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncLocalInfoEntry", (PyObject *)&libftdb_ftdbFuncLocalInfoEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncDerefInfoEntry", (PyObject *)&libftdb_ftdbFuncDerefInfoEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncOffsetrefInfoEntry", (PyObject *)&libftdb_ftdbFuncOffsetrefInfoEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncdecls", (PyObject *)&libftdb_ftdbFuncdeclsType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFuncdeclEntry", (PyObject *)&libftdb_ftdbFuncdeclsEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbUnresolvedfuncs", (PyObject *)&libftdb_ftdbUnresolvedfuncsType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbUnresolvedfuncsIter", (PyObject *)&libftdb_ftdbUnresolvedfuncsIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbGlobals", (PyObject *)&libftdb_ftdbGlobalsType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbGlobalsIter", (PyObject *)&libftdb_ftdbGlobalsIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbGlobalEntry", (PyObject *)&libftdb_ftdbGlobalEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbTypes", (PyObject *)&libftdb_ftdbTypesType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbTypesIter", (PyObject *)&libftdb_ftdbTypesIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbTypeEntry", (PyObject *)&libftdb_ftdbTypeEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFops", (PyObject *)&libftdb_ftdbFopsType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFopsIter", (PyObject *)&libftdb_ftdbFopsIterType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    if (PyModule_AddObject(m, "ftdbFopsEntry", (PyObject *)&libftdb_ftdbFopsEntryType) < 0) {
        Py_DECREF(m);
        return 0;
    }

    libftdb_ftdbError = PyErr_NewException("libftdb.FtdbError", 0, 0);
    Py_XINCREF(libftdb_ftdbError);
    if (PyModule_AddObject(m, "FtdbError", libftdb_ftdbError) < 0) {
        Py_XDECREF(libftdb_ftdbError);
        Py_CLEAR(libftdb_ftdbError);
        Py_DECREF(m);
    }

    return m;
}
