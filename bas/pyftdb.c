#include "pyftdb.h"
#include "libflat.h"

#include <fcntl.h>

static const char* PyString_get_c_str(PyObject* s) {

	if (PyUnicode_Check(s)) {
		PyObject* us = PyUnicode_AsASCIIString(s);
		if (us) {
			return PyBytes_AsString(us);
		}
		else {
			return 0;
		}
	}
	else {
		return (const char*)0xdeadbeef;
	}
}

PyObject *libftdb_ftdbError;

void libftdb_ftdb_dealloc(libftdb_ftdb_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    if (self->init_done) {
    	unflatten_fini();
    }
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_object* self;

    self = (libftdb_ftdb_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	/* Use the 'load' function to initialize the cache */
    	self->init_done = 0;
    	self->ftdb = 0;
    }

    return (PyObject *)self;
}

int libftdb_ftdb_init(libftdb_ftdb_object* self, PyObject* args, PyObject* kwds) {

	PyObject* py_verbose = PyUnicode_FromString("verbose");
	PyObject* py_debug = PyUnicode_FromString("debug");

	if (kwds) {
		if (PyDict_Contains(kwds, py_verbose)) {
			self->verbose = PyLong_AsLong(PyDict_GetItem(kwds,py_verbose));
		}
		if (PyDict_Contains(kwds, py_debug)) {
			self->debug = PyLong_AsLong(PyDict_GetItem(kwds,py_debug));
		}
	}

	Py_DecRef(py_verbose);
	Py_DecRef(py_debug);

    return 0;
}

PyObject* libftdb_ftdb_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	const char* inited = "initialized";
	const char* ninited = "not initialized";
	int written = snprintf(repr,1024,"<ftdb object at %lx : %s",(uintptr_t)self,__self->init_done?inited:ninited);
        if (__self->init_done) {
            written+=snprintf(repr+written,1024-written,"|debug:%d;verbose:%d|",__self->debug,__self->verbose);
	    written+=snprintf(repr+written,1024-written,"funcs_count:%lu;types_count:%lu;global_count:%lu;sources:%lu>",
			__self->ftdb->funcs_count,__self->ftdb->types_count,
			__self->ftdb->globals_count,__self->ftdb->sourceindex_table_count);
        }
        else {
            written+=snprintf(repr+written,1024-written,">");
        }


	return PyUnicode_FromString(repr);
}

int libftdb_ftdb_bool(PyObject* self) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	return __self->init_done;
}

PyObject* libftdb_ftdb_load(libftdb_ftdb_object* self, PyObject* args, PyObject* kwargs ) {

    const char* cache_filename = ".db.json.img";

    if (PyTuple_Size(args)>0) {
    	cache_filename = PyBytes_AsString(PyUnicode_AsASCIIString(PyTuple_GetItem(args,0)));
    }

    PyObject* py_debug = PyUnicode_FromString("debug");
    PyObject* py_quiet = PyUnicode_FromString("quiet");
    PyObject* py_no_map_memory = PyUnicode_FromString("no_map_memory");
    PyObject* py_mp_safe = PyUnicode_FromString("mp_safe");

    int debug = self->debug;
    int quiet = 0;
    int no_map_memory = 0;
    int mp_safe = 0;
    int err=0;

    if (kwargs) {
    	if (PyDict_Contains(kwargs,py_debug)) {
    		debug = PyLong_AsLong(PyDict_GetItem(kwargs,py_debug));
    	}
    	if (PyDict_Contains(kwargs,py_quiet)) {
			quiet = PyLong_AsLong(PyDict_GetItem(kwargs,py_quiet));
		}
    	if (PyDict_Contains(kwargs,py_no_map_memory)) {
    		no_map_memory = PyLong_AsLong(PyDict_GetItem(kwargs,py_no_map_memory));
		}
    	if (PyDict_Contains(kwargs,py_mp_safe)) {
    		mp_safe = PyLong_AsLong(PyDict_GetItem(kwargs,py_mp_safe));
		}
    }

    DBG( debug, "--- libftdb_ftdb_load(\"%s\")\n",cache_filename);

    if (self->init_done) {
    	PyErr_SetString(libftdb_ftdbError, "ftdb cache already initialized");
    	goto done;
	}

    if (no_map_memory) {
    	FILE* in = fopen(cache_filename, "rb");
		if (!in) {
			PyErr_SetString(libftdb_ftdbError, "Cannot open cache file");
			goto done;
		}
		unflatten_init();
		if (quiet)
			flatten_set_option(option_silent);
		if (unflatten_read(in)) {
			PyErr_SetString(libftdb_ftdbError, "Failed to read cache file");
			goto done;
		}
		fclose(in);
    }
    else {
    	int map_fd = open(cache_filename,O_RDWR);
    	if (map_fd<0) {
    		PyErr_SetString(libftdb_ftdbError, "Cannot open cache file");
    		goto done;
    	}
    	unflatten_init();
    	if (quiet)
    		flatten_set_option(option_silent);
    	int map_err;
    	if (!mp_safe) {
    		/* Try to map the cache file to the previously used address
    		 * When it fails map to new address and update all the pointers in the file
    		 * (we're not going concurrently here)
    		 */
    		map_err = unflatten_map(map_fd,0);
    	}
    	else {
    		/* Try to map the cache file to the previously used address
			 * When it fails map to new address privately (do not update the pointers in the file)
			 * (this can save us some trouble when running concurrently)
			 */
    		map_err = unflatten_map_private(map_fd,0);
    	}
    	close(map_fd);
    	if (map_err) {
    		PyErr_SetString(libftdb_ftdbError, "Failed to map cache file");
    		goto done;
    	}
    }

    self->ftdb = ROOT_POINTER_NEXT(const struct ftdb*);
	self->init_done = 1;
	err = 1;

done:
	Py_DecRef(py_debug);
	Py_DecRef(py_quiet);
	Py_DecRef(py_no_map_memory);
	PYASSTR_DECREF(cache_filename);
	if (!err) Py_RETURN_FALSE;
	Py_RETURN_TRUE;
}

PyObject* libftdb_ftdb_get_version(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	return PyUnicode_FromString(__self->ftdb->version);
}

PyObject* libftdb_ftdb_get_module(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	return PyUnicode_FromString(__self->ftdb->module);
}

PyObject* libftdb_ftdb_get_directory(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	return PyUnicode_FromString(__self->ftdb->directory);
}

PyObject* libftdb_ftdb_get_release(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	return PyUnicode_FromString(__self->ftdb->release);
}

PyObject* libftdb_ftdb_get_sources(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PyTuple_SetItem(args,0,PyLong_FromLong((uintptr_t)__self->ftdb));
	PyObject *sources = PyObject_CallObject((PyObject *) &libftdb_ftdbSourcesType, args);
	Py_DECREF(args);
	return sources;
}

PyObject* libftdb_ftdb_get_sources_as_dict(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* sources = PyList_New(0);
	for (unsigned long i=0; i<__self->ftdb->sourceindex_table_count; ++i) {
		PyObject* py_sources_entry = PyDict_New();
		FTDB_SET_ENTRY_ULONG(py_sources_entry,id,i);
		FTDB_SET_ENTRY_STRING(py_sources_entry,name,__self->ftdb->sourceindex_table[i]);
		PyList_Append(sources,py_sources_entry);
		Py_DecRef(py_sources_entry);
	}
	return sources;
}

PyObject* libftdb_ftdb_get_sources_as_list(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* sources = PyList_New(0);
	for (unsigned long i=0; i<__self->ftdb->sourceindex_table_count; ++i) {
		PyObject* py_sources_entry = PyDict_New();
		PyObject* key = PyUnicode_FromString(__self->ftdb->sourceindex_table[i]);
		PyObject* val = PyLong_FromUnsignedLong(i);
		PyDict_SetItem(py_sources_entry,key,val);
		Py_DecRef(key);
		Py_DecRef(val);
		PyList_Append(sources,py_sources_entry);
		Py_DecRef(py_sources_entry);
	}
	return sources;
}

PyObject* libftdb_ftdb_get_modules(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PyTuple_SetItem(args,0,PyLong_FromLong((uintptr_t)__self->ftdb));
	PyObject *modules = PyObject_CallObject((PyObject *) &libftdb_ftdbModulesType, args);
	Py_DECREF(args);
	return modules;
}

PyObject* libftdb_ftdb_get_modules_as_dict(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* modules = PyList_New(0);
	for (unsigned long i=0; i<__self->ftdb->moduleindex_table_count; ++i) {
		PyObject* py_modules_entry = PyDict_New();
		FTDB_SET_ENTRY_ULONG(py_modules_entry,id,i);
		FTDB_SET_ENTRY_STRING(py_modules_entry,name,__self->ftdb->moduleindex_table[i]);
		PyList_Append(modules,py_modules_entry);
		Py_DecRef(py_modules_entry);
	}
	return modules;
}

PyObject* libftdb_ftdb_get_modules_as_list(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* modules = PyList_New(0);
	for (unsigned long i=0; i<__self->ftdb->moduleindex_table_count; ++i) {
		PyObject* py_modules_entry = PyDict_New();
		PyObject* key = PyUnicode_FromString(__self->ftdb->moduleindex_table[i]);
		PyObject* val = PyLong_FromUnsignedLong(i);
		PyDict_SetItem(py_modules_entry,key,val);
		Py_DecRef(key);
		Py_DecRef(val);
		PyList_Append(modules,py_modules_entry);
		Py_DecRef(py_modules_entry);
	}
	return modules;
}

PyObject* libftdb_ftdb_get_funcs(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *funcs = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsType, args);
	Py_DECREF(args);
	return funcs;
}

PyObject* libftdb_ftdb_get_funcdecls(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *funcdecls = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsType, args);
	Py_DECREF(args);
	return funcdecls;
}

PyObject* libftdb_ftdb_get_unresolvedfuncs(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *unresolvedfuncs = PyObject_CallObject((PyObject *) &libftdb_ftdbUnresolvedfuncsType, args);
	Py_DECREF(args);
	return unresolvedfuncs;
}

PyObject* libftdb_ftdb_get_unresolvedfuncs_as_list(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* unresolvedfuncs = PyList_New(0);
	for (unsigned long i=0; i<__self->ftdb->unresolvedfuncs_count; ++i) {
		struct ftdb_unresolvedfunc_entry* entry = &__self->ftdb->unresolvedfuncs[i];
		PyObject* py_unresolvedfuncs_entry = PyDict_New();
		FTDB_SET_ENTRY_STRING(py_unresolvedfuncs_entry,name,entry->name);
		FTDB_SET_ENTRY_ULONG(py_unresolvedfuncs_entry,id,entry->id);
		PyList_Append(unresolvedfuncs,py_unresolvedfuncs_entry);
		Py_DECREF(py_unresolvedfuncs_entry);
	}
	return unresolvedfuncs;
}

PyObject* libftdb_ftdb_get_globals(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *globals = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalsType, args);
	Py_DECREF(args);
	return globals;
}

PyObject* libftdb_ftdb_get_types(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *types = PyObject_CallObject((PyObject *) &libftdb_ftdbTypesType, args);
	Py_DECREF(args);
	return types;
}

PyObject* libftdb_ftdb_get_fops(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *fops = PyObject_CallObject((PyObject *) &libftdb_ftdbFopsType, args);
	Py_DECREF(args);
	return fops;
}

PyObject* libftdb_ftdb_get_fops_as_dict(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* fops = PyDict_New();
	FTDB_SET_ENTRY_ULONG(fops,varn,__self->ftdb->fops.vars_count);
	FTDB_SET_ENTRY_ULONG(fops,sourcen,__self->ftdb->sourceindex_table_count);
	FTDB_SET_ENTRY_ULONG(fops,membern,__self->ftdb->fops.member_count);
	PyObject* py_sources = libftdb_ftdb_get_sources_as_dict(self, closure);
	FTDB_SET_ENTRY_PYOBJECT(fops,sources,py_sources);
	PyObject* py_vars = libftdb_ftdb_get_fops(self, closure);
	FTDB_SET_ENTRY_PYOBJECT(fops,vars,py_vars);
	return fops;
}

PyObject* libftdb_ftdb_get_macroinfo(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PyObject *macroinfo = PyObject_CallObject((PyObject *) &libftdb_ftdbMacroinfoType, args);
	Py_DECREF(args);
	return macroinfo;
}

PyObject* libftdb_ftdb_get_matrix_data_as_dict(struct matrix_data* matrix_data) {

	PyObject* md = PyList_New(0);

	PyObject* data = PyDict_New();
	FTDB_SET_ENTRY_STRING(data,name,"data");
	FTDB_SET_ENTRY_ULONG_ARRAY(data,data,matrix_data->data);
	PyList_Append(md,data);
	Py_DecRef(data);
	PyObject* row_ind = PyDict_New();
	FTDB_SET_ENTRY_STRING(row_ind,name,"row_ind");
	FTDB_SET_ENTRY_ULONG_ARRAY(row_ind,data,matrix_data->row_ind);
	PyList_Append(md,row_ind);
	Py_DecRef(row_ind);
	PyObject* col_ind = PyDict_New();
	FTDB_SET_ENTRY_STRING(col_ind,name,"col_ind");
	FTDB_SET_ENTRY_ULONG_ARRAY(col_ind,data,matrix_data->col_ind);
	PyList_Append(md,col_ind);
	Py_DecRef(col_ind);
	PyObject* matrix_size = PyDict_New();
	FTDB_SET_ENTRY_STRING(matrix_size,name,"matrix_size");
	FTDB_SET_ENTRY_ULONG(matrix_size,data,matrix_data->matrix_size);
	PyList_Append(md,matrix_size);
	Py_DecRef(matrix_size);

	return md;
}

PyObject* libftdb_ftdb_get_funcs_tree_calls_no_asm(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_calls_no_asm) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_calls_no_asm' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_calls_no_asm);
}

PyObject* libftdb_ftdb_get_funcs_tree_calls_no_known(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_calls_no_known) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_calls_no_known' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_calls_no_known);
}

PyObject* libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_calls_no_known_no_asm) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_calls_no_known_no_asm' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_calls_no_known_no_asm);
}

PyObject* libftdb_ftdb_get_funcs_tree_func_calls(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_func_calls) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_func_calls' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_func_calls);
}

PyObject* libftdb_ftdb_get_funcs_tree_func_refs(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_func_refs) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_func_refs' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_func_refs);
}

PyObject* libftdb_ftdb_get_funcs_tree_funrefs_no_asm(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_funrefs_no_asm) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_funrefs_no_asm' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_funrefs_no_asm);
}

PyObject* libftdb_ftdb_get_funcs_tree_funrefs_no_known(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_funrefs_no_known) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_funrefs_no_known' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_funrefs_no_known);
}

PyObject* libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->funcs_tree_funrefs_no_known_no_asm) {
		PyErr_SetString(libftdb_ftdbError, "No 'funcs_tree_funrefs_no_known_no_asm' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->funcs_tree_funrefs_no_known_no_asm);
}

PyObject* libftdb_ftdb_get_globs_tree_globalrefs(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->globs_tree_globalrefs) {
		PyErr_SetString(libftdb_ftdbError, "No 'globs_tree_globalrefs' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->globs_tree_globalrefs);
}

PyObject* libftdb_ftdb_get_types_tree_refs(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->types_tree_refs) {
		PyErr_SetString(libftdb_ftdbError, "No 'types_tree_refs' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->types_tree_refs);
}

PyObject* libftdb_ftdb_get_types_tree_usedrefs(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->types_tree_usedrefs) {
		PyErr_SetString(libftdb_ftdbError, "No 'types_tree_usedrefs' field in ftdb module");
		return 0;
	}

	return libftdb_ftdb_get_matrix_data_as_dict(__self->ftdb->types_tree_usedrefs);
}

PyObject* libftdb_ftdb_get_static_funcs_map(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->static_funcs_map) {
		PyErr_SetString(libftdb_ftdbError, "No 'static_funcs_map' field in ftdb module");
		return 0;
	}

	PyObject* sfm = PyList_New(0);

	for (Py_ssize_t i=0; i<__self->ftdb->static_funcs_map_count; ++i) {
		struct func_map_entry* entry = &__self->ftdb->static_funcs_map[i];
		PyObject* data = PyDict_New();
		FTDB_SET_ENTRY_ULONG(data,id,entry->id);
		FTDB_SET_ENTRY_ULONG_ARRAY(data,fids,entry->fids);
		PyList_Append(sfm,data);
		Py_DecRef(data);
	}

	return sfm;
}

PyObject* libftdb_ftdb_get_init_data(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->init_data) {
		PyErr_SetString(libftdb_ftdbError, "No 'init_data' field in ftdb module");
		return 0;
	}

	PyObject* init_data = PyList_New(0);
	for (Py_ssize_t i=0; i<__self->ftdb->init_data_count; ++i) {
		struct init_data_entry* entry = &__self->ftdb->init_data[i];
		PyObject* py_init_data_entry = PyDict_New();
		FTDB_SET_ENTRY_STRING(py_init_data_entry,name,entry->name);
		FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_init_data_entry,order,entry->order);
		FTDB_SET_ENTRY_STRING(py_init_data_entry,interface,entry->interface);
		PyObject* py_items = PyList_New(0);
		for (Py_ssize_t j=0; j<entry->items_count; ++j) {
			struct init_data_item* item = &entry->items[j];
			PyObject* py_item = PyDict_New();
			FTDB_SET_ENTRY_ULONG_OPTIONAL(py_item,id,item->id);
			FTDB_SET_ENTRY_STRING_ARRAY(py_item,name,item->name);
			FTDB_SET_ENTRY_ULONG_OPTIONAL(py_item,size,item->size);
			if (item->size_dep) {
				PyObject* py_size_dep = PyDict_New();
				FTDB_SET_ENTRY_ULONG(py_size_dep,id,item->size_dep->id);
				FTDB_SET_ENTRY_ULONG(py_size_dep,add,item->size_dep->add);
				FTDB_SET_ENTRY_PYOBJECT(py_item,size_dep,py_size_dep);
			}
			FTDB_SET_ENTRY_STRING_OPTIONAL(py_item,nullterminated,item->nullterminated);
			FTDB_SET_ENTRY_STRING_OPTIONAL(py_item,tagged,item->tagged);
			FTDB_SET_ENTRY_STRING_OPTIONAL(py_item,fuzz,item->fuzz);
			FTDB_SET_ENTRY_STRING_OPTIONAL(py_item,pointer,item->pointer);
			FTDB_SET_ENTRY_INT64_OPTIONAL(py_item,max_value,item->max_value);
			FTDB_SET_ENTRY_INT64_OPTIONAL(py_item,min_value,item->min_value);
			FTDB_SET_ENTRY_INT64_OPTIONAL(py_item,value,item->value);
			FTDB_SET_ENTRY_STRING_OPTIONAL(py_item,user_name,item->user_name);
			PyList_Append(py_items,py_item);
			Py_DecRef(py_item);
		}
		FTDB_SET_ENTRY_PYOBJECT(py_init_data_entry,items,py_items);
		PyList_Append(init_data,py_init_data_entry);
		Py_DecRef(py_init_data_entry);
	}

	return init_data;
}

PyObject* libftdb_ftdb_get_known_data(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->known_data) {
		PyErr_SetString(libftdb_ftdbError, "No 'known_data' field in ftdb module");
		return 0;
	}

    PyObject* py_known_data_container = PyList_New(0);
	PyObject* py_known_data = PyDict_New();
	FTDB_SET_ENTRY_STRING(py_known_data,version,__self->ftdb->known_data->version);
	FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data,func_ids,__self->ftdb->known_data->func_ids);
	FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data,builtin_ids,__self->ftdb->known_data->builtin_ids);
	FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data,asm_ids,__self->ftdb->known_data->asm_ids);
	FTDB_SET_ENTRY_STRING_ARRAY(py_known_data,lib_funcs,__self->ftdb->known_data->lib_funcs);
	FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data,lib_funcs_ids,__self->ftdb->known_data->lib_funcs_ids);
	FTDB_SET_ENTRY_ULONG_ARRAY(py_known_data,always_inc_funcs_ids,__self->ftdb->known_data->always_inc_funcs_ids);
	FTDB_SET_ENTRY_STRING(py_known_data,source_root,__self->ftdb->known_data->source_root);
    PyList_Append(py_known_data_container,py_known_data);
    Py_DecRef(py_known_data);

	return py_known_data_container;
}

PyObject* libftdb_ftdb_get_BAS(PyObject* self, void* closure) {

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;

	if (!__self->ftdb->BAS_data) {
		PyErr_SetString(libftdb_ftdbError, "No 'BAS' field in ftdb module");
		return 0;
	}

	PyObject* py_BAS = PyList_New(0);
	for (Py_ssize_t i=0; i<__self->ftdb->BAS_data_count; ++i) {
		struct BAS_item* entry = &__self->ftdb->BAS_data[i];
		PyObject* py_BAS_entry = PyDict_New();
		FTDB_SET_ENTRY_STRING(py_BAS_entry,loc,entry->loc);
		FTDB_SET_ENTRY_STRING_ARRAY(py_BAS_entry,entries,entry->entries);
		PyList_Append(py_BAS,py_BAS_entry);
		Py_DecRef(py_BAS_entry);
	}

	return py_BAS;
}

PyObject* libftdb_ftdb_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"version")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_version(self,0);
	}
	else if (!strcmp(attr,"module")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_module(self,0);
	}
	else if (!strcmp(attr,"directory")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_directory(self,0);
	}
	else if (!strcmp(attr,"release")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_release(self,0);
	}
	else if (!strcmp(attr,"sources")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_sources_as_list(self,0);
	}
	else if (!strcmp(attr,"source_info")) {
		PYASSTR_DECREF(attr);
		return libftdb_ftdb_get_sources_as_dict(self,0);
	}
	else if (!strcmp(attr,"modules")) {
		PYASSTR_DECREF(attr);
		return libftdb_ftdb_get_modules_as_list(self,0);
	}
	else if (!strcmp(attr,"module_info")) {
		PYASSTR_DECREF(attr);
		return libftdb_ftdb_get_modules_as_dict(self,0);
	}
	else if (!strcmp(attr,"funcs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs(self,0);
	}
	else if (!strcmp(attr,"funcdecls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcdecls(self,0);
	}
	else if (!strcmp(attr,"unresolvedfuncs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_unresolvedfuncs_as_list(self,0);
	}
	else if (!strcmp(attr,"globals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_globals(self,0);
	}
	else if (!strcmp(attr,"types")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_types(self,0);
	}
	else if (!strcmp(attr,"fops")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_fops(self,0);
	}
	else if (!strcmp(attr,"macroinfo")) {
		PYASSTR_DECREF(attr);
		return libftdb_ftdb_get_macroinfo(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_calls_no_asm")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_calls_no_asm(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_calls_no_known")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_calls_no_known(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_calls_no_known_no_asm")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_calls_no_known_no_asm(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_func_calls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_func_calls(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_func_refs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_func_refs(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_funrefs_no_asm")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_funrefs_no_asm(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_funrefs_no_known")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_funrefs_no_known(self,0);
	}
	else if (!strcmp(attr,"funcs_tree_funrefs_no_known_no_asm")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_funcs_tree_funrefs_no_known_no_asm(self,0);
	}
	else if (!strcmp(attr,"globs_tree_globalrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_globs_tree_globalrefs(self,0);
	}
	else if (!strcmp(attr,"types_tree_refs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_types_tree_refs(self,0);
	}
	else if (!strcmp(attr,"types_tree_usedrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_types_tree_usedrefs(self,0);
	}
	else if (!strcmp(attr,"static_funcs_map")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_static_funcs_map(self,0);
	}
	else if (!strcmp(attr,"init_data")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_init_data(self,0);
	}
	else if (!strcmp(attr,"known_data")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_known_data(self,0);
	}
	else if (!strcmp(attr,"BAS")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_get_BAS(self,0);
	}
	else if (!strcmp(attr,"funcdecln")) {
		PYASSTR_DECREF(attr);
		return PyLong_FromUnsignedLong(__self->ftdb->funcdecls_count);
	}
	else if (!strcmp(attr,"typen")) {
		PYASSTR_DECREF(attr);
		return PyLong_FromUnsignedLong(__self->ftdb->types_count);
	}
	else if (!strcmp(attr,"sourcen")) {
		PYASSTR_DECREF(attr);
		return PyLong_FromUnsignedLong(__self->ftdb->sourceindex_table_count);
	}
	else if (!strcmp(attr,"globaln")) {
		PYASSTR_DECREF(attr);
		return PyLong_FromUnsignedLong(__self->ftdb->globals_count);
	}
	else if (!strcmp(attr,"unresolvedfuncn")) {
		PYASSTR_DECREF(attr);
		return PyLong_FromUnsignedLong(__self->ftdb->unresolvedfuncs_count);
	}
	else if (!strcmp(attr,"funcn")) {
		PYASSTR_DECREF(attr);
		return PyLong_FromUnsignedLong(__self->ftdb->funcs_count);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_object* __self = (libftdb_ftdb_object*)self;
	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"version")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"module")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"directory")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"release")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"sources")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->sourceindex_table;
	}
	else if (!strcmp(attr,"source_info")) {
		PYASSTR_DECREF(attr);
		return !!__self->ftdb->sourceindex_table;
	}
	else if (!strcmp(attr,"modules")) {
		PYASSTR_DECREF(attr);
		return !!__self->ftdb->moduleindex_table;
	}
	else if (!strcmp(attr,"module_info")) {
		PYASSTR_DECREF(attr);
		return !!__self->ftdb->moduleindex_table;
	}
	else if (!strcmp(attr,"funcs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"funcdecls")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"unresolvedfuncs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"globals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"types")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"fops")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"funcs_tree_calls_no_asm")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_calls_no_asm;
	}
	else if (!strcmp(attr,"funcs_tree_calls_no_known")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_calls_no_known;
	}
	else if (!strcmp(attr,"funcs_tree_calls_no_known_no_asm")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_calls_no_known_no_asm;
	}
	else if (!strcmp(attr,"funcs_tree_func_calls")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_func_calls;
	}
	else if (!strcmp(attr,"funcs_tree_func_refs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_func_refs;
	}
	else if (!strcmp(attr,"funcs_tree_funrefs_no_asm")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_funrefs_no_asm;
	}
	else if (!strcmp(attr,"funcs_tree_funrefs_no_known")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_funrefs_no_known;
	}
	else if (!strcmp(attr,"funcs_tree_funrefs_no_known_no_asm")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->funcs_tree_funrefs_no_known_no_asm;
	}
	else if (!strcmp(attr,"globs_tree_globalrefs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->globs_tree_globalrefs;
	}
	else if (!strcmp(attr,"types_tree_refs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->types_tree_refs;
	}
	else if (!strcmp(attr,"types_tree_usedrefs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->types_tree_usedrefs;
	}
	else if (!strcmp(attr,"static_funcs_map")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->static_funcs_map;
	}
	else if (!strcmp(attr,"init_data")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->init_data;
	}
	else if (!strcmp(attr,"known_data")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->known_data;
	}
	else if (!strcmp(attr,"BAS")) {
		PYASSTR_DECREF(attr);
	    return !!__self->ftdb->BAS_data;
	}
	else if (!strcmp(attr,"funcdecln")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"typen")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"sourcen")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"globaln")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"unresolvedfuncn")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"funcn")) {
		PYASSTR_DECREF(attr);
		return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_sources_dealloc(libftdb_ftdb_sources_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_sources_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_sources_object* self;

    self = (libftdb_ftdb_sources_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_sources_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_sources_object* __self = (libftdb_ftdb_sources_object*)self;
	int written = snprintf(repr,1024,"<ftdbSources object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld sources>",__self->ftdb->sourceindex_table_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_sources_sq_length(PyObject* self) {

	libftdb_ftdb_sources_object* __self = (libftdb_ftdb_sources_object*)self;
	return __self->ftdb->sourceindex_table_count;
}

PyObject* libftdb_ftdb_sources_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_sources_object* __self = (libftdb_ftdb_sources_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long index = PyLong_AsUnsignedLong(slice);
		if (index>=__self->ftdb->sourceindex_table_count) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"source table index out of range: %ld\n",index);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		return PyUnicode_FromString(__self->ftdb->sourceindex_table[index]);
	}
	else if (PyUnicode_Check(slice)) {
		const char* key = PyString_get_c_str(slice);
		struct stringRefMap_node* node = stringRefMap_search(&__self->ftdb->sourcemap,key);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"source table file missing: %s\n",key);
			PYASSTR_DECREF(key);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(key);
		return PyLong_FromUnsignedLong(node->value);
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_sources_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in source contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_sources_object* __self = (libftdb_ftdb_sources_object*)self;
	const char* ckey = PyString_get_c_str(key);
	struct stringRefMap_node* node = stringRefMap_search(&__self->ftdb->sourcemap,ckey);
	PYASSTR_DECREF(ckey);
	if (node) return 1;
	return 0;
}

PyObject* libftdb_ftdb_sources_getiter(PyObject* self) {

	libftdb_ftdb_sources_object* __self = (libftdb_ftdb_sources_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbSourcesIterType, args);
	Py_DecRef(args);
	return iter;
}

void libftdb_ftdb_sources_iter_dealloc(libftdb_ftdb_sources_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_sources_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_sources_iter_object* self;

    self = (libftdb_ftdb_sources_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_sources_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_sources_iter_sq_length(PyObject* self) {

	libftdb_ftdb_sources_iter_object* __self = (libftdb_ftdb_sources_iter_object*)self;
	return __self->ftdb->sourceindex_table_count;
}

PyObject* libftdb_ftdb_sources_iter_next(PyObject *self) {

	libftdb_ftdb_sources_iter_object* __self = (libftdb_ftdb_sources_iter_object*)self;

	if (__self->index >= __self->ftdb->sourceindex_table_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,__self->index);
	PYTUPLE_SET_STR(args,1,__self->ftdb->sourceindex_table[__self->index]);
	__self->index++;
	return args;
}

void libftdb_ftdb_modules_dealloc(libftdb_ftdb_modules_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_modules_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_modules_object* self;

    self = (libftdb_ftdb_modules_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_modules_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_modules_object* __self = (libftdb_ftdb_modules_object*)self;
	int written = snprintf(repr,1024,"<ftdbModules object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld modules>",__self->ftdb->moduleindex_table_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_modules_sq_length(PyObject* self) {

	libftdb_ftdb_modules_object* __self = (libftdb_ftdb_modules_object*)self;
	return __self->ftdb->moduleindex_table_count;
}

PyObject* libftdb_ftdb_modules_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_modules_object* __self = (libftdb_ftdb_modules_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long index = PyLong_AsUnsignedLong(slice);
		if (index>=__self->ftdb->moduleindex_table_count) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"module table index out of range: %ld\n",index);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		return PyUnicode_FromString(__self->ftdb->moduleindex_table[index]);
	}
	else if (PyUnicode_Check(slice)) {
		const char* key = PyString_get_c_str(slice);
		struct stringRefMap_node* node = stringRefMap_search(&__self->ftdb->modulemap,key);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"module table file missing: %s\n",key);
			PYASSTR_DECREF(key);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(key);
		return PyLong_FromUnsignedLong(node->value);
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_modules_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in module contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_modules_object* __self = (libftdb_ftdb_modules_object*)self;
	const char* ckey = PyString_get_c_str(key);
	struct stringRefMap_node* node = stringRefMap_search(&__self->ftdb->modulemap,ckey);
	PYASSTR_DECREF(ckey);
	if (node) return 1;
	return 0;
}

PyObject* libftdb_ftdb_modules_getiter(PyObject* self) {

	libftdb_ftdb_modules_object* __self = (libftdb_ftdb_modules_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbModulesIterType, args);
	Py_DecRef(args);
	return iter;
}

void libftdb_ftdb_modules_iter_dealloc(libftdb_ftdb_modules_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_modules_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_modules_iter_object* self;

    self = (libftdb_ftdb_modules_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_modules_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_modules_iter_sq_length(PyObject* self) {

	libftdb_ftdb_modules_iter_object* __self = (libftdb_ftdb_modules_iter_object*)self;
	return __self->ftdb->moduleindex_table_count;
}

PyObject* libftdb_ftdb_modules_iter_next(PyObject *self) {

	libftdb_ftdb_modules_iter_object* __self = (libftdb_ftdb_modules_iter_object*)self;

	if (__self->index >= __self->ftdb->moduleindex_table_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,__self->index);
	PYTUPLE_SET_STR(args,1,__self->ftdb->sourceindex_table[__self->index]);
	__self->index++;
	return args;
}

void libftdb_ftdb_macroinfo_dealloc(libftdb_ftdb_macroinfo_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_macroinfo_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_macroinfo_object* self;

    self = (libftdb_ftdb_macroinfo_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_macroinfo_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_macroinfo_object* __self = (libftdb_ftdb_macroinfo_object*)self;
	int written = snprintf(repr,1024,"<ftdbMacroinfo object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld macros>",__self->ftdb->macroinfo_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_macroinfo_sq_length(PyObject* self) {

	libftdb_ftdb_macroinfo_object* __self = (libftdb_ftdb_macroinfo_object*)self;
	return __self->ftdb->macroinfo_count;
}

PyObject* libftdb_ftdb_macroinfo_getiter(PyObject* self) {

	libftdb_ftdb_macroinfo_object* __self = (libftdb_ftdb_macroinfo_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbMacroinfoIterType, args);
	Py_DecRef(args);
	return iter;
}

static inline PyObject* get_py_macroinfo(const struct ftdb* ftdb, unsigned long index) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (index>=ftdb->macroinfo_count) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"macroinfo table index out of range: %ld\n",index);
		PyErr_SetString(libftdb_ftdbError, errmsg);
		return 0;
	}
	struct macroinfo_item* macroinfo_item = &ftdb->macroinfo[index];
	PyObject* py_macroinfo_entry = PyDict_New();
	FTDB_SET_ENTRY_STRING(py_macroinfo_entry,name,macroinfo_item->name);
	PyObject* py_occurences = PyList_New(0);
	for (unsigned long i=0; i<macroinfo_item->occurences_count; ++i) {
		PyObject* py_occurence = PyDict_New();
		struct macro_occurence* occurence = &macroinfo_item->occurences[i];
		FTDB_SET_ENTRY_STRING(py_occurence,loc,occurence->loc);
		FTDB_SET_ENTRY_STRING(py_occurence,expanded,occurence->expanded);
		FTDB_SET_ENTRY_ULONG(py_occurence,fid,occurence->fid);
		PyList_Append(py_occurences,py_occurence);
		Py_DecRef(py_occurence);
	}
	FTDB_SET_ENTRY_PYOBJECT(py_macroinfo_entry,occurences,py_occurences);
	return py_macroinfo_entry;
}

PyObject* libftdb_ftdb_macroinfo_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_macroinfo_object* __self = (libftdb_ftdb_macroinfo_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long index = PyLong_AsUnsignedLong(slice);
		return get_py_macroinfo(__self->ftdb,index);
	}
	else if (PyUnicode_Check(slice)) {
		const char* key = PyString_get_c_str(slice);
		struct stringRefMap_node* node = stringRefMap_search(&__self->ftdb->macromap,key);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"macroinfo table file missing: %s\n",key);
			PYASSTR_DECREF(key);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(key);
		return get_py_macroinfo(__self->ftdb,node->value);
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_macroinfo_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in macroinfo contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_macroinfo_object* __self = (libftdb_ftdb_macroinfo_object*)self;
	const char* ckey = PyString_get_c_str(key);
	struct stringRefMap_node* node = stringRefMap_search(&__self->ftdb->macromap,ckey);
	PYASSTR_DECREF(ckey);
	if (node) return 1;
	return 0;
}

PyObject* libftdb_ftdb_macroinfo_keys(libftdb_ftdb_macroinfo_object* self, PyObject* args, PyObject* kwargs) {

	PyObject* macroinfo_keys = PyList_New(0);
    struct rb_node * p = rb_first(&self->ftdb->macromap);
    while(p) {
        struct stringRefMap_node* data = (struct stringRefMap_node*)p;
        PyObject* key = PyUnicode_FromString(data->key);
        PyList_Append(macroinfo_keys,key);
        Py_DecRef(key);
        p = rb_next(p);
    }
	return macroinfo_keys;
}

void libftdb_ftdb_macroinfo_iter_dealloc(libftdb_ftdb_macroinfo_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_macroinfo_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_macroinfo_iter_object* self;

    self = (libftdb_ftdb_macroinfo_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_macroinfo_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_macroinfo_iter_sq_length(PyObject* self) {

	libftdb_ftdb_macroinfo_iter_object* __self = (libftdb_ftdb_macroinfo_iter_object*)self;
	return __self->ftdb->macroinfo_count;
}

PyObject* libftdb_ftdb_macroinfo_iter_next(PyObject *self) {

	libftdb_ftdb_macroinfo_iter_object* __self = (libftdb_ftdb_macroinfo_iter_object*)self;

	if (__self->index >= __self->ftdb->macroinfo_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	return get_py_macroinfo(__self->ftdb,__self->index++);
}

void libftdb_ftdb_funcs_dealloc(libftdb_ftdb_funcs_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_funcs_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_funcs_object* self;

    self = (libftdb_ftdb_funcs_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_funcs_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_funcs_object* __self = (libftdb_ftdb_funcs_object*)self;
	int written = snprintf(repr,1024,"<ftdbFuncs object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld funcs>",__self->ftdb->funcs_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_funcs_sq_length(PyObject* self) {

	libftdb_ftdb_funcs_object* __self = (libftdb_ftdb_funcs_object*)self;
	return __self->ftdb->funcs_count;
}

PyObject* libftdb_ftdb_funcs_getiter(PyObject* self) {

	libftdb_ftdb_funcs_object* __self = (libftdb_ftdb_funcs_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0); /* index */
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsIterType, args);

	Py_DecRef(args);
	return iter;
}

PyObject* libftdb_ftdb_funcs_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_funcs_object* __self = (libftdb_ftdb_funcs_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long id = PyLong_AsUnsignedLong(slice);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&__self->ftdb->frefmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_func_entry* func_entry = (struct ftdb_func_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else if (PyUnicode_Check(slice)) {
		const char* hash = PyString_get_c_str(slice);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&__self->ftdb->fhrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_func_entry* func_entry = (struct ftdb_func_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcs_entry_by_id(libftdb_ftdb_funcs_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->frefmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_func_entry* func_entry = (struct ftdb_func_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcs_contains_id(libftdb_ftdb_funcs_object *self, PyObject *args) {

	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->frefmap, id);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcs_entry_by_hash(libftdb_ftdb_funcs_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->fhrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_func_entry* func_entry = (struct ftdb_func_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcs_contains_hash(libftdb_ftdb_funcs_object *self, PyObject *args) {

	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->fhrefmap, hash);
		PYASSTR_DECREF(hash);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcs_entry_by_name(libftdb_ftdb_funcs_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_name = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_name)) {
		const char* name = PyString_get_c_str(py_name);
		struct stringRef_entryListMap_node* node = stringRef_entryListMap_search(&self->ftdb->fnrefmap, name);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function name not present in the array: %s\n",name);
			PYASSTR_DECREF(name);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(name);
		struct ftdb_func_entry** func_entry_list = (struct ftdb_func_entry**)node->entry_list;
		PyObject* entry_list = PyList_New(0);
		for (unsigned long i=0; i<node->entry_count; ++i) {
			struct ftdb_func_entry* func_entry = (struct ftdb_func_entry*)(func_entry_list[i]);
			PyObject* args = PyTuple_New(2);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
			PYTUPLE_SET_ULONG(args,1,func_entry->__index);
			PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsEntryType, args);
			Py_DecRef(args);
			PyList_Append(entry_list,entry);
			Py_DecRef(entry);
		}
		return entry_list;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcs_contains_name(libftdb_ftdb_funcs_object *self, PyObject *args) {

	PyObject* py_name = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_name)) {
		const char* name = PyString_get_c_str(py_name);
		struct stringRef_entryListMap_node* node = stringRef_entryListMap_search(&self->ftdb->fnrefmap, name);
		PYASSTR_DECREF(name);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_funcs_sq_contains(PyObject* self, PyObject* key) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	libftdb_ftdb_funcs_object* __self = (libftdb_ftdb_funcs_object*)self;

	if (PyUnicode_Check(key)) {
		/* Check name */
		PyObject* v = libftdb_ftdb_funcs_contains_name(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}
	else if (PyLong_Check(key)) {
		/* Check id */
		PyObject* v = libftdb_ftdb_funcs_contains_id(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}
	else if (PyTuple_Check(key)) {
		/* Check hash */
		if (PyTuple_Size(key)!=1) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid size of contains check tuple: %ld (should be 1)",PyTuple_Size(key));
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PyObject* v = libftdb_ftdb_funcs_contains_hash(__self,PyTuple_GetItem(key,0));
		if (v) {
			return PyObject_IsTrue(v);
		}
	}

	PyErr_SetString(libftdb_ftdbError, "Invalid key type in contains check (not a str, tuple or integer)");
	return 0;
}

void libftdb_ftdb_funcs_iter_dealloc(libftdb_ftdb_funcs_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_funcs_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_funcs_iter_object* self;

    self = (libftdb_ftdb_funcs_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_funcs_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_funcs_iter_sq_length(PyObject* self) {

	libftdb_ftdb_funcs_iter_object* __self = (libftdb_ftdb_funcs_iter_object*)self;
	return __self->ftdb->funcs_count;
}

PyObject* libftdb_ftdb_funcs_iter_next(PyObject *self) {

	libftdb_ftdb_funcs_iter_object* __self = (libftdb_ftdb_funcs_iter_object*)self;

	if (__self->index >= __self->ftdb->funcs_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PYTUPLE_SET_ULONG(args,1,__self->index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncsEntryType, args);
	Py_DecRef(args);
	__self->index++;
	return entry;
}

void libftdb_ftdb_func_entry_dealloc(libftdb_ftdb_func_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_func_entry_object* self;

    self = (libftdb_ftdb_func_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    	unsigned long index = PyLong_AsLong(PyTuple_GetItem(args,1));
    	if (index>=self->ftdb->funcs_count) {
    		PyErr_SetString(libftdb_ftdbError, "func entry index out of bounds");
    		return 0;
    	}
    	self->index = index;
    	self->entry = &self->ftdb->funcs[self->index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_entry_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;
	int written = snprintf(repr,1024,"<ftdbFuncEntry object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"id:%lu|name:%s>",__self->entry->id,__self->entry->name);

	return PyUnicode_FromString(repr);
}

/* TODO: leaks */
PyObject* libftdb_ftdb_func_entry_json(libftdb_ftdb_func_entry_object *self, PyObject *args) {

	PyObject* json_entry = PyDict_New();

	FTDB_SET_ENTRY_STRING(json_entry,name,self->entry->name);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,namespace,self->entry->__namespace);
	FTDB_SET_ENTRY_ULONG(json_entry,id,self->entry->id);
	FTDB_SET_ENTRY_ULONG(json_entry,fid,self->entry->fid);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,fids,self->entry->fids);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,mids,self->entry->mids);
	FTDB_SET_ENTRY_ULONG(json_entry,nargs,self->entry->nargs);
	FTDB_SET_ENTRY_BOOL(json_entry,variadic,self->entry->isvariadic);
	FTDB_SET_ENTRY_STRING(json_entry,firstNonDeclStmt,self->entry->firstNonDeclStmt);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,inline,self->entry->isinline);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,template,self->entry->istemplate);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,classOuterFn,self->entry->classOuterFn);
	FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry,linkage,self->entry->linkage,functionLinkage);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,member,self->entry->ismember);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,class,self->entry->__class);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry,classid,self->entry->classid);
	FTDB_SET_ENTRY_STRING_ARRAY(json_entry,attributes,self->entry->attributes);
	FTDB_SET_ENTRY_STRING(json_entry,hash,self->entry->hash);
	FTDB_SET_ENTRY_STRING(json_entry,cshash,self->entry->cshash);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,template_parameters,self->entry->template_parameters);
	FTDB_SET_ENTRY_STRING(json_entry,body,self->entry->body);
	FTDB_SET_ENTRY_STRING(json_entry,unpreprocessed_body,self->entry->unpreprocessed_body);
	FTDB_SET_ENTRY_STRING(json_entry,declbody,self->entry->declbody);
	FTDB_SET_ENTRY_STRING(json_entry,signature,self->entry->signature);
	FTDB_SET_ENTRY_STRING(json_entry,declhash,self->entry->declhash);
	FTDB_SET_ENTRY_STRING(json_entry,location,self->entry->location);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,abs_location,self->entry->abs_location);
	FTDB_SET_ENTRY_STRING(json_entry,start_loc,self->entry->start_loc);
	FTDB_SET_ENTRY_STRING(json_entry,end_loc,self->entry->end_loc);
	FTDB_SET_ENTRY_ULONG(json_entry,refcount,self->entry->refcount);

	PyObject* literals = PyDict_New();
	FTDB_SET_ENTRY_INT64_ARRAY(literals,integer,self->entry->integer_literals);
	FTDB_SET_ENTRY_INT_ARRAY(literals,character,self->entry->character_literals);
	FTDB_SET_ENTRY_FLOAT_ARRAY(literals,floating,self->entry->floating_literals);
	FTDB_SET_ENTRY_STRING_ARRAY(literals,string,self->entry->string_literals);
	FTDB_SET_ENTRY_PYOBJECT(json_entry,literals,literals);

	FTDB_SET_ENTRY_ULONG(json_entry,declcount,self->entry->declcount);

	PyObject* taint = PyDict_New();
	for (unsigned long i=0; i<self->entry->taint_count; ++i) {
		struct taint_data* taint_data = &self->entry->taint[i];
		PyObject* taint_list = PyList_New(0);
		for (unsigned long j=0; j<taint_data->taint_list_count; ++j) {
			struct taint_element* taint_element = &taint_data->taint_list[j];
			PyObject* py_taint_element = PyList_New(0);
			PyList_Append(py_taint_element,PyLong_FromUnsignedLong(taint_element->taint_level));
			PyList_Append(py_taint_element,PyLong_FromUnsignedLong(taint_element->varid));
			PyList_Append(taint_list,py_taint_element);
			Py_DecRef(py_taint_element);
		}
		PyDict_SetItem(taint,PyUnicode_FromFormat("%lu",i),taint_list);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,taint,taint);

	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,calls,self->entry->calls);
	PyObject* call_info = PyList_New(0);
	for (unsigned long i=0; i<self->entry->calls_count; ++i) {
		struct call_info* call_info_entry = &self->entry->call_info[i];
		PyObject* py_callinfo = PyDict_New();
		FTDB_SET_ENTRY_STRING(py_callinfo,start,call_info_entry->start);
		FTDB_SET_ENTRY_STRING(py_callinfo,end,call_info_entry->end);
		FTDB_SET_ENTRY_ULONG(py_callinfo,ord,call_info_entry->ord);
		FTDB_SET_ENTRY_STRING(py_callinfo,expr,call_info_entry->expr);
		FTDB_SET_ENTRY_STRING(py_callinfo,loc,call_info_entry->loc);
		FTDB_SET_ENTRY_ULONG_ARRAY(py_callinfo,args,call_info_entry->args);
		PyList_Append(call_info,py_callinfo);
		Py_DecRef(py_callinfo);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,call_info,call_info);

	PyObject* callrefs = PyList_New(0);
	for (unsigned long i=0; i<self->entry->calls_count; ++i) {
		struct callref_info* callref_info = &self->entry->callrefs[i];
		PyObject* py_callref_info = PyList_New(0);
		for (unsigned long j=0; j<callref_info->callarg_count; ++j) {
			struct callref_data* callref_data = &callref_info->callarg[j];
			PyObject* py_callref_data = PyDict_New();
			FTDB_SET_ENTRY_ENUM_TO_STRING(py_callref_data,type,callref_data->type,exprType);
			FTDB_SET_ENTRY_ULONG(py_callref_data,pos,callref_data->pos);
			if (callref_data->id) {
				FTDB_SET_ENTRY_ULONG_OPTIONAL(py_callref_data,id,callref_data->id);
			}
			else if (callref_data->integer_literal) {
				FTDB_SET_ENTRY_INT64_OPTIONAL(py_callref_data,id,callref_data->integer_literal);
			}
			else if (callref_data->character_literal) {
				FTDB_SET_ENTRY_INT_OPTIONAL(py_callref_data,id,callref_data->character_literal);
			}
			else if (callref_data->floating_literal) {
				FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_callref_data,id,callref_data->floating_literal);
			}
			else if (callref_data->string_literal) {
				FTDB_SET_ENTRY_STRING_OPTIONAL(py_callref_data,id,callref_data->string_literal);
			}
			PyList_Append(py_callref_info,py_callref_data);
			Py_DecRef(py_callref_data);
		}
		PyList_Append(callrefs,py_callref_info);
		Py_DecRef(py_callref_info);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,callrefs,callrefs);

	PyObject* refcalls = PyList_New(0);
	for (unsigned long i=0; i<self->entry->refcalls_count; ++i) {
		struct refcall* refcall = &self->entry->refcalls[i];
		PyObject* refcall_info = PyList_New(0);
		if (refcall->ismembercall) {
			PyList_Append(refcall_info,PyLong_FromUnsignedLong(refcall->fid));
			PyList_Append(refcall_info,PyLong_FromUnsignedLong(refcall->cid));
			PyList_Append(refcall_info,PyLong_FromUnsignedLong(refcall->field_index));
		}
		else {
			PyList_Append(refcall_info,PyLong_FromUnsignedLong(refcall->fid));
		}
		PyList_Append(refcalls,refcall_info);
		Py_DecRef(refcall_info);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,refcalls,refcalls);

	PyObject* refcall_info = PyList_New(0);
	for (unsigned long i=0; i<self->entry->refcalls_count; ++i) {
		struct call_info* refcall_info_entry = &self->entry->refcall_info[i];
		PyObject* py_refcallinfo = PyDict_New();
		FTDB_SET_ENTRY_STRING(py_refcallinfo,start,refcall_info_entry->start);
		FTDB_SET_ENTRY_STRING(py_refcallinfo,end,refcall_info_entry->end);
		FTDB_SET_ENTRY_ULONG(py_refcallinfo,ord,refcall_info_entry->ord);
		FTDB_SET_ENTRY_STRING(py_refcallinfo,expr,refcall_info_entry->expr);
		FTDB_SET_ENTRY_STRING_OPTIONAL(py_refcallinfo,loc,refcall_info_entry->loc);
		FTDB_SET_ENTRY_ULONG_ARRAY(py_refcallinfo,args,refcall_info_entry->args);
		PyList_Append(refcall_info,py_refcallinfo);
		Py_DecRef(py_refcallinfo);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,refcall_info,refcall_info);

	PyObject* refcallrefs = PyList_New(0);
	for (unsigned long i=0; i<self->entry->refcalls_count; ++i) {
		struct callref_info* refcallref_info = &self->entry->refcallrefs[i];
		PyObject* py_refcallref_info = PyList_New(0);
		for (unsigned long j=0; j<refcallref_info->callarg_count; ++j) {
			struct callref_data* refcallref_data = &refcallref_info->callarg[j];
			PyObject* py_refcallref_data = PyDict_New();
			FTDB_SET_ENTRY_ENUM_TO_STRING(py_refcallref_data,type,refcallref_data->type,exprType);
			FTDB_SET_ENTRY_ULONG(py_refcallref_data,pos,refcallref_data->pos);
			if (refcallref_data->id) {
				FTDB_SET_ENTRY_ULONG_OPTIONAL(py_refcallref_data,id,refcallref_data->id);
			}
			else if (refcallref_data->integer_literal) {
				FTDB_SET_ENTRY_INT64_OPTIONAL(py_refcallref_data,id,refcallref_data->integer_literal);
			}
			else if (refcallref_data->character_literal) {
				FTDB_SET_ENTRY_INT_OPTIONAL(py_refcallref_data,id,refcallref_data->character_literal);
			}
			else if (refcallref_data->floating_literal) {
				FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_refcallref_data,id,refcallref_data->floating_literal);
			}
			else if (refcallref_data->string_literal) {
				FTDB_SET_ENTRY_STRING_OPTIONAL(py_refcallref_data,id,refcallref_data->string_literal);
			}
			PyList_Append(py_refcallref_info,py_refcallref_data);
			Py_DecRef(py_refcallref_data);
		}
		PyList_Append(refcallrefs,py_refcallref_info);
		Py_DecRef(py_refcallref_info);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,refcallrefs,refcallrefs);

	PyObject* switches = PyList_New(0);
	for (unsigned long i=0; i<self->entry->switches_count; ++i) {
		struct switch_info* switch_info_entry = &self->entry->switches[i];
		PyObject* py_switch_info = PyDict_New();
		FTDB_SET_ENTRY_STRING(py_switch_info,condition,switch_info_entry->condition);
		PyObject* cases = PyList_New(0);
		for (unsigned long j=0; j<switch_info_entry->cases_count; ++j) {
			struct case_info* case_info = &switch_info_entry->cases[j];
			PyObject* py_case_info = PyList_New(0);
			PyList_Append(py_case_info,PyLong_FromUnsignedLong(case_info->lhs.expr_value));
			PyObject* enum_code = PyUnicode_FromString(case_info->lhs.enum_code);
			PyList_Append(py_case_info,enum_code);
			Py_DecRef(enum_code);
			PyObject* macro_code = PyUnicode_FromString(case_info->lhs.macro_code);
			PyList_Append(py_case_info,macro_code);
			Py_DecRef(macro_code);
			PyObject* raw_code = PyUnicode_FromString(case_info->lhs.raw_code);
			PyList_Append(py_case_info,raw_code);
			Py_DecRef(raw_code);
			if (case_info->rhs) {
				PyList_Append(py_case_info,PyLong_FromUnsignedLong(case_info->rhs->expr_value));
				PyObject* enum_code = PyUnicode_FromString(case_info->rhs->enum_code);
				PyList_Append(py_case_info,enum_code);
				Py_DecRef(enum_code);
				PyObject* macro_code = PyUnicode_FromString(case_info->rhs->macro_code);
				PyList_Append(py_case_info,macro_code);
				Py_DecRef(macro_code);
				PyObject* raw_code = PyUnicode_FromString(case_info->rhs->raw_code);
				PyList_Append(py_case_info,raw_code);
				Py_DecRef(raw_code);
			}
		}
		FTDB_SET_ENTRY_PYOBJECT(py_switch_info,cases,cases);
		PyList_Append(switches,py_switch_info);
		Py_DecRef(py_switch_info);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,switches,switches);

	PyObject* csmap = PyList_New(0);
	for (unsigned long i=0; i<self->entry->csmap_count; ++i) {
		struct csitem* csitem = &self->entry->csmap[i];
		PyObject* py_csitem = PyDict_New();
		FTDB_SET_ENTRY_INT64(py_csitem,pid,csitem->pid);
		FTDB_SET_ENTRY_ULONG(py_csitem,id,csitem->id);
		FTDB_SET_ENTRY_STRING(py_csitem,cf,csitem->cf);
		PyList_Append(csmap,py_csitem);
		Py_DecRef(py_csitem);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,csmap,csmap);

	PyObject* locals = PyList_New(0);
	for (unsigned long i=0; i<self->entry->locals_count; ++i) {
		struct local_info* local_info_entry = &self->entry->locals[i];
		PyObject* py_local_entry = PyDict_New();
		FTDB_SET_ENTRY_ULONG(py_local_entry,id,local_info_entry->id);
		FTDB_SET_ENTRY_STRING(py_local_entry,name,local_info_entry->name);
		FTDB_SET_ENTRY_ULONG(py_local_entry,type,local_info_entry->type);
		FTDB_SET_ENTRY_BOOL(py_local_entry,parm,local_info_entry->isparm);
		FTDB_SET_ENTRY_BOOL(py_local_entry,static,local_info_entry->isstatic);
		FTDB_SET_ENTRY_BOOL(py_local_entry,used,local_info_entry->isused);
		FTDB_SET_ENTRY_STRING(py_local_entry,location,local_info_entry->location);
		FTDB_SET_ENTRY_INT64_OPTIONAL(py_local_entry,csid,local_info_entry->csid);
		PyList_Append(locals,py_local_entry);
		Py_DecRef(py_local_entry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,locals,locals);

	PyObject* derefs = PyList_New(0);
	for (unsigned long i=0; i<self->entry->derefs_count; ++i) {
		struct deref_info* deref_info_entry = &self->entry->derefs[i];
		PyObject* py_deref_entry = PyDict_New();
		FTDB_SET_ENTRY_ENUM_TO_STRING(py_deref_entry,kind,deref_info_entry->kind,exprType);
		FTDB_SET_ENTRY_INT64_OPTIONAL(py_deref_entry,offset,deref_info_entry->offset);
		FTDB_SET_ENTRY_ULONG_OPTIONAL(py_deref_entry,basecnt,deref_info_entry->basecnt);
		PyObject* offsetrefs = PyList_New(0);
		for (unsigned long j=0; j<deref_info_entry->offsetrefs_count; ++j) {
			struct offsetref_info* offsetref_info = &deref_info_entry->offsetrefs[j];
			PyObject* py_offsetref_entry = PyDict_New();
			FTDB_SET_ENTRY_ENUM_TO_STRING(py_offsetref_entry,kind,offsetref_info->kind,exprType);
			if (offsetref_info->kind==EXPR_STRING) {
				FTDB_SET_ENTRY_STRING(py_offsetref_entry,id,offsetref_info->id_string);
			}
			else if (offsetref_info->kind==EXPR_FLOATING) {
				FTDB_SET_ENTRY_FLOAT(py_offsetref_entry,id,offsetref_info->id_float);
			}
			else {
				FTDB_SET_ENTRY_INT64(py_offsetref_entry,id,offsetref_info->id_integer);
			}
			FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,mi,offsetref_info->mi);
			FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,di,offsetref_info->di);
			FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,cast,offsetref_info->cast);
			PyList_Append(offsetrefs,py_offsetref_entry);
			Py_DecRef(py_offsetref_entry);
		}
		FTDB_SET_ENTRY_PYOBJECT(py_deref_entry,offsetrefs,offsetrefs);
		FTDB_SET_ENTRY_STRING(py_deref_entry,expr,deref_info_entry->expr);
		FTDB_SET_ENTRY_INT64(py_deref_entry,csid,deref_info_entry->csid);
		FTDB_SET_ENTRY_ULONG_ARRAY(py_deref_entry,ord,deref_info_entry->ord);
		FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry,member,deref_info_entry->member);
		FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry,type,deref_info_entry->type);
		FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry,access,deref_info_entry->access);
		FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry,shift,deref_info_entry->shift);
		FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry,mcall,deref_info_entry->mcall);
		PyList_Append(derefs,py_deref_entry);
		Py_DecRef(py_deref_entry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,derefs,derefs);

	PyObject* ifs = PyList_New(0);
	for (unsigned long i=0; i<self->entry->ifs_count; ++i) {
		struct if_info* if_entry = &self->entry->ifs[i];
		PyObject* py_if_entry = PyDict_New();
		FTDB_SET_ENTRY_INT64(py_if_entry,csid,if_entry->csid);
		PyObject* if_refs = PyList_New(0);
		for (unsigned long j=0; j<if_entry->refs_count; ++j) {
			struct ifref_info* ifref_info = &if_entry->refs[j];
			PyObject* py_ifref_info = PyDict_New();
			FTDB_SET_ENTRY_ENUM_TO_STRING(py_ifref_info,type,ifref_info->type,exprType);
			if (ifref_info->id) {
				FTDB_SET_ENTRY_ULONG_OPTIONAL(py_ifref_info,id,ifref_info->id);
			}
			else if (ifref_info->integer_literal) {
				FTDB_SET_ENTRY_INT64_OPTIONAL(py_ifref_info,id,ifref_info->integer_literal);
			}
			else if (ifref_info->character_literal) {
				FTDB_SET_ENTRY_INT_OPTIONAL(py_ifref_info,id,ifref_info->character_literal);
			}
			else if (ifref_info->floating_literal) {
				FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_ifref_info,id,ifref_info->floating_literal);
			}
			else if (ifref_info->string_literal) {
				FTDB_SET_ENTRY_STRING_OPTIONAL(py_ifref_info,id,ifref_info->string_literal);
			}
			PyList_Append(if_refs,py_ifref_info);
			Py_DecRef(py_ifref_info);
		}
		FTDB_SET_ENTRY_PYOBJECT(py_if_entry,refs,if_refs);
		PyList_Append(ifs,py_if_entry);
		Py_DecRef(py_if_entry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,ifs,ifs);

	PyObject* asms = PyList_New(0);
	for (unsigned long i=0; i<self->entry->asms_count; ++i) {
		struct asm_info* asm_info = &self->entry->asms[i];
		PyObject* py_asm_entry = PyDict_New();
		FTDB_SET_ENTRY_INT64(py_asm_entry,csid,asm_info->csid);
		FTDB_SET_ENTRY_STRING(py_asm_entry,str,asm_info->str);
		PyList_Append(asms,py_asm_entry);
		Py_DecRef(py_asm_entry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,asm,asms);

	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,globalrefs,self->entry->globalrefs);

	PyObject* globalrefInfo = PyList_New(0);
	for (unsigned long i=0; i<self->entry->globalrefs_count; ++i) {
		struct globalref_data* globalref_data = &self->entry->globalrefInfo[i];
		PyObject* refs = PyList_New(0);
		for (unsigned long j=0; j<globalref_data->refs_count; ++j) {
			struct globalref_info* globalref_info = &globalref_data->refs[j];
			PyObject* py_globalref_info = PyDict_New();
			FTDB_SET_ENTRY_STRING(py_globalref_info,start,globalref_info->start);
			FTDB_SET_ENTRY_STRING(py_globalref_info,end,globalref_info->end);
			PyList_Append(refs,py_globalref_info);
			Py_DecRef(py_globalref_info);
		}
		PyList_Append(globalrefInfo,refs);
		Py_DecRef(refs);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,globalrefInfo,globalrefInfo);

	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,funrefs,self->entry->funrefs);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,refs,self->entry->refs);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,decls,self->entry->decls);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,types,self->entry->types);

	return json_entry;
}

PyObject* libftdb_ftdb_func_entry_get_id(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyLong_FromUnsignedLong(__self->entry->id);
}

PyObject* libftdb_ftdb_func_entry_get_hash(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->hash);
}

PyObject* libftdb_ftdb_func_entry_get_name(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->name);
}

PyObject* libftdb_ftdb_func_entry_has_namespace(libftdb_ftdb_func_entry_object *self, PyObject *args) {

	if (self->entry->__namespace) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_entry_has_classOuterFn(libftdb_ftdb_func_entry_object *self, PyObject *args) {

	if (self->entry->classOuterFn) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;

}

PyObject* libftdb_ftdb_func_entry_has_class(libftdb_ftdb_func_entry_object *self, PyObject *args) {

	if ((self->entry->__class)||(self->entry->classid)) {
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

PyObject* libftdb_ftdb_func_entry_get_namespace(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->__namespace) {
		PyErr_SetString(libftdb_ftdbError, "No 'namespace' field in function entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->__namespace);
}

PyObject* libftdb_ftdb_func_entry_get_fid(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyLong_FromUnsignedLong(__self->entry->fid);
}

PyObject* libftdb_ftdb_func_entry_get_fids(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* fids = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->fids_count; ++i) {
		PYLIST_ADD_ULONG(fids,__self->entry->fids[i]);
	}
	return fids;
}

PyObject* libftdb_ftdb_func_entry_get_mids(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->mids) {
		Py_RETURN_NONE;
	}

	PyObject* mids = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->mids_count; ++i) {
		PYLIST_ADD_ULONG(mids,__self->entry->mids[i]);
	}
	return mids;
}

PyObject* libftdb_ftdb_func_entry_get_nargs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyLong_FromUnsignedLong(__self->entry->nargs);
}

PyObject* libftdb_ftdb_func_entry_get_variadic(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (__self->entry->isvariadic) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_entry_get_firstNonDeclStmt(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->firstNonDeclStmt);
}

PyObject* libftdb_ftdb_func_entry_get_inline(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if ((__self->entry->isinline) && (*__self->entry->isinline)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_entry_get_template(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if ((__self->entry->istemplate) && (*__self->entry->istemplate)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_entry_get_classOuterFn(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->classOuterFn) {
		PyErr_SetString(libftdb_ftdbError, "No 'classOuterFn' field in function entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->classOuterFn);
}

PyObject* libftdb_ftdb_func_entry_get_linkage(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyLong_FromLong((long)__self->entry->linkage);
}


PyObject* libftdb_ftdb_func_entry_get_linkage_string(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(set_functionLinkage(__self->entry->linkage));
}

PyObject* libftdb_ftdb_func_entry_get_member(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if ((__self->entry->ismember) && (*__self->entry->ismember)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_entry_get_class(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->__class) {
		PyErr_SetString(libftdb_ftdbError, "No 'class' field in function entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->__class);
}

PyObject* libftdb_ftdb_func_entry_get_classid(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->classid) {
		PyErr_SetString(libftdb_ftdbError, "No 'classid' field in function entry");
		return 0;
	}

	return PyLong_FromUnsignedLong(*__self->entry->classid);
}

PyObject* libftdb_ftdb_func_entry_get_attributes(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* attributes = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->attributes_count; ++i) {
		PYLIST_ADD_STRING(attributes,__self->entry->attributes[i]);
	}
	return attributes;
}

PyObject* libftdb_ftdb_func_entry_get_cshash(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->cshash);
}

PyObject* libftdb_ftdb_func_entry_get_template_parameters(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->template_parameters) {
		PyErr_SetString(libftdb_ftdbError, "No 'template_parameters' field in function entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->template_parameters);
}

PyObject* libftdb_ftdb_func_entry_get_body(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->body);
}

PyObject* libftdb_ftdb_func_entry_get_unpreprocessed_body(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->unpreprocessed_body);
}

PyObject* libftdb_ftdb_func_entry_get_declbody(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->declbody);
}

PyObject* libftdb_ftdb_func_entry_get_signature(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->signature);
}

PyObject* libftdb_ftdb_func_entry_get_declhash(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->declhash);
}

PyObject* libftdb_ftdb_func_entry_get_location(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->location);
}

PyObject* libftdb_ftdb_func_entry_get_abs_location(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	if (!__self->entry->abs_location) {
		PyErr_SetString(libftdb_ftdbError, "No 'abs_location' field in function entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->abs_location);
}

PyObject* libftdb_ftdb_func_entry_get_start_loc(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->start_loc);
}

PyObject* libftdb_ftdb_func_entry_get_end_loc(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyUnicode_FromString(__self->entry->end_loc);
}

PyObject* libftdb_ftdb_func_entry_get_refcount(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyLong_FromUnsignedLong(__self->entry->refcount);
}

PyObject* libftdb_ftdb_func_entry_get_declcount(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	return PyLong_FromUnsignedLong(__self->entry->declcount);
}

PyObject* libftdb_ftdb_func_entry_get_literals(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* literals = PyDict_New();
	FTDB_SET_ENTRY_INT64_ARRAY(literals,integer,__self->entry->integer_literals);
	FTDB_SET_ENTRY_FLOAT_ARRAY(literals,floating,__self->entry->floating_literals);
	FTDB_SET_ENTRY_UINT_ARRAY(literals,character,__self->entry->character_literals);
	FTDB_SET_ENTRY_STRING_ARRAY(literals,string,__self->entry->string_literals);

	return literals;
}

PyObject* libftdb_ftdb_func_entry_get_integer_literals(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* integer_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->integer_literals_count; ++i) {
		PYLIST_ADD_LONG(integer_literals,__self->entry->integer_literals[i]);
	}
	return integer_literals;
}

PyObject* libftdb_ftdb_func_entry_get_character_literals(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* character_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->character_literals_count; ++i) {
		PYLIST_ADD_ULONG(character_literals,__self->entry->character_literals[i]);
	}
	return character_literals;
}

PyObject* libftdb_ftdb_func_entry_get_floating_literals(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* floating_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->floating_literals_count; ++i) {
		PYLIST_ADD_FLOAT(floating_literals,__self->entry->floating_literals[i]);
	}
	return floating_literals;
}

PyObject* libftdb_ftdb_func_entry_get_string_literals(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* string_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->string_literals_count; ++i) {
		PYLIST_ADD_STRING(string_literals,__self->entry->string_literals[i]);
	}
	return string_literals;
}

PyObject* libftdb_ftdb_func_entry_get_taint(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* taint = PyDict_New();

	for (unsigned long i=0; i<__self->entry->taint_count; ++i) {
		struct taint_data* taint_data = &__self->entry->taint[i];
		PyObject* py_taint_list = PyList_New(0);
		for (unsigned long j=0; j<taint_data->taint_list_count; ++j) {
			struct taint_element* taint_element = &taint_data->taint_list[j];
			PyObject* py_taint_element = PyList_New(0);
			PYLIST_ADD_ULONG(py_taint_element,taint_element->taint_level);
			PYLIST_ADD_ULONG(py_taint_element,taint_element->varid);
			PyList_Append(py_taint_list,py_taint_element);
			Py_DecRef(py_taint_element);
		}
		PyObject* key = PyUnicode_FromFormat("%lu",i);
		PyDict_SetItem(taint, key, py_taint_list);
		Py_DecRef(key);
		Py_DecRef(py_taint_list);
	}
	return taint;
}

PyObject* libftdb_ftdb_func_entry_get_calls(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* calls = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->calls_count; ++i) {
		PYLIST_ADD_ULONG(calls,__self->entry->calls[i]);
	}
	return calls;
}

PyObject* libftdb_ftdb_func_entry_get_call_info(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* call_info = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->calls_count; ++i) {
		PyObject* args = PyTuple_New(4);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->index);
		PYTUPLE_SET_ULONG(args,2,i);
		PYTUPLE_SET_ULONG(args,3,0);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncCallInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(call_info,entry);
		Py_DecRef(entry);
	}
	return call_info;
}

PyObject* libftdb_ftdb_func_entry_get_callrefs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* callrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->calls_count; ++i) {
		struct callref_info* callref_info = &__self->entry->callrefs[i];
		PyObject* py_callref_info = PyList_New(0);
		for (unsigned long j=0; j<callref_info->callarg_count; ++j) {
			struct callref_data* callref_data = &callref_info->callarg[j];
			PyObject* py_callref_data = PyDict_New();
			FTDB_SET_ENTRY_ENUM_TO_STRING(py_callref_data,type,callref_data->type,exprType);
			FTDB_SET_ENTRY_ULONG(py_callref_data,pos,callref_data->pos);
			if (callref_data->id) {
				FTDB_SET_ENTRY_ULONG_OPTIONAL(py_callref_data,id,callref_data->id);
			}
			else if (callref_data->integer_literal) {
				FTDB_SET_ENTRY_INT64_OPTIONAL(py_callref_data,id,callref_data->integer_literal);
			}
			else if (callref_data->character_literal) {
				FTDB_SET_ENTRY_UINT_OPTIONAL(py_callref_data,id,callref_data->character_literal);
			}
			else if (callref_data->floating_literal) {
				FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_callref_data,id,callref_data->floating_literal);
			}
			else if (callref_data->string_literal) {
				FTDB_SET_ENTRY_STRING_OPTIONAL(py_callref_data,id,callref_data->string_literal);
			}
			PyList_Append(py_callref_info,py_callref_data);
			Py_DecRef(py_callref_data);
		}
		PyList_Append(callrefs,py_callref_info);
		Py_DecRef(py_callref_info);
	}

	return callrefs;
}

PyObject* libftdb_ftdb_func_entry_get_refcalls(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* refcalls = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refcalls_count; ++i) {
		struct refcall* refcall = &__self->entry->refcalls[i];
		if (refcall->ismembercall) {
			PyObject* refcall_info = PyList_New(0);
			PYLIST_ADD_ULONG(refcall_info,refcall->fid);
			PYLIST_ADD_ULONG(refcall_info,refcall->cid);
			PYLIST_ADD_ULONG(refcall_info,refcall->field_index);
			PyList_Append(refcalls,refcall_info);
			Py_DecRef(refcall_info);
		}
		else {
			PyObject* refcall_info = PyList_New(0);
			PYLIST_ADD_ULONG(refcall_info,refcall->fid);
			PyList_Append(refcalls,refcall_info);
			Py_DecRef(refcall_info);
		}
	}
	return refcalls;
}

PyObject* libftdb_ftdb_func_entry_get_refcall_info(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* call_info = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refcalls_count; ++i) {
		PyObject* args = PyTuple_New(4);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->index);
		PYTUPLE_SET_ULONG(args,2,i);
		PYTUPLE_SET_ULONG(args,3,1);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncCallInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(call_info,entry);
		Py_DecRef(entry);
	}
	return call_info;
}

PyObject* libftdb_ftdb_func_entry_get_refcallrefs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* refcallrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refcalls_count; ++i) {
		struct callref_info* refcallref_info = &__self->entry->refcallrefs[i];
		PyObject* py_refcallref_info = PyList_New(0);
		for (unsigned long j=0; j<refcallref_info->callarg_count; ++j) {
			struct callref_data* refcallref_data = &refcallref_info->callarg[j];
			PyObject* py_refcallref_data = PyDict_New();
			FTDB_SET_ENTRY_ENUM_TO_STRING(py_refcallref_data,type,refcallref_data->type,exprType);
			FTDB_SET_ENTRY_ULONG(py_refcallref_data,pos,refcallref_data->pos);
			if (refcallref_data->id) {
				FTDB_SET_ENTRY_ULONG_OPTIONAL(py_refcallref_data,id,refcallref_data->id);
			}
			else if (refcallref_data->integer_literal) {
				FTDB_SET_ENTRY_INT64_OPTIONAL(py_refcallref_data,id,refcallref_data->integer_literal);
			}
			else if (refcallref_data->character_literal) {
				FTDB_SET_ENTRY_UINT_OPTIONAL(py_refcallref_data,id,refcallref_data->character_literal);
			}
			else if (refcallref_data->floating_literal) {
				FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_refcallref_data,id,refcallref_data->floating_literal);
			}
			else if (refcallref_data->string_literal) {
				FTDB_SET_ENTRY_STRING_OPTIONAL(py_refcallref_data,id,refcallref_data->string_literal);
			}
			PyList_Append(py_refcallref_info,py_refcallref_data);
			Py_DecRef(py_refcallref_data);
		}
		PyList_Append(refcallrefs,py_refcallref_info);
		Py_DecRef(py_refcallref_info);
	}

	return refcallrefs;
}

PyObject* libftdb_ftdb_func_entry_get_switches(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* switch_info = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->switches_count; ++i) {
		PyObject* args = PyTuple_New(3);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->index);
		PYTUPLE_SET_ULONG(args,2,i);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncSwitchInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(switch_info,entry);
		Py_DecRef(entry);
	}
	return switch_info;
}

PyObject* libftdb_ftdb_func_entry_get_csmap(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* csmap = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->csmap_count; ++i) {
		struct csitem* csitem = &__self->entry->csmap[i];
		PyObject* py_csitem = PyDict_New();
		FTDB_SET_ENTRY_INT64(py_csitem,pid,csitem->pid);
		FTDB_SET_ENTRY_ULONG(py_csitem,id,csitem->id);
		FTDB_SET_ENTRY_STRING(py_csitem,cf,csitem->cf);
		PyList_Append(csmap,py_csitem);
		Py_DecRef(py_csitem);
	}
	return csmap;
}

PyObject* libftdb_ftdb_func_entry_get_locals(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* local_info = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->locals_count; ++i) {
		PyObject* args = PyTuple_New(3);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->index);
		PYTUPLE_SET_ULONG(args,2,i);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncLocalInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(local_info,entry);
		Py_DecRef(entry);
	}
	return local_info;
}

PyObject* libftdb_ftdb_func_entry_get_derefs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* derefs_info = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->derefs_count; ++i) {
		PyObject* args = PyTuple_New(3);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->index);
		PYTUPLE_SET_ULONG(args,2,i);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncDerefInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(derefs_info,entry);
		Py_DecRef(entry);
	}
	return derefs_info;
}

PyObject* libftdb_ftdb_func_entry_get_ifs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* ifs_info = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->ifs_count; ++i) {
		PyObject* args = PyTuple_New(3);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->index);
		PYTUPLE_SET_ULONG(args,2,i);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncIfInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(ifs_info,entry);
		Py_DecRef(entry);
	}
	return ifs_info;
}

PyObject* libftdb_ftdb_func_entry_get_asms(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* asms = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->asms_count; ++i) {
		struct asm_info* asm_item = &__self->entry->asms[i];
		PyObject* py_asm_item = PyDict_New();
		FTDB_SET_ENTRY_INT64(py_asm_item,csid,asm_item->csid);
		FTDB_SET_ENTRY_STRING(py_asm_item,str,asm_item->str);
		PyList_Append(asms,py_asm_item);
		Py_DecRef(py_asm_item);
	}
	return asms;
}

PyObject* libftdb_ftdb_func_entry_get_globalrefs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* globalrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->globalrefs_count; ++i) {
		PYLIST_ADD_ULONG(globalrefs,__self->entry->globalrefs[i]);
	}
	return globalrefs;
}

PyObject* libftdb_ftdb_func_entry_get_globalrefInfo(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* globalrefInfo = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->globalrefs_count; ++i) {
		struct globalref_data* globalref_data = &__self->entry->globalrefInfo[i];
		PyObject* py_globalref_data = PyList_New(0);
		for (unsigned long j=0; j<globalref_data->refs_count; ++j) {
			struct globalref_info* globalref_info = &globalref_data->refs[j];
			PyObject* py_globalref_info = PyDict_New();
			FTDB_SET_ENTRY_STRING(py_globalref_info,start,globalref_info->start);
			FTDB_SET_ENTRY_STRING(py_globalref_info,end,globalref_info->end);
			PyList_Append(py_globalref_data,py_globalref_info);
			Py_DecRef(py_globalref_info);
		}
		PyList_Append(globalrefInfo,py_globalref_data);
		Py_DecRef(py_globalref_data);
	}
	return globalrefInfo;
}

PyObject* libftdb_ftdb_func_entry_get_funrefs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* funrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->funrefs_count; ++i) {
		PYLIST_ADD_ULONG(funrefs,__self->entry->funrefs[i]);
	}
	return funrefs;
}

PyObject* libftdb_ftdb_func_entry_get_refs(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* refs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refs_count; ++i) {
		PYLIST_ADD_ULONG(refs,__self->entry->refs[i]);
	}
	return refs;
}

PyObject* libftdb_ftdb_func_entry_get_decls(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* decls = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->decls_count; ++i) {
		PYLIST_ADD_ULONG(decls,__self->entry->decls[i]);
	}
	return decls;
}

PyObject* libftdb_ftdb_func_entry_get_types(PyObject* self, void* closure) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;

	PyObject* types = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->types_count; ++i) {
		PYLIST_ADD_ULONG(types,__self->entry->types[i]);
	}
	return types;
}

PyObject* libftdb_ftdb_func_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_id(self,0);
	}
	else if (!strcmp(attr,"hash")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_hash(self,0);
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_name(self,0);
	}
	else if (!strcmp(attr,"namespace")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_namespace(self,0);
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_fid(self,0);
	}
	else if (!strcmp(attr,"fids")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_fids(self,0);
	}
	else if (!strcmp(attr,"mids")) {
		PYASSTR_DECREF(attr);
		return libftdb_ftdb_func_entry_get_mids(self,0);
	}
	else if (!strcmp(attr,"nargs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_nargs(self,0);
	}
	else if (!strcmp(attr,"variadic")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_variadic(self,0);
	}
	else if (!strcmp(attr,"firstNonDeclStmt")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_firstNonDeclStmt(self,0);
	}
	else if (!strcmp(attr,"inline")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_inline(self,0);
	}
	else if (!strcmp(attr,"template")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_template(self,0);
	}
	else if (!strcmp(attr,"classOuterFn")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_classOuterFn(self,0);
	}
	else if (!strcmp(attr,"linkage")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_linkage_string(self,0);
	}
	else if (!strcmp(attr,"member")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_member(self,0);
	}
	else if (!strcmp(attr,"class")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_class(self,0);
	}
	else if (!strcmp(attr,"classid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_classid(self,0);
	}
	else if (!strcmp(attr,"attributes")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_attributes(self,0);
	}
	else if (!strcmp(attr,"cshash")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_cshash(self,0);
	}
	else if (!strcmp(attr,"template_parameters")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_template_parameters(self,0);
	}
	else if (!strcmp(attr,"body")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_body(self,0);
	}
	else if (!strcmp(attr,"unpreprocessed_body")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_unpreprocessed_body(self,0);
	}
	else if (!strcmp(attr,"declbody")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_declbody(self,0);
	}
	else if (!strcmp(attr,"signature")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_signature(self,0);
	}
	else if (!strcmp(attr,"declhash")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_declhash(self,0);
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_location(self,0);
	}
	else if (!strcmp(attr,"abs_location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_abs_location(self,0);
	}
	else if (!strcmp(attr,"start_loc")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_start_loc(self,0);
	}
	else if (!strcmp(attr,"end_loc")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_end_loc(self,0);
	}
	else if (!strcmp(attr,"refcount")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_refcount(self,0);
	}
	else if (!strcmp(attr,"declcount")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_declcount(self,0);
	}
	else if (!strcmp(attr,"literals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_literals(self,0);
	}
	else if (!strcmp(attr,"integer_literals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_integer_literals(self,0);
	}
	else if (!strcmp(attr,"character_literals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_character_literals(self,0);
	}
	else if (!strcmp(attr,"floating_literals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_floating_literals(self,0);
	}
	else if (!strcmp(attr,"string_literals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_string_literals(self,0);
	}
	else if (!strcmp(attr,"taint")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_taint(self,0);
	}
	else if (!strcmp(attr,"calls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_calls(self,0);
	}
	else if (!strcmp(attr,"call_info")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_call_info(self,0);
	}
	else if (!strcmp(attr,"callrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_callrefs(self,0);
	}
	else if (!strcmp(attr,"refcalls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_refcalls(self,0);
	}
	else if (!strcmp(attr,"refcall_info")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_refcall_info(self,0);
	}
	else if (!strcmp(attr,"refcallrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_refcallrefs(self,0);
	}
	else if (!strcmp(attr,"switches")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_switches(self,0);
	}
	else if (!strcmp(attr,"csmap")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_csmap(self,0);
	}
	else if (!strcmp(attr,"locals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_locals(self,0);
	}
	else if (!strcmp(attr,"derefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_derefs(self,0);
	}
	else if (!strcmp(attr,"ifs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_ifs(self,0);
	}
	else if (!strcmp(attr,"asm")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_asms(self,0);
	}
	else if (!strcmp(attr,"globalrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_globalrefs(self,0);
	}
	else if (!strcmp(attr,"globalrefInfo")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_globalrefInfo(self,0);
	}
	else if (!strcmp(attr,"funrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_funrefs(self,0);
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_refs(self,0);
	}
	else if (!strcmp(attr,"decls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_decls(self,0);
	}
	else if (!strcmp(attr,"types")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_entry_get_types(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_entry_sq_contains(PyObject* self, PyObject* slice) {

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;
	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"hash")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"namespace")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->__namespace;
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"fids")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"mids")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"nargs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"variadic")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"firstNonDeclStmt")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"inline")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->isinline;
	}
	else if (!strcmp(attr,"template")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->istemplate;
	}
	else if (!strcmp(attr,"classOuterFn")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->classOuterFn;
	}
	else if (!strcmp(attr,"linkage")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"member")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->ismember;
	}
	else if (!strcmp(attr,"class")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->__class;
	}
	else if (!strcmp(attr,"classid")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->classid;
	}
	else if (!strcmp(attr,"attributes")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"cshash")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"template_parameters")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->template_parameters;
	}
	else if (!strcmp(attr,"body")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"unpreprocessed_body")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"declbody")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"signature")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"declhash")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"abs_location")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->abs_location;
	}
	else if (!strcmp(attr,"start_loc")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"end_loc")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refcount")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"declcount")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"literals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"integer_literals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"character_literals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"floating_literals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"string_literals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"taint")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"calls")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"call_info")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"callrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refcalls")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refcall_info")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refcallrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"switches")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"csmap")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"locals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"derefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"ifs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"asm")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"globalrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"globalrefInfo")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"funrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"decls")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"types")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

PyObject* libftdb_ftdb_func_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

Py_hash_t libftdb_ftdb_func_entry_hash(PyObject *o) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)o;
	return __self->index;
}

PyObject* libftdb_ftdb_func_entry_richcompare(PyObject *self, PyObject *other, int op) {

	libftdb_ftdb_func_entry_object* __self = (libftdb_ftdb_func_entry_object*)self;
	libftdb_ftdb_func_entry_object* __other = (libftdb_ftdb_func_entry_object*)other;
	Py_RETURN_RICHCOMPARE(__self->index, __other->index , op);
}

void libftdb_ftdb_func_callinfo_entry_dealloc(libftdb_ftdb_func_callinfo_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_callinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_func_callinfo_entry_object* self;

	self = (libftdb_ftdb_func_callinfo_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args,1));
        if (func_index>=self->ftdb->funcs_count) {
        	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"func entry index out of bounds: %lu\n",func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args,2));
        self->is_refcall = PyLong_AsUnsignedLong(PyTuple_GetItem(args,3));

        if (self->is_refcall==0) {
        	if (entry_index>=self->ftdb->funcs[func_index].calls_count) {
				snprintf(errmsg,ERRMSG_BUFFER_SIZE,"callinfo entry index out of bounds: %lu@%lu\n",func_index,entry_index);
				PyErr_SetString(libftdb_ftdbError,errmsg);
				return 0;
			}
        	self->entry_index = entry_index;
        	self->entry = &self->ftdb->funcs[self->func_index].call_info[self->entry_index];
        }
        else {
        	if (entry_index>=self->ftdb->funcs[func_index].refcalls_count) {
				snprintf(errmsg,ERRMSG_BUFFER_SIZE,"refcallinfo entry index out of bounds: %lu@%lu\n",func_index,entry_index);
				PyErr_SetString(libftdb_ftdbError,errmsg);
				return 0;
			}
        	self->entry_index = entry_index;
        	self->entry = &self->ftdb->funcs[self->func_index].refcall_info[self->entry_index];
        }
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_callinfo_entry_repr(PyObject* self) {

    static char repr[1024];

    libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;
    int written = snprintf(repr,1024,"<ftdbFuncCallInfoEntry object at %lx : ",(uintptr_t)self);
    written+=snprintf(repr+written,1024-written,"fdx:%lu|edx:%lu>",__self->func_index,__self->entry_index);

    return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_func_callinfo_entry_get_start(PyObject* self, void* closure) {

	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;
	return PyUnicode_FromString(__self->entry->start);
}

PyObject* libftdb_ftdb_func_callinfo_entry_get_end(PyObject* self, void* closure) {

	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;
	return PyUnicode_FromString(__self->entry->end);
}

PyObject* libftdb_ftdb_func_callinfo_entry_get_ord(PyObject* self, void* closure) {

	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->ord);
}

PyObject* libftdb_ftdb_func_callinfo_entry_get_expr(PyObject* self, void* closure) {

	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;
	return PyUnicode_FromString(__self->entry->expr);
}

PyObject* libftdb_ftdb_func_callinfo_entry_get_loc(PyObject* self, void* closure) {

	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;
	if (__self->entry->loc) {
		return PyUnicode_FromString(__self->entry->loc);
	}
	else {
		Py_RETURN_NONE;
	}
}

PyObject* libftdb_ftdb_func_callinfo_entry_get_args(PyObject* self, void* closure) {

	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;

	PyObject* args = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->args_count; ++i) {
		PYLIST_ADD_ULONG(args,__self->entry->args[i]);
	}
	return args;
}

PyObject* libftdb_ftdb_func_callinfo_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"start")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_callinfo_entry_get_start(self,0);
	}
	else if (!strcmp(attr,"end")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_callinfo_entry_get_end(self,0);
	}
	else if (!strcmp(attr,"ord")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_callinfo_entry_get_ord(self,0);
	}
	else if (!strcmp(attr,"expr")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_callinfo_entry_get_expr(self,0);
	}
	else if (!strcmp(attr,"loc")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_callinfo_entry_get_loc(self,0);
	}
	else if (!strcmp(attr,"args")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_callinfo_entry_get_args(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_callinfo_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);
	libftdb_ftdb_func_callinfo_entry_object* __self = (libftdb_ftdb_func_callinfo_entry_object*)self;

	if (!strcmp(attr,"start")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"end")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"ord")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"expr")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"loc")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->loc;
	}
	else if (!strcmp(attr,"args")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_func_switchinfo_entry_dealloc(libftdb_ftdb_func_switchinfo_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_switchinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_switchinfo_entry_object* self;

    self = (libftdb_ftdb_func_switchinfo_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args,1));
        if (func_index>=self->ftdb->funcs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"func entry index out of bounds: %lu\n",func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args,2));
		if (entry_index>=self->ftdb->funcs[func_index].switches_count) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"switchinfo entry index out of bounds: %lu@%lu\n",func_index,entry_index);
			PyErr_SetString(libftdb_ftdbError,errmsg);
			return 0;
		}
        self->entry_index = entry_index;
        self->entry = &self->ftdb->funcs[self->func_index].switches[self->entry_index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_switchinfo_entry_repr(PyObject* self) {

    static char repr[1024];

    libftdb_ftdb_func_switchinfo_entry_object* __self = (libftdb_ftdb_func_switchinfo_entry_object*)self;
    int written = snprintf(repr,1024,"<ftdbFuncSwitchInfoEntry object at %lx : ",(uintptr_t)self);
    written+=snprintf(repr+written,1024-written,"fdx:%lu|edx:%lu>",__self->func_index,__self->entry_index);

    return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_func_switchinfo_entry_get_condition(PyObject* self, void* closure) {

	libftdb_ftdb_func_switchinfo_entry_object* __self = (libftdb_ftdb_func_switchinfo_entry_object*)self;

	return PyUnicode_FromString(__self->entry->condition);
}

PyObject* libftdb_ftdb_func_switchinfo_entry_get_cases(PyObject* self, void* closure) {

	libftdb_ftdb_func_switchinfo_entry_object* __self = (libftdb_ftdb_func_switchinfo_entry_object*)self;

	PyObject* cases = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->cases_count; ++i) {
		struct case_info* case_info = &__self->entry->cases[i];
		if (!case_info->rhs) {
			PyObject* py_case_info = PyList_New(0);
			PYLIST_ADD_LONG(py_case_info,case_info->lhs.expr_value);
			PYLIST_ADD_STRING(py_case_info,case_info->lhs.enum_code);
			PYLIST_ADD_STRING(py_case_info,case_info->lhs.macro_code);
			PYLIST_ADD_STRING(py_case_info,case_info->lhs.raw_code);
			PyList_Append(cases,py_case_info);
			Py_DecRef(py_case_info);
		}
		else {
			PyObject* py_case_info = PyList_New(0);
			PYLIST_ADD_LONG(py_case_info,case_info->lhs.expr_value);
			PYLIST_ADD_STRING(py_case_info,case_info->lhs.enum_code);
			PYLIST_ADD_STRING(py_case_info,case_info->lhs.macro_code);
			PYLIST_ADD_STRING(py_case_info,case_info->lhs.raw_code);
			PYLIST_ADD_LONG(py_case_info,case_info->rhs->expr_value);
			PYLIST_ADD_STRING(py_case_info,case_info->rhs->enum_code);
			PYLIST_ADD_STRING(py_case_info,case_info->rhs->macro_code);
			PYLIST_ADD_STRING(py_case_info,case_info->rhs->raw_code);
			PyList_Append(cases,py_case_info);
			Py_DecRef(py_case_info);
		}
	}
	return cases;
}

PyObject* libftdb_ftdb_func_switchinfo_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"condition")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_switchinfo_entry_get_condition(self,0);
	}
	else if (!strcmp(attr,"cases")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_switchinfo_entry_get_cases(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_switchinfo_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"condition")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"cases")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_func_ifinfo_entry_dealloc(libftdb_ftdb_func_ifinfo_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_ifinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_ifinfo_entry_object* self;

    self = (libftdb_ftdb_func_ifinfo_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args,1));
        if (func_index>=self->ftdb->funcs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"func entry index out of bounds: %lu\n",func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args,2));
		if (entry_index>=self->ftdb->funcs[func_index].ifs_count) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"ifinfo entry index out of bounds: %lu@%lu\n",func_index,entry_index);
			PyErr_SetString(libftdb_ftdbError,errmsg);
			return 0;
		}
        self->entry_index = entry_index;
        self->entry = &self->ftdb->funcs[self->func_index].ifs[self->entry_index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_ifinfo_entry_repr(PyObject* self) {

    static char repr[1024];

    libftdb_ftdb_func_ifinfo_entry_object* __self = (libftdb_ftdb_func_ifinfo_entry_object*)self;
    int written = snprintf(repr,1024,"<ftdbFuncIfInfoEntry object at %lx : ",(uintptr_t)self);
    written+=snprintf(repr+written,1024-written,"fdx:%lu|edx:%lu>",__self->func_index,__self->entry_index);

    return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_func_ifinfo_entry_get_csid(PyObject* self, void* closure) {

	libftdb_ftdb_func_ifinfo_entry_object* __self = (libftdb_ftdb_func_ifinfo_entry_object*)self;
	return PyLong_FromLong(__self->entry->csid);
}

PyObject* libftdb_ftdb_func_ifinfo_entry_get_refs(PyObject* self, void* closure) {

	libftdb_ftdb_func_ifinfo_entry_object* __self = (libftdb_ftdb_func_ifinfo_entry_object*)self;

	PyObject* refs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refs_count; ++i) {
		struct ifref_info* ifref_info = &__self->entry->refs[i];
		PyObject* py_ifref_info = PyDict_New();
		FTDB_SET_ENTRY_ENUM_TO_STRING(py_ifref_info,type,ifref_info->type,exprType);
		if (ifref_info->id) {
			FTDB_SET_ENTRY_ULONG_OPTIONAL(py_ifref_info,id,ifref_info->id);
		}
		else if (ifref_info->integer_literal) {
			FTDB_SET_ENTRY_INT64_OPTIONAL(py_ifref_info,id,ifref_info->integer_literal);
		}
		else if (ifref_info->character_literal) {
			FTDB_SET_ENTRY_UINT_OPTIONAL(py_ifref_info,id,ifref_info->character_literal);
		}
		else if (ifref_info->floating_literal) {
			FTDB_SET_ENTRY_FLOAT_OPTIONAL(py_ifref_info,id,ifref_info->floating_literal);
		}
		else if (ifref_info->string_literal) {
			FTDB_SET_ENTRY_STRING_OPTIONAL(py_ifref_info,id,ifref_info->string_literal);
		}
		PyList_Append(refs,py_ifref_info);
		Py_DecRef(py_ifref_info);
	}
	return refs;
}

PyObject* libftdb_ftdb_func_ifinfo_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"csid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_ifinfo_entry_get_csid(self,0);
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_ifinfo_entry_get_refs(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_ifinfo_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"csid")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_func_localinfo_entry_dealloc(libftdb_ftdb_func_localinfo_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_localinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_localinfo_entry_object* self;

    self = (libftdb_ftdb_func_localinfo_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args,1));
        if (func_index>=self->ftdb->funcs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"func entry index out of bounds: %lu\n",func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args,2));
        if (entry_index>=self->ftdb->funcs[func_index].locals_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"localinfo entry index out of bounds: %lu@%lu\n",func_index,entry_index);
            PyErr_SetString(libftdb_ftdbError,errmsg);
            return 0;
        }
        self->entry_index = entry_index;
        self->entry = &self->ftdb->funcs[self->func_index].locals[self->entry_index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_localinfo_entry_repr(PyObject* self) {

    static char repr[1024];

    libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
    int written = snprintf(repr,1024,"<ftdbFuncLocalInfoEntry object at %lx : ",(uintptr_t)self);
    written+=snprintf(repr+written,1024-written,"fdx:%lu|edx:%lu>",__self->func_index,__self->entry_index);

    return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_func_localinfo_entry_get_id(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->id);
}

PyObject* libftdb_ftdb_func_localinfo_entry_get_name(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
	return PyUnicode_FromString(__self->entry->name);
}

PyObject* libftdb_ftdb_func_localinfo_entry_is_parm(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;

	if (__self->entry->isparm) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_localinfo_entry_get_type(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->type);
}

PyObject* libftdb_ftdb_func_localinfo_entry_is_static(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;

	if (__self->entry->isstatic) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_localinfo_entry_is_used(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;

	if (__self->entry->isused) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_localinfo_entry_get_csid(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
	if (!__self->entry->csid) {
		PyErr_SetString(libftdb_ftdbError, "No 'csid' field in localinfo entry");
		return 0;
	}
	return PyLong_FromUnsignedLong(*__self->entry->csid);
}

PyObject* libftdb_ftdb_func_localinfo_entry_get_location(PyObject* self, void* closure) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
	return PyUnicode_FromString(__self->entry->location);
}

PyObject* libftdb_ftdb_func_localinfo_entry_has_csid(libftdb_ftdb_func_entry_object *self, PyObject *args) {

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;

	if (__self->entry->csid) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_localinfo_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_get_id(self,0);
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_get_name(self,0);
	}
	else if (!strcmp(attr,"parm")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_is_parm(self,0);
	}
	else if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_get_type(self,0);
	}
	else if (!strcmp(attr,"static")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_is_static(self,0);
	}
	else if (!strcmp(attr,"used")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_is_used(self,0);
	}
	else if (!strcmp(attr,"csid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_get_csid(self,0);
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_localinfo_entry_get_location(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_localinfo_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_func_localinfo_entry_object* __self = (libftdb_ftdb_func_localinfo_entry_object*)self;
	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"parm")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"static")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"used")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"csid")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->csid;
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_func_derefinfo_entry_dealloc(libftdb_ftdb_func_derefinfo_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_derefinfo_entry_object* self;

    self = (libftdb_ftdb_func_derefinfo_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args,1));
        if (func_index>=self->ftdb->funcs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"func entry index out of bounds: %lu\n",func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args,2));
        if (entry_index>=self->ftdb->funcs[func_index].derefs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"derefinfo entry index out of bounds: %lu@%lu\n",func_index,entry_index);
            PyErr_SetString(libftdb_ftdbError,errmsg);
            return 0;
        }
        self->entry_index = entry_index;
        self->entry = &self->ftdb->funcs[self->func_index].derefs[self->entry_index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_repr(PyObject* self) {

    static char repr[1024];

    libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;
    int written = snprintf(repr,1024,"<ftdbFuncDerefInfoEntry object at %lx : ",(uintptr_t)self);
    written+=snprintf(repr+written,1024-written,"fdx:%lu|edx:%lu>",__self->func_index,__self->entry_index);

    return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_kind(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;
	return PyLong_FromLong((long)__self->entry->kind);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_kindname(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;
	return PyUnicode_FromString(set_exprType(__self->entry->kind));
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_offset(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->offset) {
		PyErr_SetString(libftdb_ftdbError, "No 'offset' field in derefinfo entry");
		return 0;
	}
	return PyLong_FromLong(*__self->entry->offset);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_basecnt(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->basecnt) {
		PyErr_SetString(libftdb_ftdbError, "No 'basecnt' field in derefinfo entry");
		return 0;
	}
	return PyLong_FromUnsignedLong(*__self->entry->basecnt);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_offsetrefs(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	PyObject* offsetrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->offsetrefs_count; ++i) {
		PyObject* args = PyTuple_New(4);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,__self->func_index);
		PYTUPLE_SET_ULONG(args,2,__self->entry_index);
		PYTUPLE_SET_ULONG(args,3,i);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncOffsetrefInfoEntryType, args);
		Py_DecRef(args);
		PyList_Append(offsetrefs,entry);
		Py_DecRef(entry);
	}
	return offsetrefs;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_expr(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;
	return PyUnicode_FromString(__self->entry->expr);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_csid(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;
	return PyLong_FromLong(__self->entry->csid);
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_ords(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	PyObject* ords = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->ord_count; ++i) {
		PYLIST_ADD_ULONG(ords,__self->entry->ord[i]);
	}
	return ords;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_member(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->member) {
		PyErr_SetString(libftdb_ftdbError, "No 'member' field in derefinfo entry");
		return 0;
	}

	PyObject* member = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->member_count; ++i) {
		PYLIST_ADD_LONG(member,__self->entry->member[i]);
	}
	return member;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_type(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->type) {
		PyErr_SetString(libftdb_ftdbError, "No 'type' field in derefinfo entry");
		return 0;
	}

	PyObject* type = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->type_count; ++i) {
		PYLIST_ADD_ULONG(type,__self->entry->type[i]);
	}
	return type;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_access(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->access) {
		PyErr_SetString(libftdb_ftdbError, "No 'access' field in derefinfo entry");
		return 0;
	}

	PyObject* access = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->access_count; ++i) {
		PYLIST_ADD_ULONG(access,__self->entry->access[i]);
	}
	return access;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_shift(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->shift) {
		PyErr_SetString(libftdb_ftdbError, "No 'shift' field in derefinfo entry");
		return 0;
	}

	PyObject* shift = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->shift_count; ++i) {
		PYLIST_ADD_LONG(shift,__self->entry->shift[i]);
	}
	return shift;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_get_mcall(PyObject* self, void* closure) {

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;

	if (!__self->entry->mcall) {
		PyErr_SetString(libftdb_ftdbError, "No 'mcall' field in derefinfo entry");
		return 0;
	}

	PyObject* mcall = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->mcall_count; ++i) {
		PYLIST_ADD_LONG(mcall,__self->entry->mcall[i]);
	}
	return mcall;
}

/* TODO: leaks */
PyObject* libftdb_ftdb_func_derefinfo_entry_json(libftdb_ftdb_func_derefinfo_entry_object *self, PyObject *args) {

	PyObject* py_deref_entry = PyDict_New();

	FTDB_SET_ENTRY_ENUM_TO_STRING(py_deref_entry,kind,self->entry->kind,exprType);
	FTDB_SET_ENTRY_INT64_OPTIONAL(py_deref_entry,offset,self->entry->offset);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(py_deref_entry,basecnt,self->entry->basecnt);
	PyObject* offsetrefs = PyList_New(0);
	for (unsigned long j=0; j<self->entry->offsetrefs_count; ++j) {
		struct offsetref_info* offsetref_info = &self->entry->offsetrefs[j];
		PyObject* py_offsetref_entry = PyDict_New();
		FTDB_SET_ENTRY_ENUM_TO_STRING(py_offsetref_entry,kind,offsetref_info->kind,exprType);
		if (offsetref_info->kind==EXPR_STRING) {
			FTDB_SET_ENTRY_STRING(py_offsetref_entry,id,offsetref_info->id_string);
		}
		else if (offsetref_info->kind==EXPR_FLOATING) {
			FTDB_SET_ENTRY_FLOAT(py_offsetref_entry,id,offsetref_info->id_float);
		}
		else {
			FTDB_SET_ENTRY_INT64(py_offsetref_entry,id,offsetref_info->id_integer);
		}
		FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,mi,offsetref_info->mi);
		FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,di,offsetref_info->di);
		FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,cast,offsetref_info->cast);
		PyList_Append(offsetrefs,py_offsetref_entry);
		Py_DecRef(py_offsetref_entry);
	}
	FTDB_SET_ENTRY_PYOBJECT(py_deref_entry,offsetrefs,offsetrefs);
	FTDB_SET_ENTRY_STRING(py_deref_entry,expr,self->entry->expr);
	FTDB_SET_ENTRY_INT64(py_deref_entry,csid,self->entry->csid);
	FTDB_SET_ENTRY_ULONG_ARRAY(py_deref_entry,ord,self->entry->ord);
	FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry,member,self->entry->member);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry,type,self->entry->type);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(py_deref_entry,access,self->entry->access);
	FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry,shift,self->entry->shift);
	FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(py_deref_entry,mcall,self->entry->mcall);

	return py_deref_entry;
}

PyObject* libftdb_ftdb_func_derefinfo_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"kind")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_kindname(self,0);
	}
	else if (!strcmp(attr,"offset")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_offset(self,0);
	}
	else if (!strcmp(attr,"basecnt")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_basecnt(self,0);
	}
	else if (!strcmp(attr,"offsetrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_offsetrefs(self,0);
	}
	else if (!strcmp(attr,"expr")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_expr(self,0);
	}
	else if (!strcmp(attr,"csid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_csid(self,0);
	}
	else if (!strcmp(attr,"ord")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_ords(self,0);
	}
	else if (!strcmp(attr,"member")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_member(self,0);
	}
	else if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_type(self,0);
	}
	else if (!strcmp(attr,"access")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_access(self,0);
	}
	else if (!strcmp(attr,"shift")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_shift(self,0);
	}
	else if (!strcmp(attr,"mcall")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_derefinfo_entry_get_mcall(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_derefinfo_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_func_derefinfo_entry_object* __self = (libftdb_ftdb_func_derefinfo_entry_object*)self;
	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"kind")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"offset")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->offset;
	}
	else if (!strcmp(attr,"basecnt")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->basecnt;
	}
	else if (!strcmp(attr,"offsetrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"expr")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"csid")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"ord")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"member")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->member;
	}
	else if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->type;
	}
	else if (!strcmp(attr,"access")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->access;
	}
	else if (!strcmp(attr,"shift")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->shift;
	}
	else if (!strcmp(attr,"mcall")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->mcall;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_func_offsetrefinfo_entry_dealloc(libftdb_ftdb_func_offsetrefinfo_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

    static char errmsg[ERRMSG_BUFFER_SIZE];
    libftdb_ftdb_func_offsetrefinfo_entry_object* self;

    self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
        self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
        unsigned long func_index = PyLong_AsLong(PyTuple_GetItem(args,1));
        if (func_index>=self->ftdb->funcs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"func entry index out of bounds: %lu\n",func_index);
            PyErr_SetString(libftdb_ftdbError, errmsg);
            return 0;
        }
        self->func_index = func_index;
        unsigned long deref_index = PyLong_AsLong(PyTuple_GetItem(args,2));
        if (deref_index>=self->ftdb->funcs[func_index].derefs_count) {
            snprintf(errmsg,ERRMSG_BUFFER_SIZE,"derefinfo entry index out of bounds: %lu@%lu\n",func_index,deref_index);
            PyErr_SetString(libftdb_ftdbError,errmsg);
            return 0;
        }
        self->deref_index = deref_index;
        unsigned long entry_index = PyLong_AsLong(PyTuple_GetItem(args,3));
        if (entry_index>=self->ftdb->funcs[func_index].derefs[deref_index].offsetrefs_count) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"offsetrefinfo entry index out of bounds: %lu@%lu@%lu\n",
					func_index,deref_index,entry_index);
			PyErr_SetString(libftdb_ftdbError,errmsg);
			return 0;
		}
        self->entry_index = entry_index;
        self->entry = &self->ftdb->funcs[self->func_index].derefs[self->deref_index].offsetrefs[self->entry_index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_repr(PyObject* self) {

    static char repr[1024];

    libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;
    int written = snprintf(repr,1024,"<ftdbFuncOffsetrefInfoEntry object at %lx : ",(uintptr_t)self);
    written+=snprintf(repr+written,1024-written,"fdx:%lu|ddx:%lu|edx:%lu>",
    		__self->func_index,__self->deref_index,__self->entry_index);

    return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_kind(PyObject* self, void* closure) {

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;
	return PyLong_FromLong((long)__self->entry->kind);
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_kindname(PyObject* self, void* closure) {

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;
	return PyUnicode_FromString(set_exprType(__self->entry->kind));
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_id(PyObject* self, void* closure) {

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;
	if (__self->entry->kind==EXPR_STRING) {
		return PyUnicode_FromString(__self->entry->id_string);
	}
	else if (__self->entry->kind==EXPR_FLOATING) {
		return PyFloat_FromDouble(__self->entry->id_float);
	}
	else {
		return PyLong_FromLong(__self->entry->id_integer);
	}
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_mi(PyObject* self, void* closure) {

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;

	if (!__self->entry->mi) {
		PyErr_SetString(libftdb_ftdbError, "No 'mi' field in offsetrefinfo entry");
		return 0;
	}
	return PyLong_FromUnsignedLong(*__self->entry->mi);
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_di(PyObject* self, void* closure) {

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;

	if (!__self->entry->di) {
		PyErr_SetString(libftdb_ftdbError, "No 'di' field in offsetrefinfo entry");
		return 0;
	}
	return PyLong_FromUnsignedLong(*__self->entry->di);
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_get_cast(PyObject* self, void* closure) {

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;

	if (!__self->entry->cast) {
		PyErr_SetString(libftdb_ftdbError, "No 'cast' field in offsetrefinfo entry");
		return 0;
	}
	return PyLong_FromUnsignedLong(*__self->entry->cast);
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_has_cast(libftdb_ftdb_func_offsetrefinfo_entry_object *self, PyObject *args) {

	if (self->entry->cast) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_json(libftdb_ftdb_func_offsetrefinfo_entry_object *self, PyObject *args) {

	PyObject* py_offsetref_entry = PyDict_New();

	struct offsetref_info* offsetref_info = self->entry;
	FTDB_SET_ENTRY_ENUM_TO_STRING(py_offsetref_entry,kind,offsetref_info->kind,exprType);
	if (offsetref_info->kind==EXPR_STRING) {
		FTDB_SET_ENTRY_STRING(py_offsetref_entry,id,offsetref_info->id_string);
	}
	else if (offsetref_info->kind==EXPR_FLOATING) {
		FTDB_SET_ENTRY_FLOAT(py_offsetref_entry,id,offsetref_info->id_float);
	}
	else {
		FTDB_SET_ENTRY_INT64(py_offsetref_entry,id,offsetref_info->id_integer);
	}
	FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,mi,offsetref_info->mi);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,di,offsetref_info->di);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(py_offsetref_entry,cast,offsetref_info->cast);

	return py_offsetref_entry;
}

PyObject* libftdb_ftdb_func_offsetrefinfo_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"kind")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_offsetrefinfo_entry_get_kindname(self,0);
	}
	else if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_offsetrefinfo_entry_get_id(self,0);
	}
	else if (!strcmp(attr,"mi")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_offsetrefinfo_entry_get_mi(self,0);
	}
	else if (!strcmp(attr,"di")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_offsetrefinfo_entry_get_di(self,0);
	}
	else if (!strcmp(attr,"cast")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_func_offsetrefinfo_entry_get_cast(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_func_offsetrefinfo_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_func_offsetrefinfo_entry_object* __self = (libftdb_ftdb_func_offsetrefinfo_entry_object*)self;
	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"kind")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"mi")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->mi;
	}
	else if (!strcmp(attr,"di")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->di;
	}
	else if (!strcmp(attr,"cast")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->cast;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

void libftdb_ftdb_funcdecls_dealloc(libftdb_ftdb_funcdecls_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_funcdecls_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_funcdecls_object* self;

    self = (libftdb_ftdb_funcdecls_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_funcdecls_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_funcdecls_object* __self = (libftdb_ftdb_funcdecls_object*)self;
	int written = snprintf(repr,1024,"<ftdbFuncdecls object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld funcdecls>",__self->ftdb->funcdecls_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_funcdecls_sq_length(PyObject* self) {

	libftdb_ftdb_funcdecls_object* __self = (libftdb_ftdb_funcdecls_object*)self;
	return __self->ftdb->funcdecls_count;
}

PyObject* libftdb_ftdb_funcdecls_getiter(PyObject* self) {

	libftdb_ftdb_funcdecls_object* __self = (libftdb_ftdb_funcdecls_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsIterType, args);
	Py_DecRef(args);
	return iter;
}

void libftdb_ftdb_funcdecls_iter_dealloc(libftdb_ftdb_funcdecls_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_funcdecls_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_funcdecls_iter_object* self;

    self = (libftdb_ftdb_funcdecls_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_funcdecls_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_funcdecls_iter_sq_length(PyObject* self) {

	libftdb_ftdb_funcdecls_iter_object* __self = (libftdb_ftdb_funcdecls_iter_object*)self;
	return __self->ftdb->funcdecls_count;
}

PyObject* libftdb_ftdb_funcdecls_iter_next(PyObject *self) {

	libftdb_ftdb_funcdecls_iter_object* __self = (libftdb_ftdb_funcdecls_iter_object*)self;

	if (__self->index >= __self->ftdb->funcdecls_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PYTUPLE_SET_ULONG(args,1,__self->index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsEntryType, args);
	Py_DecRef(args);
	__self->index++;
	return entry;
}

PyObject* libftdb_ftdb_funcdecls_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_funcdecls_object* __self = (libftdb_ftdb_funcdecls_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long id = PyLong_AsUnsignedLong(slice);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&__self->ftdb->fdrefmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function decl id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_funcdecl_entry* func_entry = (struct ftdb_funcdecl_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else if (PyUnicode_Check(slice)) {
		const char* hash = PyString_get_c_str(slice);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&__self->ftdb->fdhrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function decl hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_funcdecl_entry* func_entry = (struct ftdb_funcdecl_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcdecls_entry_by_id(libftdb_ftdb_funcdecls_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->fdrefmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function decl id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_funcdecl_entry* func_entry = (struct ftdb_funcdecl_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,func_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcdecls_contains_id(libftdb_ftdb_funcdecls_object *self, PyObject *args) {

	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->fdrefmap, id);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcdecls_entry_by_hash(libftdb_ftdb_funcdecls_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->fdhrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function decl hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_funcdecl_entry* funcdecl_entry = (struct ftdb_funcdecl_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,funcdecl_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcdecls_contains_hash(libftdb_ftdb_funcdecls_object *self, PyObject *args) {

	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->fdhrefmap, hash);
		PYASSTR_DECREF(hash);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcdecls_entry_by_name(libftdb_ftdb_funcdecls_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_name = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_name)) {
		const char* name = PyString_get_c_str(py_name);
		struct stringRef_entryListMap_node* node = stringRef_entryListMap_search(&self->ftdb->fdnrefmap, name);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function declaration name not present in the array: %s\n",name);
			PYASSTR_DECREF(name);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(name);
		struct ftdb_funcdecl_entry** funcdecl_entry_list = (struct ftdb_funcdecl_entry**)node->entry_list;
		PyObject* entry_list = PyList_New(0);
		for (unsigned long i=0; i<node->entry_count; ++i) {
			struct ftdb_funcdecl_entry* funcdecl_entry = (struct ftdb_funcdecl_entry*)(funcdecl_entry_list[i]);
			PyObject* args = PyTuple_New(2);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
			PYTUPLE_SET_ULONG(args,1,funcdecl_entry->__index);
			PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFuncdeclsEntryType, args);
			Py_DecRef(args);
			PyList_Append(entry_list,entry);
			Py_DecRef(entry);
		}
		return entry_list;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_funcdecls_contains_name(libftdb_ftdb_funcdecls_object *self, PyObject *args) {

	PyObject* py_name = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_name)) {
		const char* name = PyString_get_c_str(py_name);
		struct stringRef_entryListMap_node* node = stringRef_entryListMap_search(&self->ftdb->fdnrefmap, name);
		PYASSTR_DECREF(name);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_funcdecls_sq_contains(PyObject* self, PyObject* key) {

	libftdb_ftdb_funcdecls_object* __self = (libftdb_ftdb_funcdecls_object*)self;

	if (PyUnicode_Check(key)) {
		/* Check hash */
		PyObject* v = libftdb_ftdb_funcdecls_contains_hash(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}
	else if (PyLong_Check(key)) {
		/* Check id */
		PyObject* v = libftdb_ftdb_funcdecls_contains_id(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}

	PyErr_SetString(libftdb_ftdbError, "Invalid key type in contains check (not a str or integer)");
	return 0;
}

void libftdb_ftdb_funcdecl_entry_dealloc(libftdb_ftdb_funcdecl_entry_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_funcdecl_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_funcdecl_entry_object* self;

    self = (libftdb_ftdb_funcdecl_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    	unsigned long index = PyLong_AsLong(PyTuple_GetItem(args,1));
    	if (index>=self->ftdb->funcdecls_count) {
    		PyErr_SetString(libftdb_ftdbError, "funcdecl entry index out of bounds");
    		return 0;
    	}
    	self->index = index;
    	self->entry = &self->ftdb->funcdecls[self->index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_funcdecl_entry_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	int written = snprintf(repr,1024,"<ftdbFuncdeclEntry object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"id:%lu|name:%s>",__self->entry->id,__self->entry->name);

	return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_funcdecl_entry_json(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args) {

	PyObject* json_entry = PyDict_New();

	FTDB_SET_ENTRY_STRING(json_entry,name,self->entry->name);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,namespace,self->entry->__namespace);
	FTDB_SET_ENTRY_ULONG(json_entry,id,self->entry->id);
	FTDB_SET_ENTRY_ULONG(json_entry,fid,self->entry->fid);
	FTDB_SET_ENTRY_ULONG(json_entry,nargs,self->entry->nargs);
	FTDB_SET_ENTRY_BOOL(json_entry,variadic,self->entry->isvariadic);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,template,self->entry->istemplate);
	FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry,linkage,self->entry->linkage,functionLinkage);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,member,self->entry->ismember);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,class,self->entry->__class);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry,classid,self->entry->classid);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,template_parameters,self->entry->template_parameters);
	FTDB_SET_ENTRY_STRING(json_entry,decl,self->entry->decl);
	FTDB_SET_ENTRY_STRING(json_entry,signature,self->entry->signature);
	FTDB_SET_ENTRY_STRING(json_entry,declhash,self->entry->declhash);
	FTDB_SET_ENTRY_STRING(json_entry,location,self->entry->location);
	FTDB_SET_ENTRY_ULONG(json_entry,refcount,self->entry->refcount);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,types,self->entry->types);

	return json_entry;
}

PyObject* libftdb_ftdb_funcdecl_entry_has_namespace(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args) {

	if (self->entry->__namespace) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_funcdecl_entry_has_class(libftdb_ftdb_funcdecl_entry_object *self, PyObject *args) {

	if ((self->entry->__class)||(self->entry->classid)) {
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

PyObject* libftdb_ftdb_funcdecl_entry_get_id(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->id);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_name(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyUnicode_FromString(__self->entry->name);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_namespace(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if (!__self->entry->__namespace) {
		PyErr_SetString(libftdb_ftdbError, "No 'namespace' field in function decl entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->__namespace);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_fid(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->fid);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_nargs(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->nargs);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_variadic(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if (__self->entry->isvariadic) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_funcdecl_entry_get_template(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if ((__self->entry->istemplate) && (*__self->entry->istemplate)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_funcdecl_entry_get_linkage(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyLong_FromLong((long)__self->entry->linkage);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_linkage_string(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyUnicode_FromString(set_functionLinkage(__self->entry->linkage));
}

PyObject* libftdb_ftdb_funcdecl_entry_get_member(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if ((__self->entry->ismember) && (*__self->entry->ismember)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_funcdecl_entry_get_class(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if (!__self->entry->__class) {
		PyErr_SetString(libftdb_ftdbError, "No 'class' field in function decl entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->__class);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_classid(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if (!__self->entry->classid) {
		PyErr_SetString(libftdb_ftdbError, "No 'classid' field in function decl entry");
		return 0;
	}

	return PyLong_FromUnsignedLong(*__self->entry->classid);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_template_parameters(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	if (!__self->entry->template_parameters) {
		PyErr_SetString(libftdb_ftdbError, "No 'template_parameters' field in function decl entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->template_parameters);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_decl(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyUnicode_FromString(__self->entry->decl);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_signature(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyUnicode_FromString(__self->entry->signature);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_declhash(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyUnicode_FromString(__self->entry->declhash);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_location(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyUnicode_FromString(__self->entry->location);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_refcount(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->refcount);
}

PyObject* libftdb_ftdb_funcdecl_entry_get_types(PyObject* self, void* closure) {

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;

	PyObject* types = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->types_count; ++i) {
		PYLIST_ADD_ULONG(types,__self->entry->types[i]);
	}
	return types;
}

PyObject* libftdb_ftdb_funcdecl_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_id(self,0);
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_name(self,0);
	}
	else if (!strcmp(attr,"namespace")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_namespace(self,0);
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_fid(self,0);
	}
	else if (!strcmp(attr,"nargs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_nargs(self,0);
	}
	else if (!strcmp(attr,"variadic")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_variadic(self,0);
	}
	else if (!strcmp(attr,"template")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_template(self,0);
	}
	else if (!strcmp(attr,"linkage")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_linkage_string(self,0);
	}
	else if (!strcmp(attr,"member")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_member(self,0);
	}
	else if (!strcmp(attr,"class")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_class(self,0);
	}
	else if (!strcmp(attr,"classid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_classid(self,0);
	}
	else if (!strcmp(attr,"template_parameters")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_template_parameters(self,0);
	}
	else if (!strcmp(attr,"decl")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_decl(self,0);
	}
	else if (!strcmp(attr,"signature")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_signature(self,0);
	}
	else if (!strcmp(attr,"declhash")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_declhash(self,0);
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_location(self,0);
	}
	else if (!strcmp(attr,"refcount")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_refcount(self,0);
	}
	else if (!strcmp(attr,"types")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_funcdecl_entry_get_types(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_funcdecl_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	libftdb_ftdb_funcdecl_entry_object* __self = (libftdb_ftdb_funcdecl_entry_object*)self;
	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"namespace")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->__namespace;
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"nargs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"variadic")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"template")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->istemplate;
	}
	else if (!strcmp(attr,"linkage")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"member")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->ismember;
	}
	else if (!strcmp(attr,"class")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->__class;
	}
	else if (!strcmp(attr,"classid")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->classid;
	}
	else if (!strcmp(attr,"template_parameters")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->template_parameters;
	}
	else if (!strcmp(attr,"decl")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"signature")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"declhash")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refcount")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"types")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	return 0;
}

PyObject* libftdb_ftdb_funcdecl_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

void libftdb_ftdb_unresolvedfuncs_dealloc(libftdb_ftdb_unresolvedfuncs_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_unresolvedfuncs_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_unresolvedfuncs_object* self;

    self = (libftdb_ftdb_unresolvedfuncs_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_unresolvedfuncs_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_unresolvedfuncs_object* __self = (libftdb_ftdb_unresolvedfuncs_object*)self;
	int written = snprintf(repr,1024,"<ftdbUnresolvedfuncs object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld unresolvedfuncs>",__self->ftdb->unresolvedfuncs_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_unresolvedfuncs_sq_length(PyObject* self) {

	libftdb_ftdb_unresolvedfuncs_object* __self = (libftdb_ftdb_unresolvedfuncs_object*)self;
	return __self->ftdb->unresolvedfuncs_count;
}

PyObject* libftdb_ftdb_unresolvedfuncs_getiter(PyObject* self) {

	libftdb_ftdb_unresolvedfuncs_object* __self = (libftdb_ftdb_unresolvedfuncs_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbUnresolvedfuncsIterType, args);

	Py_DecRef(args);
	return iter;
}

PyObject* libftdb_ftdb_unresolvedfuncs_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

void libftdb_ftdb_unresolvedfuncs_iter_dealloc(libftdb_ftdb_unresolvedfuncs_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_unresolvedfuncs_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_unresolvedfuncs_iter_object* self;

    self = (libftdb_ftdb_unresolvedfuncs_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_unresolvedfuncs_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_unresolvedfuncs_iter_sq_length(PyObject* self) {

	libftdb_ftdb_unresolvedfuncs_iter_object* __self = (libftdb_ftdb_unresolvedfuncs_iter_object*)self;
	return __self->ftdb->unresolvedfuncs_count;
}

PyObject* libftdb_ftdb_unresolvedfuncs_iter_next(PyObject *self) {

	libftdb_ftdb_unresolvedfuncs_iter_object* __self = (libftdb_ftdb_unresolvedfuncs_iter_object*)self;

	if (__self->index >= __self->ftdb->unresolvedfuncs_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,__self->ftdb->unresolvedfuncs[__self->index].id);
	PYTUPLE_SET_STR(args,1,__self->ftdb->unresolvedfuncs[__self->index].name);
	__self->index++;
	return args;
}

void libftdb_ftdb_globals_dealloc(libftdb_ftdb_globals_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_globals_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_globals_object* self;

    self = (libftdb_ftdb_globals_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_globals_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_globals_object* __self = (libftdb_ftdb_globals_object*)self;
	int written = snprintf(repr,1024,"<ftdbGlobals object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld globals>",__self->ftdb->globals_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_globals_sq_length(PyObject* self) {

	libftdb_ftdb_globals_object* __self = (libftdb_ftdb_globals_object*)self;
	return __self->ftdb->globals_count;
}

PyObject* libftdb_ftdb_globals_getiter(PyObject* self) {

	libftdb_ftdb_globals_object* __self = (libftdb_ftdb_globals_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalsIterType, args);

	Py_DecRef(args);
	return iter;
}

PyObject* libftdb_ftdb_globals_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_globals_object* __self = (libftdb_ftdb_globals_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long id = PyLong_AsUnsignedLong(slice);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&__self->ftdb->grefmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid global id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_global_entry* global_entry = (struct ftdb_global_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,global_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else if (PyUnicode_Check(slice)) {
		const char* hash = PyString_get_c_str(slice);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&__self->ftdb->ghrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid global hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_global_entry* global_entry = (struct ftdb_global_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,global_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_globals_entry_by_id(libftdb_ftdb_globals_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->grefmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid global id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_global_entry* global_entry = (struct ftdb_global_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,global_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_globals_contains_id(libftdb_ftdb_globals_object *self, PyObject *args) {

	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->grefmap, id);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}

}

PyObject* libftdb_ftdb_globals_entry_by_hash(libftdb_ftdb_globals_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->ghrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid global hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_global_entry* global_entry = (struct ftdb_global_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,global_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_globals_contains_hash(libftdb_ftdb_globals_object *self, PyObject *args) {

	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->ghrefmap, hash);
		PYASSTR_DECREF(hash);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_globals_entry_by_name(libftdb_ftdb_globals_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_name = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_name)) {
		const char* name = PyString_get_c_str(py_name);
		struct stringRef_entryListMap_node* node = stringRef_entryListMap_search(&self->ftdb->gnrefmap, name);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid global name not present in the array: %s\n",name);
			PYASSTR_DECREF(name);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(name);
		struct ftdb_global_entry** func_entry_list = (struct ftdb_global_entry**)node->entry_list;
		PyObject* entry_list = PyList_New(0);
		for (unsigned long i=0; i<node->entry_count; ++i) {
			struct ftdb_global_entry* global_entry = (struct ftdb_global_entry*)(func_entry_list[i]);
			PyObject* args = PyTuple_New(2);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
			PYTUPLE_SET_ULONG(args,1,global_entry->__index);
			PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalEntryType, args);
			Py_DecRef(args);
			PyList_Append(entry_list,entry);
		}
		return entry_list;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_globals_contains_name(libftdb_ftdb_globals_object *self, PyObject *args) {

	PyObject* py_name = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_name)) {
		const char* name = PyString_get_c_str(py_name);
		struct stringRef_entryListMap_node* node = stringRef_entryListMap_search(&self->ftdb->gnrefmap, name);
		PYASSTR_DECREF(name);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_globals_sq_contains(PyObject* self, PyObject* key) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	libftdb_ftdb_globals_object* __self = (libftdb_ftdb_globals_object*)self;

	if (PyUnicode_Check(key)) {
		/* Check name */
		PyObject* v = libftdb_ftdb_globals_contains_name(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}
	else if (PyLong_Check(key)) {
		/* Check id */
		PyObject* v = libftdb_ftdb_globals_contains_id(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}
	else if (PyTuple_Check(key)) {
		/* Check hash */
		if (PyTuple_Size(key)!=1) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid size of contains check tuple: %ld (should be 1)",PyTuple_Size(key));
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PyObject* v = libftdb_ftdb_globals_contains_hash(__self,PyTuple_GetItem(key,0));
		if (v) {
			return PyObject_IsTrue(v);
		}
	}

	PyErr_SetString(libftdb_ftdbError, "Invalid key type in contains check (not a str, tuple or integer)");
	return 0;
}

PyObject* libftdb_ftdb_globals_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

void libftdb_ftdb_globals_iter_dealloc(libftdb_ftdb_globals_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_globals_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {


	libftdb_ftdb_globals_iter_object* self;

    self = (libftdb_ftdb_globals_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_globals_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_globals_iter_sq_length(PyObject* self) {

	libftdb_ftdb_globals_iter_object* __self = (libftdb_ftdb_globals_iter_object*)self;
	return __self->ftdb->globals_count;
}

PyObject* libftdb_ftdb_globals_iter_next(PyObject *self) {

	libftdb_ftdb_globals_iter_object* __self = (libftdb_ftdb_globals_iter_object*)self;

	if (__self->index >= __self->ftdb->globals_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PYTUPLE_SET_ULONG(args,1,__self->index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbGlobalEntryType, args);
	Py_DecRef(args);
	__self->index++;
	return entry;
}

void libftdb_ftdb_global_entry_dealloc(libftdb_ftdb_global_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_global_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_global_entry_object* self;

    self = (libftdb_ftdb_global_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    	unsigned long index = PyLong_AsLong(PyTuple_GetItem(args,1));
    	if (index>=self->ftdb->globals_count) {
    		PyErr_SetString(libftdb_ftdbError, "global entry index out of bounds");
    		return 0;
    	}
    	self->index = index;
    	self->entry = &self->ftdb->globals[self->index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_global_entry_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	int written = snprintf(repr,1024,"<ftdbGlobalEntry object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"id:%lu|name:%s>",__self->entry->id,__self->entry->name);

	return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_global_entry_has_init(libftdb_ftdb_global_entry_object *self, PyObject *args) {

	if (self->entry->hasinit) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_global_entry_get_name(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(__self->entry->name);
}

PyObject* libftdb_ftdb_global_entry_get_hash(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(__self->entry->hash);
}

PyObject* libftdb_ftdb_global_entry_get_id(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->id);
}

PyObject* libftdb_ftdb_global_entry_get_fid(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->fid);
}

PyObject* libftdb_ftdb_global_entry_get_def(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(__self->entry->def);
}

PyObject* libftdb_ftdb_global_entry_get_file(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(__self->entry->file);
}

PyObject* libftdb_ftdb_global_entry_get_type(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->type);
}

PyObject* libftdb_ftdb_global_entry_get_linkage(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyLong_FromLong((long)__self->entry->linkage);
}

PyObject* libftdb_ftdb_global_entry_get_linkage_string(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(set_functionLinkage(__self->entry->linkage));
}

PyObject* libftdb_ftdb_global_entry_get_location(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(__self->entry->location);
}

PyObject* libftdb_ftdb_global_entry_get_deftype(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyLong_FromLong((long)__self->entry->deftype);
}

PyObject* libftdb_ftdb_global_entry_get_init(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyUnicode_FromString(__self->entry->init);
}

PyObject* libftdb_ftdb_global_entry_get_globalrefs(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* globalrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->globalrefs_count; ++i) {
		PYLIST_ADD_ULONG(globalrefs,__self->entry->globalrefs[i]);
	}
	return globalrefs;
}

PyObject* libftdb_ftdb_global_entry_get_refs(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* refs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refs_count; ++i) {
		PYLIST_ADD_ULONG(refs,__self->entry->refs[i]);
	}
	return refs;
}

PyObject* libftdb_ftdb_global_entry_get_funrefs(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* funrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->funrefs_count; ++i) {
		PYLIST_ADD_ULONG(funrefs,__self->entry->funrefs[i]);
	}
	return funrefs;
}

PyObject* libftdb_ftdb_global_entry_get_decls(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* decls = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->decls_count; ++i) {
		PYLIST_ADD_ULONG(decls,__self->entry->decls[i]);
	}
	return decls;
}

PyObject* libftdb_ftdb_global_entry_get_mids(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	if (!__self->entry->mids) {
		Py_RETURN_NONE;
	}

	PyObject* mids = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->mids_count; ++i) {
		PYLIST_ADD_ULONG(mids,__self->entry->mids[i]);
	}
	return mids;
}

PyObject* libftdb_ftdb_global_entry_get_literals(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* literals = PyDict_New();
	FTDB_SET_ENTRY_INT64_ARRAY(literals,integer,__self->entry->integer_literals);
	FTDB_SET_ENTRY_UINT_ARRAY(literals,character,__self->entry->character_literals);
	FTDB_SET_ENTRY_FLOAT_ARRAY(literals,floating,__self->entry->floating_literals);
	FTDB_SET_ENTRY_STRING_ARRAY(literals,string,__self->entry->string_literals);

	return literals;
}

PyObject* libftdb_ftdb_global_entry_get_hasinit(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;
	return PyLong_FromLong(__self->entry->hasinit);
}

PyObject* libftdb_ftdb_global_entry_get_integer_literals(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* integer_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->integer_literals_count; ++i) {
		PYLIST_ADD_LONG(integer_literals,__self->entry->integer_literals[i]);
	}
	return integer_literals;
}

PyObject* libftdb_ftdb_global_entry_get_character_literals(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* character_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->character_literals_count; ++i) {
		PYLIST_ADD_LONG(character_literals,__self->entry->character_literals[i]);
	}
	return character_literals;
}

PyObject* libftdb_ftdb_global_entry_get_floating_literals(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* floating_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->floating_literals_count; ++i) {
		PYLIST_ADD_FLOAT(floating_literals,__self->entry->floating_literals[i]);
	}
	return floating_literals;
}

PyObject* libftdb_ftdb_global_entry_get_string_literals(PyObject* self, void* closure) {

	libftdb_ftdb_global_entry_object* __self = (libftdb_ftdb_global_entry_object*)self;

	PyObject* string_literals = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->string_literals_count; ++i) {
		PYLIST_ADD_STRING(string_literals,__self->entry->string_literals[i]);
	}
	return string_literals;
}

PyObject* libftdb_ftdb_global_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_name(self,0);
	}
	else if (!strcmp(attr,"hash")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_hash(self,0);
	}
	else if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_id(self,0);
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_fid(self,0);
	}
	else if (!strcmp(attr,"def")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_def(self,0);
	}
	else if (!strcmp(attr,"file")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_file(self,0);
	}
	else if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_type(self,0);
	}
	else if (!strcmp(attr,"linkage")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_linkage_string(self,0);
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_location(self,0);
	}
	else if (!strcmp(attr,"deftype")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_deftype(self,0);
	}
	else if (!strcmp(attr,"init")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_init(self,0);
	}
	else if (!strcmp(attr,"globalrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_globalrefs(self,0);
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_refs(self,0);
	}
	else if (!strcmp(attr,"funrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_funrefs(self,0);
	}
	else if (!strcmp(attr,"decls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_decls(self,0);
	}
	else if (!strcmp(attr,"mids")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_mids(self,0);
	}
	else if (!strcmp(attr,"literals")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_literals(self,0);
	}
	else if (!strcmp(attr,"hasinit")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_global_entry_get_hasinit(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_global_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"hash")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"def")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"file")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"linkage")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"deftype")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"init")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"globalrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"funrefs")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"decls")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"mids")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"literals")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"hasinit")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	return 0;
}

PyObject* libftdb_ftdb_global_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

PyObject* libftdb_ftdb_global_entry_json(libftdb_ftdb_global_entry_object *self, PyObject *args) {

	PyObject* json_entry = PyDict_New();

	FTDB_SET_ENTRY_STRING(json_entry,name,self->entry->name);
	FTDB_SET_ENTRY_STRING(json_entry,hash,self->entry->hash);
	FTDB_SET_ENTRY_ULONG(json_entry,id,self->entry->id);
	FTDB_SET_ENTRY_STRING(json_entry,def,self->entry->def);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,globalrefs,self->entry->globalrefs);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,refs,self->entry->refs);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,funrefs,self->entry->funrefs);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,decls,self->entry->decls);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,mids,self->entry->mids);
	FTDB_SET_ENTRY_ULONG(json_entry,fid,self->entry->fid);
	FTDB_SET_ENTRY_STRING(json_entry,file,self->entry->file);
	FTDB_SET_ENTRY_ULONG(json_entry,type,self->entry->type);
	FTDB_SET_ENTRY_ENUM_TO_STRING(json_entry,linkage,self->entry->linkage,functionLinkage);
	FTDB_SET_ENTRY_STRING(json_entry,location,self->entry->location);
	FTDB_SET_ENTRY_ULONG(json_entry,deftype,(unsigned long)self->entry->deftype);
	FTDB_SET_ENTRY_INT(json_entry,hasinit,self->entry->hasinit);
	FTDB_SET_ENTRY_STRING(json_entry,init,self->entry->init);

	PyObject* literals = PyDict_New();
	FTDB_SET_ENTRY_INT64_ARRAY(literals,integer,self->entry->integer_literals);
	FTDB_SET_ENTRY_INT_ARRAY(literals,character,self->entry->character_literals);
	FTDB_SET_ENTRY_FLOAT_ARRAY(literals,floating,self->entry->floating_literals);
	FTDB_SET_ENTRY_STRING_ARRAY(literals,string,self->entry->string_literals);
	FTDB_SET_ENTRY_PYOBJECT(json_entry,literals,literals);

    return json_entry;
}

void libftdb_ftdb_types_dealloc(libftdb_ftdb_types_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}
PyObject* libftdb_ftdb_types_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_types_object* self;

    self = (libftdb_ftdb_types_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_types_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_types_object* __self = (libftdb_ftdb_types_object*)self;
	int written = snprintf(repr,1024,"<ftdbTypes object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld types>",__self->ftdb->types_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_types_sq_length(PyObject* self) {

	libftdb_ftdb_types_object* __self = (libftdb_ftdb_types_object*)self;
	return __self->ftdb->types_count;
}

PyObject* libftdb_ftdb_types_getiter(PyObject* self) {

	libftdb_ftdb_types_object* __self = (libftdb_ftdb_types_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbTypesIterType, args);

	Py_DecRef(args);
	return iter;
}

PyObject* libftdb_ftdb_types_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_types_object* __self = (libftdb_ftdb_types_object*)self;

	if (PyLong_Check(slice)) {
		unsigned long id = PyLong_AsUnsignedLong(slice);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&__self->ftdb->refmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid type id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_type_entry* type_entry = (struct ftdb_type_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,type_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbTypeEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else if (PyUnicode_Check(slice)) {
		const char* hash = PyString_get_c_str(slice);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&__self->ftdb->hrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid type hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_type_entry* type_entry = (struct ftdb_type_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
		PYTUPLE_SET_ULONG(args,1,type_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbTypeEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}

}

PyObject* libftdb_ftdb_types_contains_hash(libftdb_ftdb_types_object *self, PyObject *args) {

	PyObject* py_hash = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->hrefmap, hash);
		PYASSTR_DECREF(hash);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_types_contains_id(libftdb_ftdb_types_object *self, PyObject *args) {

	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->refmap, id);
		if (node) {
			Py_RETURN_TRUE;
		}
		else {
			Py_RETURN_FALSE;
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_types_entry_by_hash(libftdb_ftdb_types_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_hash = PyTuple_GetItem(args,0);
	if (PyUnicode_Check(py_hash)) {
		const char* hash = PyString_get_c_str(py_hash);
		struct stringRef_entryMap_node* node = stringRef_entryMap_search(&self->ftdb->hrefmap, hash);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function hash not present in the array: %s\n",hash);
			PYASSTR_DECREF(hash);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		PYASSTR_DECREF(hash);
		struct ftdb_type_entry* types_entry = (struct ftdb_type_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,types_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbTypeEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

PyObject* libftdb_ftdb_types_entry_by_id(libftdb_ftdb_types_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	PyObject* py_id = PyTuple_GetItem(args,0);

	if (PyLong_Check(py_id)) {
		unsigned long id = PyLong_AsUnsignedLong(py_id);
		struct ulong_entryMap_node* node = ulong_entryMap_search(&self->ftdb->refmap, id);
		if (!node) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"invalid function id not present in the array: %lu\n",id);
			PyErr_SetString(libftdb_ftdbError, errmsg);
			return 0;
		}
		struct ftdb_type_entry* types_entry = (struct ftdb_type_entry*)node->entry;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,types_entry->__index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbTypeEntryType, args);
		Py_DecRef(args);
		return entry;
	}
	else {
		PyErr_SetString(libftdb_ftdbError, "Invalid subscript argument");
		return 0;
	}
}

int libftdb_ftdb_types_sq_contains(PyObject* self, PyObject* key) {

	libftdb_ftdb_types_object* __self = (libftdb_ftdb_types_object*)self;

	if (PyUnicode_Check(key)) {
		/* Check hash */
		PyObject* v = libftdb_ftdb_types_contains_hash(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}
	else if (PyLong_Check(key)) {
		/* Check id */
		PyObject* v = libftdb_ftdb_types_contains_id(__self,key);
		if (v) {
			return PyObject_IsTrue(v);
		}
	}

	PyErr_SetString(libftdb_ftdbError, "Invalid key type in contains check (not a str or integer)");
	return 0;


}

void libftdb_ftdb_types_iter_dealloc(libftdb_ftdb_types_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_types_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_types_iter_object* self;

    self = (libftdb_ftdb_types_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_types_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_types_iter_sq_length(PyObject* self) {

	libftdb_ftdb_types_iter_object* __self = (libftdb_ftdb_types_iter_object*)self;
	return __self->ftdb->types_count;
}

PyObject* libftdb_ftdb_types_iter_next(PyObject *self) {

	libftdb_ftdb_types_iter_object* __self = (libftdb_ftdb_types_iter_object*)self;

	if (__self->index >= __self->ftdb->types_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PYTUPLE_SET_ULONG(args,1,__self->index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbTypeEntryType, args);
	Py_DecRef(args);
	__self->index++;
	return entry;
}

void libftdb_ftdb_type_entry_dealloc(libftdb_ftdb_type_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_type_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_type_entry_object* self;

    self = (libftdb_ftdb_type_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    	unsigned long index = PyLong_AsLong(PyTuple_GetItem(args,1));
    	if (index>=self->ftdb->types_count) {
    		PyErr_SetString(libftdb_ftdbError, "type entry index out of bounds");
    		return 0;
    	}
    	self->index = index;
    	self->entry = &self->ftdb->types[self->index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_type_entry_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	int written = snprintf(repr,1024,"<ftdbTypeEntry object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"id:%lu|class:%s,str:%s>",__self->entry->id,__self->entry->class_name,__self->entry->str);

	return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_type_entry_get_id(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->id);
}

PyObject* libftdb_ftdb_type_entry_get_fid(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->fid);
}

PyObject* libftdb_ftdb_type_entry_get_hash(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyUnicode_FromString(__self->entry->hash);
}

PyObject* libftdb_ftdb_type_entry_get_classname(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyUnicode_FromString(__self->entry->class_name);
}

PyObject* libftdb_ftdb_type_entry_get_classid(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyLong_FromLong((long)__self->entry->__class);
}

PyObject* libftdb_ftdb_type_entry_get_qualifiers(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyUnicode_FromString(__self->entry->qualifiers);
}

PyObject* libftdb_ftdb_type_entry_get_size(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->size);
}

PyObject* libftdb_ftdb_type_entry_get_str(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyUnicode_FromString(__self->entry->str);
}

PyObject* libftdb_ftdb_type_entry_get_refcount(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->entry->refcount);
}

PyObject* libftdb_ftdb_type_entry_get_refs(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	PyObject* refs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refs_count; ++i) {
		PYLIST_ADD_ULONG(refs,__self->entry->refs[i]);
	}
	return refs;
}

PyObject* libftdb_ftdb_type_entry_get_usedrefs(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->usedrefs) {
		PyErr_SetString(libftdb_ftdbError, "No 'usedrefs' field in type entry");
		return 0;
	}

	PyObject* usedrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->usedrefs_count; ++i) {
		PYLIST_ADD_LONG(usedrefs,__self->entry->usedrefs[i]);
	}
	return usedrefs;
}

PyObject* libftdb_ftdb_type_entry_get_decls(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->decls) {
		PyErr_SetString(libftdb_ftdbError, "No 'decls' field in type entry");
		return 0;
	}

	PyObject* decls = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->decls_count; ++i) {
		PYLIST_ADD_ULONG(decls,__self->entry->decls[i]);
	}
	return decls;
}

PyObject* libftdb_ftdb_type_entry_get_def(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->def) {
		PyErr_SetString(libftdb_ftdbError, "No 'def' field in type entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->def);
}

PyObject* libftdb_ftdb_type_entry_get_memberoffsets(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->memberoffsets) {
		PyErr_SetString(libftdb_ftdbError, "No 'memberoffsets' field in type entry");
		return 0;
	}

	PyObject* memberoffsets = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->memberoffsets_count; ++i) {
		PYLIST_ADD_ULONG(memberoffsets,__self->entry->memberoffsets[i]);
	}
	return memberoffsets;
}

PyObject* libftdb_ftdb_type_entry_get_attrrefs(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->attrrefs) {
		PyErr_SetString(libftdb_ftdbError, "No 'attrrefs' field in type entry");
		return 0;
	}

	PyObject* attrrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->attrrefs_count; ++i) {
		PYLIST_ADD_ULONG(attrrefs,__self->entry->attrrefs[i]);
	}
	return attrrefs;
}

PyObject* libftdb_ftdb_type_entry_get_attrnum(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->attrnum) {
		return PyLong_FromUnsignedLong(0);
	}

	return PyLong_FromUnsignedLong(*__self->entry->attrnum);
}

PyObject* libftdb_ftdb_type_entry_get_name(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->name) {
		PyErr_SetString(libftdb_ftdbError, "No 'name' field in type entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->name);
}

PyObject* libftdb_ftdb_type_entry_get_is_dependent(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if ((__self->entry->isdependent) && (*__self->entry->isdependent)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_get_globalrefs(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->globalrefs) {
		PyErr_SetString(libftdb_ftdbError, "No 'globalrefs' field in type entry");
		return 0;
	}

	PyObject* globalrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->globalrefs_count; ++i) {
		PYLIST_ADD_ULONG(globalrefs,__self->entry->globalrefs[i]);
	}
	return globalrefs;
}

PyObject* libftdb_ftdb_type_entry_get_enumvalues(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->enumvalues) {
		PyErr_SetString(libftdb_ftdbError, "No 'enumvalues' field in type entry");
		return 0;
	}

	PyObject* enumvalues = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->enumvalues_count; ++i) {
		PYLIST_ADD_LONG(enumvalues,__self->entry->enumvalues[i]);
	}
	return enumvalues;
}

PyObject* libftdb_ftdb_type_entry_get_outerfn(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->outerfn) {
		PyErr_SetString(libftdb_ftdbError, "No 'outerfn' field in type entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->outerfn);
}

PyObject* libftdb_ftdb_type_entry_get_outerfnid(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->outerfnid) {
		PyErr_SetString(libftdb_ftdbError, "No 'outerfnid' field in type entry");
		return 0;
	}

	return PyLong_FromUnsignedLong(*__self->entry->outerfnid);
}

PyObject* libftdb_ftdb_type_entry_get_is_implicit(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if ((__self->entry->isimplicit) && (*__self->entry->isimplicit)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_get_is_union(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if ((__self->entry->isunion) && (*__self->entry->isunion)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_get_refnames(PyObject* self, void* closure) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->refnames) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"No 'refnames' field in type entry ([%lu])\n",__self->entry->refnames_count);
		PyErr_SetString(libftdb_ftdbError, errmsg);
		return 0;
	}

	PyObject* refnames = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->refnames_count; ++i) {
		PYLIST_ADD_STRING(refnames,__self->entry->refnames[i]);
	}
	return refnames;
}

PyObject* libftdb_ftdb_type_entry_get_location(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->location) {
		PyErr_SetString(libftdb_ftdbError, "No 'location' field in type entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->location);
}

PyObject* libftdb_ftdb_type_entry_get_is_variadic(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if ((__self->entry->isvariadic) && (*__self->entry->isvariadic)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_get_funrefs(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->funrefs) {
		PyErr_SetString(libftdb_ftdbError, "No 'funrefs' field in type entry");
		return 0;
	}

	PyObject* funrefs = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->funrefs_count; ++i) {
		PYLIST_ADD_ULONG(funrefs,__self->entry->funrefs[i]);
	}
	return funrefs;
}

PyObject* libftdb_ftdb_type_entry_get_useddef(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->useddef) {
		PyErr_SetString(libftdb_ftdbError, "No 'useddef' field in type entry");
		return 0;
	}

	PyObject* useddef = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->useddef_count; ++i) {
		PYLIST_ADD_STRING(useddef,__self->entry->useddef[i]);
	}
	return useddef;
}

PyObject* libftdb_ftdb_type_entry_get_defhead(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->defhead) {
		PyErr_SetString(libftdb_ftdbError, "No 'defhead' field in type entry");
		return 0;
	}

	return PyUnicode_FromString(__self->entry->defhead);
}

PyObject* libftdb_ftdb_type_entry_get_identifiers(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->identifiers) {
		PyErr_SetString(libftdb_ftdbError, "No 'identifiers' field in type entry");
		return 0;
	}

	PyObject* identifiers = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->identifiers_count; ++i) {
		PYLIST_ADD_STRING(identifiers,__self->entry->identifiers[i]);
	}
	return identifiers;
}

PyObject* libftdb_ftdb_type_entry_get_methods(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->methods) {
		PyErr_SetString(libftdb_ftdbError, "No 'methods' field in type entry");
		return 0;
	}

	PyObject* methods = PyList_New(0);
	for (unsigned long i=0; i<__self->entry->methods_count; ++i) {
		PYLIST_ADD_ULONG(methods,__self->entry->methods[i]);
	}
	return methods;
}

PyObject* libftdb_ftdb_type_entry_get_bitfields(PyObject* self, void* closure) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!__self->entry->bitfields) {
		PyErr_SetString(libftdb_ftdbError, "No 'bitfields' field in type entry");
		return 0;
	}

	PyObject* bitfields = PyDict_New();
	for (unsigned long i=0; i<__self->entry->bitfields_count; ++i) {
		struct bitfield* bitfield = &__self->entry->bitfields[i];
		PyObject* py_bkey = PyUnicode_FromFormat("%lu",bitfield->index);
		PyObject* py_bval = PyLong_FromUnsignedLong(bitfield->bitwidth);
		PyDict_SetItem(bitfields,py_bkey,py_bval);
		Py_DecRef(py_bkey);
		Py_DecRef(py_bval);
	}
	return bitfields;
}

PyObject* libftdb_ftdb_type_entry_has_usedrefs(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if (self->entry->usedrefs) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_has_decls(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if (self->entry->decls) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_has_attrs(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if ((self->entry->attrnum) && (*self->entry->attrnum>0)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_has_outerfn(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if (self->entry->outerfn) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libftdb_ftdb_type_entry_has_location(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if (self->entry->location) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

static int libftdb_ftdb_type_entry_is_const_internal_type(struct ftdb_type_entry* entry) {

	const char* c = entry->qualifiers;
	while(*c) {
		if (*c++=='c') return 1;
	}
	return 0;
}

static int libftdb_ftdb_type_entry_is_const_internal(libftdb_ftdb_type_entry_object *self) {

	const char* c = self->entry->qualifiers;
	while(*c) {
		if (*c++=='c') return 1;
	}
	return 0;
}

PyObject* libftdb_ftdb_type_entry_is_const(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if (libftdb_ftdb_type_entry_is_const_internal(self)) {
		Py_RETURN_TRUE;
	}
	Py_RETURN_FALSE;
}

static inline const char* __real_hash(const char* hash) {

	int coln = 0;
	while(*hash) {
		if (*hash++==':') coln++;
		if (coln>=2) break;
	}
	return hash;
}

PyObject* libftdb_ftdb_type_entry_to_non_const(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	if (!libftdb_ftdb_type_entry_is_const_internal(self)) {
		Py_IncRef((PyObject*)self);
		return (PyObject*)self;
	}

	for (unsigned long i=0; i<self->ftdb->types_count; ++i) {
		struct ftdb_type_entry* entry = &self->ftdb->types[i];
		if (strcmp(self->entry->str,entry->str)) continue;
		if (entry->__class==TYPECLASS_RECORDFORWARD) continue;
		if (strcmp(__real_hash(self->entry->hash),__real_hash(entry->hash))) continue;
		if (libftdb_ftdb_type_entry_is_const_internal_type(entry)) continue;
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->ftdb);
		PYTUPLE_SET_ULONG(args,1,entry->__index);
		PyObject *py_entry = PyObject_CallObject((PyObject *) &libftdb_ftdbTypeEntryType, args);
		Py_DecRef(args);
		return py_entry;
	}

	Py_IncRef((PyObject*)self);
	return (PyObject*)self;
}

PyObject* libftdb_ftdb_type_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_id(self,0);
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_fid(self,0);
	}
	else if (!strcmp(attr,"hash")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_hash(self,0);
	}
	else if (!strcmp(attr,"class")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_classname(self,0);
	}
	else if (!strcmp(attr,"classid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_classid(self,0);
	}
	else if (!strcmp(attr,"qualifiers")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_qualifiers(self,0);
	}
	else if (!strcmp(attr,"size")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_size(self,0);
	}
	else if (!strcmp(attr,"str")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_str(self,0);
	}
	else if (!strcmp(attr,"refcount")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_refcount(self,0);
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_refs(self,0);
	}
	else if (!strcmp(attr,"usedrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_usedrefs(self,0);
	}
	else if (!strcmp(attr,"decls")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_decls(self,0);
	}
	else if (!strcmp(attr,"def")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_def(self,0);
	}
	else if (!strcmp(attr,"memberoffsets")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_memberoffsets(self,0);
	}
	else if (!strcmp(attr,"attrrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_attrrefs(self,0);
	}
	else if (!strcmp(attr,"attrnum")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_attrnum(self,0);
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_name(self,0);
	}
	else if (!strcmp(attr,"dependent")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_is_dependent(self,0);
	}
	else if (!strcmp(attr,"globalrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_globalrefs(self,0);
	}
	else if (!strcmp(attr,"values")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_enumvalues(self,0);
	}
	else if (!strcmp(attr,"outerfn")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_outerfn(self,0);
	}
	else if (!strcmp(attr,"outerfnid")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_outerfnid(self,0);
	}
	else if (!strcmp(attr,"implicit")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_is_implicit(self,0);
	}
	else if (!strcmp(attr,"union")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_is_union(self,0);
	}
	else if (!strcmp(attr,"refnames")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_refnames(self,0);
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_location(self,0);
	}
	else if (!strcmp(attr,"variadic")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_is_variadic(self,0);
	}
	else if (!strcmp(attr,"funrefs")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_funrefs(self,0);
	}
	else if (!strcmp(attr,"useddef")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_useddef(self,0);
	}
	else if (!strcmp(attr,"defhead")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_defhead(self,0);
	}
	else if (!strcmp(attr,"identifiers")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_identifiers(self,0);
	}
	else if (!strcmp(attr,"methods")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_methods(self,0);
	}
	else if (!strcmp(attr,"bitfields")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_type_entry_get_bitfields(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_type_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);
	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;

	if (!strcmp(attr,"id")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"fid")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"hash")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"class")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"classid")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"qualifiers")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"size")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"str")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"refcount")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"refs")) {
		PYASSTR_DECREF(attr);
		return 1;
	}
	else if (!strcmp(attr,"usedrefs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->usedrefs;
	}
	else if (!strcmp(attr,"decls")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->decls;
	}
	else if (!strcmp(attr,"def")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->def;
	}
	else if (!strcmp(attr,"memberoffsets")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->memberoffsets;
	}
	else if (!strcmp(attr,"attrrefs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->attrrefs;
	}
	else if (!strcmp(attr,"attrnum")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->attrnum;
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->name;
	}
	else if (!strcmp(attr,"dependent")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->isdependent;
	}
	else if (!strcmp(attr,"globalrefs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->globalrefs;
	}
	else if (!strcmp(attr,"values")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->enumvalues;
	}
	else if (!strcmp(attr,"outerfn")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->outerfn;
	}
	else if (!strcmp(attr,"outerfnid")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->outerfnid;
	}
	else if (!strcmp(attr,"implicit")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->isimplicit;
	}
	else if (!strcmp(attr,"union")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->isunion;
	}
	else if (!strcmp(attr,"refnames")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->refnames;
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->location;
	}
	else if (!strcmp(attr,"variadic")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->isvariadic;
	}
	else if (!strcmp(attr,"funrefs")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->funrefs;
	}
	else if (!strcmp(attr,"useddef")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->useddef;
	}
	else if (!strcmp(attr,"defhead")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->defhead;
	}
	else if (!strcmp(attr,"identifiers")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->identifiers;
	}
	else if (!strcmp(attr,"methods")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->methods;
	}
	else if (!strcmp(attr,"bitfields")) {
		PYASSTR_DECREF(attr);
	    return !!__self->entry->bitfields;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

PyObject* libftdb_ftdb_type_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

Py_hash_t libftdb_ftdb_type_entry_hash(PyObject *o) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)o;
	return __self->index;
}

PyObject* libftdb_ftdb_type_entry_richcompare(PyObject *self, PyObject *other, int op) {

	libftdb_ftdb_type_entry_object* __self = (libftdb_ftdb_type_entry_object*)self;
	libftdb_ftdb_type_entry_object* __other = (libftdb_ftdb_type_entry_object*)other;
	Py_RETURN_RICHCOMPARE(__self->index, __other->index , op);
}

PyObject* libftdb_ftdb_type_entry_json(libftdb_ftdb_type_entry_object *self, PyObject *args) {

	PyObject* json_entry = PyDict_New();

	FTDB_SET_ENTRY_ULONG(json_entry,id,self->entry->id);
	FTDB_SET_ENTRY_ULONG(json_entry,fid,self->entry->fid);
	FTDB_SET_ENTRY_STRING(json_entry,hash,self->entry->hash);
	FTDB_SET_ENTRY_STRING(json_entry,class,self->entry->class_name);
	FTDB_SET_ENTRY_STRING(json_entry,qualifiers,self->entry->qualifiers);
	FTDB_SET_ENTRY_ULONG(json_entry,size,self->entry->size);
	FTDB_SET_ENTRY_STRING(json_entry,str,self->entry->str);
	FTDB_SET_ENTRY_ULONG(json_entry,refcount,self->entry->refcount);
	FTDB_SET_ENTRY_ULONG_ARRAY(json_entry,refs,self->entry->refs);
	FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(json_entry,usedrefs,self->entry->usedrefs);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,decls,self->entry->decls);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,def,self->entry->def);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,memberoffsets,self->entry->memberoffsets);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,attrrefs,self->entry->attrrefs);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry,attrnum,self->entry->attrnum);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,name,self->entry->name);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,dependent,self->entry->isdependent);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,globalrefs,self->entry->globalrefs);
	FTDB_SET_ENTRY_INT64_ARRAY_OPTIONAL(json_entry,values,self->entry->enumvalues);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,outerfn,self->entry->outerfn);
	FTDB_SET_ENTRY_ULONG_OPTIONAL(json_entry,outerfnid,self->entry->outerfnid);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,implicit,self->entry->isimplicit);
	FTDB_SET_ENTRY_BOOL_OPTIONAL(json_entry,union,self->entry->isunion);
	FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(json_entry,refnames,self->entry->refnames);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,location,self->entry->location);
	FTDB_SET_ENTRY_INT_OPTIONAL(json_entry,variadic,self->entry->isvariadic);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,funrefs,self->entry->funrefs);
	FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(json_entry,useddef,self->entry->useddef);
	FTDB_SET_ENTRY_STRING_OPTIONAL(json_entry,defhead,self->entry->defhead);
	FTDB_SET_ENTRY_STRING_ARRAY_OPTIONAL(json_entry,identifiers,self->entry->identifiers);
	FTDB_SET_ENTRY_ULONG_ARRAY_OPTIONAL(json_entry,methods,self->entry->methods);

	if (self->entry->bitfields) {
		PyObject* bitfields = PyDict_New();
		for (unsigned long i=0; i<self->entry->bitfields_count; ++i) {
			struct bitfield* bitfield = &self->entry->bitfields[i];
			PyObject* py_bkey = PyUnicode_FromFormat("%lu",bitfield->index);
			PyObject* py_bval = PyLong_FromUnsignedLong(bitfield->bitwidth);
			PyDict_SetItem(bitfields,py_bkey,py_bval);
			Py_DecRef(py_bkey);
			Py_DecRef(py_bval);
		}
		FTDB_SET_ENTRY_PYOBJECT(json_entry,bitfields,bitfields);
	}

	return json_entry;
}

void libftdb_ftdb_fops_dealloc(libftdb_ftdb_fops_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_fops_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_fops_object* self;

    self = (libftdb_ftdb_fops_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_fops_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_fops_object* __self = (libftdb_ftdb_fops_object*)self;
	int written = snprintf(repr,1024,"<ftdbFops object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"%ld fops variables>",__self->ftdb->fops.vars_count);

	return PyUnicode_FromString(repr);
}

Py_ssize_t libftdb_ftdb_fops_sq_length(PyObject* self) {

	libftdb_ftdb_fops_object* __self = (libftdb_ftdb_fops_object*)self;
	return __self->ftdb->fops.vars_count;
}

PyObject* libftdb_ftdb_fops_getiter(PyObject* self) {

	libftdb_ftdb_fops_object* __self = (libftdb_ftdb_fops_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self);
	PYTUPLE_SET_ULONG(args,1,0);
	PyObject *iter = PyObject_CallObject((PyObject *) &libftdb_ftdbFopsIterType, args);

	Py_DecRef(args);
	return iter;
}

PyObject* libftdb_ftdb_fops_mp_subscript(PyObject* self, PyObject* slice) {

	libftdb_ftdb_fops_object* __self = (libftdb_ftdb_fops_object*)self;

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if ((!PyLong_Check(slice))&&(!PyUnicode_Check(slice))) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type fops index (not an (integer) and not (str))");
		Py_RETURN_NONE;
	}

	if (PyUnicode_Check(slice)) {
		const char* attr = PyString_get_c_str(slice);
		if (!strcmp(attr,"varn")) {
			PYASSTR_DECREF(attr);
			return libftdb_ftdb_fops_get_varn(self,0);
		}
		else if (!strcmp(attr,"membern")) {
			PYASSTR_DECREF(attr);
			return libftdb_ftdb_fops_get_membern(self,0);
		}
		else if (!strcmp(attr,"vars")) {
			PYASSTR_DECREF(attr);
			return libftdb_ftdb_fops_get_vars(self,0);
		}

		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
		PyErr_SetString(libftdb_ftdbError, errmsg);
		PYASSTR_DECREF(attr);
		Py_RETURN_NONE;
	}

	unsigned long index = PyLong_AsLong(slice);

	if (index >= __self->ftdb->fops.vars_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetString(libftdb_ftdbError, "fops index out of range");
		Py_RETURN_NONE;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PYTUPLE_SET_ULONG(args,1,index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFopsEntryType, args);
	Py_DecRef(args);
	return entry;
}

int libftdb_ftdb_fops_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"varn")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"membern")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"vars")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

PyObject* libftdb_ftdb_fops_get_varn(PyObject* self, void* closure) {

	libftdb_ftdb_fops_object* __self = (libftdb_ftdb_fops_object*)self;
	return PyLong_FromUnsignedLong(__self->ftdb->fops.vars_count);
}

PyObject* libftdb_ftdb_fops_get_membern(PyObject* self, void* closure) {

	libftdb_ftdb_fops_object* __self = (libftdb_ftdb_fops_object*)self;
	return PyLong_FromUnsignedLong(__self->ftdb->fops.member_count);
}

PyObject* libftdb_ftdb_fops_get_vars(PyObject* self, void* closure) {

	return libftdb_ftdb_fops_getiter(self);
}

void libftdb_ftdb_fops_iter_dealloc(libftdb_ftdb_fops_iter_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_fops_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_fops_iter_object* self;

    self = (libftdb_ftdb_fops_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = ((libftdb_ftdb_fops_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->ftdb;
    	self->index = PyLong_AsLong(PyTuple_GetItem(args,1));
    }

    return (PyObject *)self;
}

Py_ssize_t libftdb_ftdb_fops_iter_sq_length(PyObject* self) {

	libftdb_ftdb_fops_iter_object* __self = (libftdb_ftdb_fops_iter_object*)self;
	return __self->ftdb->fops.vars_count;
}

PyObject* libftdb_ftdb_fops_iter_next(PyObject *self) {

	libftdb_ftdb_fops_iter_object* __self = (libftdb_ftdb_fops_iter_object*)self;

	if (__self->index >= __self->ftdb->fops.vars_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->ftdb);
	PYTUPLE_SET_ULONG(args,1,__self->index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libftdb_ftdbFopsEntryType, args);
	Py_DecRef(args);
	__self->index++;
	return entry;
}

void libftdb_ftdb_fops_entry_dealloc(libftdb_ftdb_fops_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libftdb_ftdb_fops_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libftdb_ftdb_fops_entry_object* self;

    self = (libftdb_ftdb_fops_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->ftdb = (const struct ftdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    	unsigned long index = PyLong_AsLong(PyTuple_GetItem(args,1));
    	if (index>=self->ftdb->fops.vars_count) {
    		PyErr_SetString(libftdb_ftdbError, "fops entry index out of bounds");
    		return 0;
    	}
    	self->index = index;
    	self->entry = &self->ftdb->fops.vars[self->index];
    }

    return (PyObject *)self;
}

PyObject* libftdb_ftdb_fops_entry_repr(PyObject* self) {

	static char repr[1024];

	libftdb_ftdb_fops_entry_object* __self = (libftdb_ftdb_fops_entry_object*)self;
	int written = snprintf(repr,1024,"<ftdbFopsEntry object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"type:%s|name:%s>",__self->entry->type,__self->entry->name);

	return PyUnicode_FromString(repr);
}

PyObject* libftdb_ftdb_fops_entry_json(libftdb_ftdb_fops_entry_object *self, PyObject *args) {

	PyObject* json_entry = PyDict_New();

	FTDB_SET_ENTRY_STRING(json_entry,type,self->entry->type);
	FTDB_SET_ENTRY_STRING(json_entry,name,self->entry->name);
	PyObject* members = PyDict_New();
	for (unsigned long i=0; i<self->entry->members_count; ++i) {
		struct fops_member_info* fops_member_info = &self->entry->members[i];
		PyObject* py_member = PyUnicode_FromFormat("%lu",fops_member_info->index);
		PyObject* py_fnid = PyLong_FromUnsignedLong(fops_member_info->fnid);
		PyDict_SetItem(members,py_member,py_fnid);
		Py_DecRef(py_member);
		Py_DecRef(py_fnid);
	}
	FTDB_SET_ENTRY_PYOBJECT(json_entry,members,members);
	FTDB_SET_ENTRY_STRING(json_entry,location,self->entry->location);

	return json_entry;
}

PyObject* libftdb_ftdb_fops_entry_get_type(PyObject* self, void* closure) {

	libftdb_ftdb_fops_entry_object* __self = (libftdb_ftdb_fops_entry_object*)self;
	return PyUnicode_FromString(__self->entry->type);
}

PyObject* libftdb_ftdb_fops_entry_get_name(PyObject* self, void* closure) {

	libftdb_ftdb_fops_entry_object* __self = (libftdb_ftdb_fops_entry_object*)self;
	return PyUnicode_FromString(__self->entry->name);
}

PyObject* libftdb_ftdb_fops_entry_get_members(PyObject* self, void* closure) {

	libftdb_ftdb_fops_entry_object* __self = (libftdb_ftdb_fops_entry_object*)self;

	PyObject* members = PyDict_New();
	for (unsigned long i=0; i<__self->entry->members_count; ++i) {
		struct fops_member_info* fops_member_info = &__self->entry->members[i];
		PyObject* py_member = PyUnicode_FromFormat("%lu",fops_member_info->index);
		PyObject* py_fnid = PyLong_FromUnsignedLong(fops_member_info->fnid);
		PyDict_SetItem(members,py_member,py_fnid);
		Py_DecRef(py_member);
		Py_DecRef(py_fnid);
	}

	return members;
}

PyObject* libftdb_ftdb_fops_entry_get_location(PyObject* self, void* closure) {

	libftdb_ftdb_fops_entry_object* __self = (libftdb_ftdb_fops_entry_object*)self;
	return PyUnicode_FromString(__self->entry->location);
}

PyObject* libftdb_ftdb_fops_entry_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (!PyUnicode_Check(slice)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in property mapping (not a str)");
		Py_RETURN_NONE;
	}

	const char* attr = PyString_get_c_str(slice);

	if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_fops_entry_get_type(self,0);
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_fops_entry_get_name(self,0);
	}
	else if (!strcmp(attr,"members")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_fops_entry_get_members(self,0);
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return libftdb_ftdb_fops_entry_get_location(self,0);
	}

	snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid attribute name: %s",attr);
	PyErr_SetString(libftdb_ftdbError, errmsg);
	PYASSTR_DECREF(attr);
	Py_RETURN_NONE;
}

int libftdb_ftdb_fops_entry_sq_contains(PyObject* self, PyObject* key) {

	if (!PyUnicode_Check(key)) {
		PyErr_SetString(libftdb_ftdbError, "Invalid type in contains check (not a str)");
		return 0;
	}

	const char* attr = PyString_get_c_str(key);

	if (!strcmp(attr,"type")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"name")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"members")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}
	else if (!strcmp(attr,"location")) {
		PYASSTR_DECREF(attr);
	    return 1;
	}

	PYASSTR_DECREF(attr);
	return 0;
}

PyObject* libftdb_ftdb_fops_entry_deepcopy(PyObject* self, PyObject* memo) {

    Py_INCREF(self);
    return (PyObject *)self;
}

FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(stringRefMap_node);

static inline const struct stringRefMap_node* stringMap_remove_color(const struct stringRefMap_node* ptr) {
	return (const struct stringRefMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* stringMap_add_color(struct flatten_pointer* fptr, const struct stringRefMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(stringRefMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER_ITER(stringRefMap_node,node.__rb_parent_color,
			stringMap_remove_color,stringMap_add_color);
	AGGREGATE_FLATTEN_STRUCT2_ITER(stringRefMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT2_ITER(stringRefMap_node,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
);

FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb_func_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb_funcdecl_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb_unresolvedfunc_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(taint_element);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(taint_data);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(call_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(callref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(callref_data);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(refcall);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(switch_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(case_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(case_data);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(csitem);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(local_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(deref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(offsetref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(if_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ifref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(asm_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(globalref_data);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(globalref_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb_global_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb_type_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(bitfield);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ftdb_fops_var_entry);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(fops_member_info);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(macroinfo_item);
FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(macro_occurence);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(taint_data,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(taint_element,taint_list,ATTR(taint_list_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(taint_element,
	/* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(call_info,
	AGGREGATE_FLATTEN_STRING2(start);
	AGGREGATE_FLATTEN_STRING2(end);
	AGGREGATE_FLATTEN_STRING2(expr);
	AGGREGATE_FLATTEN_STRING2(loc);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,args,ATTR(args_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(callref_info,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(callref_data,callarg,ATTR(callarg_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(callref_data,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,id,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,integer_literal,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,character_literal,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(double,floating_literal,1);
	AGGREGATE_FLATTEN_STRING2(string_literal);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(refcall,
	/* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(switch_info,
	AGGREGATE_FLATTEN_STRING2(condition);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(case_info,cases,ATTR(cases_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(case_info,
	AGGREGATE_FLATTEN_STRING2(lhs.enum_code);
	AGGREGATE_FLATTEN_STRING2(lhs.macro_code);
	AGGREGATE_FLATTEN_STRING2(lhs.raw_code);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(case_data,rhs,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(case_data,
	AGGREGATE_FLATTEN_STRING2(enum_code);
	AGGREGATE_FLATTEN_STRING2(macro_code);
	AGGREGATE_FLATTEN_STRING2(raw_code);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(csitem,
		AGGREGATE_FLATTEN_STRING2(cf);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(local_info,
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,csid,1);
	AGGREGATE_FLATTEN_STRING2(location);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(deref_info,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,offset,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,basecnt,1);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(offsetref_info,offsetrefs,ATTR(offsetrefs_count));
	AGGREGATE_FLATTEN_STRING2(expr);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,ord,ATTR(ord_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,member,ATTR(member_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,type,ATTR(type_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,access,ATTR(access_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,shift,ATTR(shift_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,mcall,ATTR(mcall_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(offsetref_info,
	if (ATTR(kind)==EXPR_STRING) {
		AGGREGATE_FLATTEN_STRING2(id_string);
	}
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,mi,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,di,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,cast,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(if_info,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ifref_info,refs,ATTR(refs_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ifref_info,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,id,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,integer_literal,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,character_literal,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(double,floating_literal,1);
	AGGREGATE_FLATTEN_STRING2(string_literal);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(asm_info,
	AGGREGATE_FLATTEN_STRING2(str);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(globalref_data,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(globalref_info,refs,ATTR(refs_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(globalref_info,
	AGGREGATE_FLATTEN_STRING2(start);
	AGGREGATE_FLATTEN_STRING2(end);
);

FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_type_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_func_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_funcdecl_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_global_entryMap);

static inline const struct ulong_entryMap_node* ulong_entryMap_remove_color(const struct ulong_entryMap_node* ptr) {
	return (const struct ulong_entryMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* ulong_entryMap_add_color(struct flatten_pointer* fptr, const struct ulong_entryMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_type_entryMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_ulong_type_entryMap,node.__rb_parent_color,
			ulong_entryMap_remove_color,ulong_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_type_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_type_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_type_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_func_entryMap,
	STRUCT_ALIGN(4);
AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_ulong_func_entryMap,node.__rb_parent_color,
			ulong_entryMap_remove_color,ulong_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_func_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_func_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_func_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_funcdecl_entryMap,
	STRUCT_ALIGN(4);
AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_ulong_func_entryMap,node.__rb_parent_color,
			ulong_entryMap_remove_color,ulong_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_funcdecl_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_funcdecl_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_funcdecl_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_global_entryMap,
	STRUCT_ALIGN(4);
AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_ulong_func_entryMap,node.__rb_parent_color,
			ulong_entryMap_remove_color,ulong_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_global_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_global_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_global_entry,entry);
);

FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_type_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_funcdecl_entryMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryMap);

static inline const struct stringRef_entryMap_node* stringRef_entryMap_remove_color(const struct stringRef_entryMap_node* ptr) {
	return (const struct stringRef_entryMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* stringRef_entryMap_add_color(struct flatten_pointer* fptr, const struct stringRef_entryMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_type_entryMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_stringRef_type_entryMap,node.__rb_parent_color,
			stringRef_entryMap_remove_color,stringRef_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_type_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_type_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_type_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_stringRef_func_entryMap,node.__rb_parent_color,
			stringRef_entryMap_remove_color,stringRef_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_func_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_funcdecl_entryMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_stringRef_funcdecl_entryMap,node.__rb_parent_color,
			stringRef_entryMap_remove_color,stringRef_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_funcdecl_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_funcdecl_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_funcdecl_entry,entry);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_stringRef_global_entryMap,node.__rb_parent_color,
			stringRef_entryMap_remove_color,stringRef_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryMap,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ftdb_global_entry,entry);
);

FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryListMap);
FUNCTION_DECLARE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryListMap);

static inline const struct stringRef_entryListMap_node* stringRef_entryListMap_remove_color(const struct stringRef_entryListMap_node* ptr) {
	return (const struct stringRef_entryListMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* stringRef_entryListMap_add_color(struct flatten_pointer* fptr, const struct stringRef_entryListMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryListMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_stringRef_func_entryListMap,node.__rb_parent_color,
			stringRef_entryListMap_remove_color,stringRef_entryListMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryListMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryListMap,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(void*,entry_list,ATTR(entry_count));
	FOREACH_POINTER(void*,entry,ATTR(entry_list),ATTR(entry_count),
		FLATTEN_STRUCT2_ITER(ftdb_func_entry,entry);
	);
);

FUNCTION_DEFINE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryListMap,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_MIXED_POINTER_ITER(ftdb_stringRef_global_entryListMap,node.__rb_parent_color,
			stringRef_entryListMap_remove_color,stringRef_entryListMap_add_color);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryListMap,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryListMap,node.rb_left);
	AGGREGATE_FLATTEN_STRING2(key);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(void*,entry_list,ATTR(entry_count));
	FOREACH_POINTER(void*,entry,ATTR(entry_list),ATTR(entry_count),
		FLATTEN_STRUCT2_ITER(ftdb_global_entry,entry);
	);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(matrix_data,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,data,ATTR(data_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,row_ind,ATTR(row_ind_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,col_ind,ATTR(col_ind_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(func_map_entry,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,fids,ATTR(fids_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(size_dep_item,
    // No recipes needed
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(init_data_item,
        AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,id,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,name,ATTR(name_count));
	FOREACH_POINTER(const char*,s,ATTR(name),ATTR(name_count),
		FLATTEN_STRING(s);
	);
        AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,size,1);
        AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(size_dep_item,size_dep,1);
        AGGREGATE_FLATTEN_STRING2(nullterminated);
        AGGREGATE_FLATTEN_STRING2(tagged);
        AGGREGATE_FLATTEN_STRING2(fuzz);
        AGGREGATE_FLATTEN_STRING2(pointer);
        AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,min_value,1);
        AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,max_value,1);
        AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,value,1);
        AGGREGATE_FLATTEN_STRING2(user_name);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(init_data_entry,
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,order,ATTR(order_count));
	AGGREGATE_FLATTEN_STRING2(interface);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(init_data_item,items,ATTR(items_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(known_data_entry,
	AGGREGATE_FLATTEN_STRING2(version);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,func_ids,ATTR(func_ids_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,builtin_ids,ATTR(builtin_ids_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,asm_ids,ATTR(asm_ids_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,lib_funcs,ATTR(lib_funcs_count));
	FOREACH_POINTER(const char*,s,ATTR(lib_funcs),ATTR(lib_funcs_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,lib_funcs_ids,ATTR(lib_funcs_ids_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,always_inc_funcs_ids,ATTR(always_inc_funcs_ids_count));
	AGGREGATE_FLATTEN_STRING2(source_root);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(BAS_item,
	AGGREGATE_FLATTEN_STRING2(loc);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,entries,ATTR(entries_count));
	FOREACH_POINTER(const char*,s,ATTR(entries),ATTR(entries_count),
		FLATTEN_STRING(s);
	);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ftdb_func_entry,funcs,ATTR(funcs_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ftdb_funcdecl_entry,funcdecls,ATTR(funcdecls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ftdb_unresolvedfunc_entry,unresolvedfuncs,ATTR(unresolvedfuncs_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ftdb_global_entry,globals,ATTR(globals_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ftdb_type_entry,types,ATTR(types_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(ftdb_fops_var_entry,fops.vars,ATTR(fops.vars_count));
	AGGREGATE_FLATTEN_STRUCT2_ITER(stringRefMap_node,sourcemap.rb_node);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,sourceindex_table,ATTR(sourceindex_table_count));
	FOREACH_POINTER(const char*,s,ATTR(sourceindex_table),ATTR(sourceindex_table_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRUCT2_ITER(stringRefMap_node,modulemap.rb_node);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,moduleindex_table,ATTR(moduleindex_table_count));
	FOREACH_POINTER(const char*,s,ATTR(moduleindex_table),ATTR(moduleindex_table_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(macroinfo_item,macroinfo,ATTR(macroinfo_count));
	AGGREGATE_FLATTEN_STRUCT2_ITER(stringRefMap_node,macromap.rb_node);
	AGGREGATE_FLATTEN_STRING2(version);
	AGGREGATE_FLATTEN_STRING2(module);
	AGGREGATE_FLATTEN_STRING2(directory);
	AGGREGATE_FLATTEN_STRING2(release);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_type_entryMap,refmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_type_entryMap,hrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_func_entryMap,frefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryMap,fhrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryListMap,fnrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_funcdecl_entryMap,fdrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_funcdecl_entryMap,fdhrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_func_entryListMap,fdnrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_ulong_global_entryMap,grefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryMap,ghrefmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT_TYPE2_ITER(ftdb_stringRef_global_entryListMap,gnrefmap.rb_node);

	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_calls_no_asm);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_calls_no_known);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_calls_no_known_no_asm);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_func_calls);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_func_refs);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_funrefs_no_asm);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_funrefs_no_known);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,funcs_tree_funrefs_no_known_no_asm);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,globs_tree_globalrefs);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,types_tree_refs);
	AGGREGATE_FLATTEN_STRUCT2_ITER(matrix_data,types_tree_usedrefs);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(func_map_entry,static_funcs_map,ATTR(static_funcs_map_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(init_data_entry,init_data,ATTR(init_data_count));
	AGGREGATE_FLATTEN_STRUCT2_ITER(known_data_entry,known_data);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(BAS_item,BAS_data,ATTR(BAS_data_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb_func_entry,
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_STRING2(__namespace);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,fids,ATTR(fids_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,mids,ATTR(mids_count));
	AGGREGATE_FLATTEN_STRING2(firstNonDeclStmt);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,isinline,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,istemplate,1);
	AGGREGATE_FLATTEN_STRING2(classOuterFn);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,ismember,1);
	AGGREGATE_FLATTEN_STRING2(__class);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,classid,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,attributes,ATTR(attributes_count));
	FOREACH_POINTER(const char*,s,ATTR(attributes),ATTR(attributes_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRING2(hash);
	AGGREGATE_FLATTEN_STRING2(cshash);
	AGGREGATE_FLATTEN_STRING2(template_parameters);
	AGGREGATE_FLATTEN_STRING2(body);
	AGGREGATE_FLATTEN_STRING2(unpreprocessed_body);
	AGGREGATE_FLATTEN_STRING2(declbody);
	AGGREGATE_FLATTEN_STRING2(signature);
	AGGREGATE_FLATTEN_STRING2(declhash);
	AGGREGATE_FLATTEN_STRING2(location);
	AGGREGATE_FLATTEN_STRING2(abs_location);
	AGGREGATE_FLATTEN_STRING2(start_loc);
	AGGREGATE_FLATTEN_STRING2(end_loc);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,integer_literals,ATTR(integer_literals_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,character_literals,ATTR(character_literals_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(double,floating_literals,ATTR(floating_literals_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,string_literals,ATTR(string_literals_count));
	FOREACH_POINTER(const char*,s,ATTR(string_literals),ATTR(string_literals_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(taint_data,taint,ATTR(taint_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,calls,ATTR(calls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(call_info,call_info,ATTR(calls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(callref_info,callrefs,ATTR(calls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(refcall,refcalls,ATTR(refcalls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(call_info,refcall_info,ATTR(refcalls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(callref_info,refcallrefs,ATTR(refcalls_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(switch_info,switches,ATTR(switches_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(csitem,csmap,ATTR(csmap_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(local_info,locals,ATTR(locals_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(deref_info,derefs,ATTR(derefs_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(if_info,ifs,ATTR(ifs_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(asm_info,asms,ATTR(asms_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,globalrefs,ATTR(globalrefs_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(globalref_data,globalrefInfo,ATTR(globalrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,funrefs,ATTR(funrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,refs,ATTR(refs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,decls,ATTR(decls_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,types,ATTR(types_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb_funcdecl_entry,
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_STRING2(__namespace);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,istemplate,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,ismember,1);
	AGGREGATE_FLATTEN_STRING2(__class);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,classid,1);
	AGGREGATE_FLATTEN_STRING2(template_parameters);
	AGGREGATE_FLATTEN_STRING2(decl);
	AGGREGATE_FLATTEN_STRING2(signature);
	AGGREGATE_FLATTEN_STRING2(declhash);
	AGGREGATE_FLATTEN_STRING2(location);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,types,ATTR(types_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb_unresolvedfunc_entry,
	AGGREGATE_FLATTEN_STRING2(name);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb_global_entry,
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_STRING2(hash);
	AGGREGATE_FLATTEN_STRING2(def);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,globalrefs,ATTR(globalrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,refs,ATTR(refs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,funrefs,ATTR(funrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,decls,ATTR(decls_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,mids,ATTR(mids_count));
	AGGREGATE_FLATTEN_STRING2(file);
	AGGREGATE_FLATTEN_STRING2(location);
	AGGREGATE_FLATTEN_STRING2(init);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,integer_literals,ATTR(integer_literals_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,character_literals,ATTR(character_literals_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(double,floating_literals,ATTR(floating_literals_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,string_literals,ATTR(string_literals_count));
	FOREACH_POINTER(const char*,s,ATTR(string_literals),ATTR(string_literals_count),
		FLATTEN_STRING(s);
	);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(bitfield,
	/* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb_type_entry,
	AGGREGATE_FLATTEN_STRING2(hash);
	AGGREGATE_FLATTEN_STRING2(class_name);
	AGGREGATE_FLATTEN_STRING2(qualifiers);
	AGGREGATE_FLATTEN_STRING2(str);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,refs,ATTR(refs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,usedrefs,ATTR(usedrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,decls,ATTR(decls_count));
	AGGREGATE_FLATTEN_STRING2(def);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,memberoffsets,ATTR(memberoffsets_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,attrrefs,ATTR(attrrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,attrnum,1);
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,isdependent,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,globalrefs,ATTR(globalrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int64_t,enumvalues,ATTR(enumvalues_count));
	AGGREGATE_FLATTEN_STRING2(outerfn);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,outerfnid,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,isimplicit,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,isunion,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,refnames,ATTR(refnames_count));
	FOREACH_POINTER(const char*,s,ATTR(refnames),ATTR(refnames_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRING2(location);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(int,isvariadic,1);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,funrefs,ATTR(funrefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,useddef,ATTR(useddef_count));
	FOREACH_POINTER(const char*,s,ATTR(useddef),ATTR(useddef_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRING2(defhead);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,identifiers,ATTR(identifiers_count));
	FOREACH_POINTER(const char*,s,ATTR(identifiers),ATTR(identifiers_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,methods,ATTR(methods_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(bitfield,bitfields,ATTR(bitfields_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(fops_member_info,
	/* No recipes needed */
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ftdb_fops_var_entry,
	AGGREGATE_FLATTEN_STRING2(type);
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(fops_member_info,members,ATTR(members_count));
	AGGREGATE_FLATTEN_STRING2(location);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(macroinfo_item,
	AGGREGATE_FLATTEN_STRING2(name);
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(macro_occurence,occurences,ATTR(occurences_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(macro_occurence,
	AGGREGATE_FLATTEN_STRING2(loc);
	AGGREGATE_FLATTEN_STRING2(expanded);
);

/* TODO: memory leaks */
static void libftdb_create_ftdb_func_entry(PyObject *self, PyObject* func_entry, struct ftdb_func_entry* new_entry) {

	new_entry->name = FTDB_ENTRY_STRING(func_entry,name);
	new_entry->__namespace = FTDB_ENTRY_STRING_OPTIONAL(func_entry,namespace);
	new_entry->id = FTDB_ENTRY_ULONG(func_entry,id);
	new_entry->fid = FTDB_ENTRY_ULONG(func_entry,fid);
	new_entry->fids_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,fids);
	new_entry->fids = FTDB_ENTRY_ULONG_ARRAY(func_entry,fids);
	new_entry->mids_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(func_entry,mids);
	new_entry->mids = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(func_entry,mids);
	new_entry->nargs = FTDB_ENTRY_ULONG(func_entry,nargs);
	new_entry->isvariadic = FTDB_ENTRY_BOOL(func_entry,variadic);
	new_entry->firstNonDeclStmt = FTDB_ENTRY_STRING(func_entry,firstNonDeclStmt);
	new_entry->isinline = FTDB_ENTRY_BOOL_OPTIONAL(func_entry,inline);
	new_entry->istemplate = FTDB_ENTRY_BOOL_OPTIONAL(func_entry,template);
	new_entry->classOuterFn = FTDB_ENTRY_STRING_OPTIONAL(func_entry,classOuterFn);
	new_entry->linkage = FTDB_ENTRY_ENUM_FROM_STRING(func_entry,linkage,functionLinkage);
	new_entry->ismember = FTDB_ENTRY_BOOL_OPTIONAL(func_entry,member);
	new_entry->__class = FTDB_ENTRY_STRING_OPTIONAL(func_entry,class);
	new_entry->classid = FTDB_ENTRY_ULONG_OPTIONAL(func_entry,classid);
	new_entry->attributes_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,attributes);
	new_entry->attributes = FTDB_ENTRY_STRING_ARRAY(func_entry,attributes);
	new_entry->hash = FTDB_ENTRY_STRING(func_entry,hash);
	new_entry->cshash = FTDB_ENTRY_STRING(func_entry,cshash);
	new_entry->template_parameters = FTDB_ENTRY_STRING_OPTIONAL(func_entry,template_parameters);
	new_entry->body = FTDB_ENTRY_STRING(func_entry,body);
	new_entry->unpreprocessed_body = FTDB_ENTRY_STRING(func_entry,unpreprocessed_body);
	new_entry->declbody = FTDB_ENTRY_STRING(func_entry,declbody);
	new_entry->signature = FTDB_ENTRY_STRING(func_entry,signature);
	new_entry->declhash = FTDB_ENTRY_STRING(func_entry,declhash);
	new_entry->location = FTDB_ENTRY_STRING(func_entry,location);
	new_entry->abs_location = FTDB_ENTRY_STRING_OPTIONAL(func_entry,abs_location);
	new_entry->start_loc = FTDB_ENTRY_STRING(func_entry,start_loc);
	new_entry->end_loc = FTDB_ENTRY_STRING(func_entry,end_loc);
	new_entry->refcount = FTDB_ENTRY_ULONG(func_entry,refcount);

	PyObject* key_literals = PyUnicode_FromString("literals");
	PyObject* literals = PyDict_GetItem(func_entry,key_literals);
	Py_DecRef(key_literals);
	new_entry->integer_literals = FTDB_ENTRY_INT64_ARRAY(literals,integer);
	new_entry->integer_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,integer);
	new_entry->character_literals = FTDB_ENTRY_UINT_ARRAY(literals,character);
	new_entry->character_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,character);
	new_entry->floating_literals = FTDB_ENTRY_FLOAT_ARRAY(literals,floating);
	new_entry->floating_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,floating);
	new_entry->string_literals = FTDB_ENTRY_STRING_ARRAY(literals,string);
	new_entry->string_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,string);
	new_entry->declcount = FTDB_ENTRY_ULONG(func_entry,declcount);

	PyObject* key_taint = PyUnicode_FromString("taint");
	PyObject* taint = PyDict_GetItem(func_entry,key_taint);
	Py_DecRef(key_taint);
	PyObject* taint_keys = PyDict_Keys(taint);
	new_entry->taint_count = PyDict_Size(taint);
	new_entry->taint = calloc(new_entry->taint_count,sizeof(struct taint_data));
	for (Py_ssize_t i=0; i<PyList_Size(taint_keys); ++i) {
		PyObject* py_int_key = PyLong_FromString((char*)PyString_get_c_str(PyList_GetItem(taint_keys,i)),0,10);
		if (py_int_key) {
			PyObject* taint_tuple_list = PyDict_GetItem(taint, PyList_GetItem(taint_keys,i));
			unsigned long integer_key = PyLong_AsUnsignedLong(py_int_key);
			struct taint_data* taint_data = &new_entry->taint[integer_key];
			taint_data->taint_list_count = PyList_Size(taint_tuple_list);
			taint_data->taint_list = calloc(taint_data->taint_list_count,sizeof(struct taint_element));
			for (Py_ssize_t j=0; j<PyList_Size(taint_tuple_list); ++j) {
				PyObject* taint_tuple_as_list = PyList_GetItem(taint_tuple_list,j);
				taint_data->taint_list[j].taint_level = PyLong_AsUnsignedLong(PyList_GetItem(taint_tuple_as_list,0));
				taint_data->taint_list[j].varid = PyLong_AsUnsignedLong(PyList_GetItem(taint_tuple_as_list,1));
			}
		}
	}
	Py_DecRef(taint_keys);

	new_entry->calls = FTDB_ENTRY_ULONG_ARRAY(func_entry,calls);
	new_entry->calls_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,calls);

	PyObject* key_call_info = PyUnicode_FromString("call_info");
	PyObject* call_info = PyDict_GetItem(func_entry,key_call_info);
	Py_DecRef(key_call_info);
	new_entry->call_info = calloc(new_entry->calls_count,sizeof(struct call_info));
	for (Py_ssize_t i=0; i<PyList_Size(call_info); ++i) {
		PyObject* py_single_call_info = PyList_GetItem(call_info,i);
		struct call_info* single_call_info = &new_entry->call_info[i];
		single_call_info->start = FTDB_ENTRY_STRING(py_single_call_info,start);
		single_call_info->end = FTDB_ENTRY_STRING(py_single_call_info,end);
		single_call_info->ord = FTDB_ENTRY_ULONG(py_single_call_info,ord);
		single_call_info->expr = FTDB_ENTRY_STRING(py_single_call_info,expr);
		single_call_info->loc = FTDB_ENTRY_STRING(py_single_call_info,loc);
		single_call_info->args = FTDB_ENTRY_ULONG_ARRAY(py_single_call_info,args);
		single_call_info->args_count = FTDB_ENTRY_ARRAY_SIZE(py_single_call_info,args);
	}

	PyObject* key_callrefs = PyUnicode_FromString("callrefs");
	PyObject* callrefs = PyDict_GetItem(func_entry,key_callrefs);
	Py_DecRef(key_callrefs);
	new_entry->callrefs = calloc(new_entry->calls_count,sizeof(struct callref_info));
	for (Py_ssize_t i=0; i<PyList_Size(callrefs); ++i) {
		PyObject* py_single_callref = PyList_GetItem(callrefs,i);
		struct callref_info* single_callref = &new_entry->callrefs[i];
		single_callref->callarg_count = PyList_Size(py_single_callref);
		single_callref->callarg = calloc(single_callref->callarg_count,sizeof(struct callref_data));
		for (unsigned long j=0; j<single_callref->callarg_count; ++j) {
			PyObject* py_callref_data = PyList_GetItem(py_single_callref,j);
			single_callref->callarg[j].type = FTDB_ENTRY_ENUM_FROM_STRING(py_callref_data,type,exprType);
			single_callref->callarg[j].pos = FTDB_ENTRY_ULONG(py_callref_data,pos);
			if (single_callref->callarg[j].type==EXPR_STRINGLITERAL) {
				single_callref->callarg[j].string_literal = FTDB_ENTRY_STRING_OPTIONAL(py_callref_data,id);
			}
			else if (single_callref->callarg[j].type==EXPR_FLOATLITERAL) {
				single_callref->callarg[j].floating_literal = FTDB_ENTRY_FLOAT_OPTIONAL(py_callref_data,id);
			}
			else if (single_callref->callarg[j].type==EXPR_INTEGERLITERAL) {
				single_callref->callarg[j].integer_literal = FTDB_ENTRY_INT64_OPTIONAL(py_callref_data,id);
			}
			else if (single_callref->callarg[j].type==EXPR_CHARLITERAL) {
				single_callref->callarg[j].character_literal = FTDB_ENTRY_INT_OPTIONAL(py_callref_data,id);
			}
			else {
				single_callref->callarg[j].id = FTDB_ENTRY_ULONG_OPTIONAL(py_callref_data,id);
			}
		}
	}


	PyObject* key_refcalls = PyUnicode_FromString("refcalls");
	PyObject* refcalls = PyDict_GetItem(func_entry,key_refcalls);
	Py_DecRef(key_refcalls);
	new_entry->refcalls_count = PyList_Size(refcalls);
	new_entry->refcalls = calloc(new_entry->refcalls_count,sizeof(struct refcall));
	for (Py_ssize_t i=0; i<new_entry->refcalls_count; ++i) {
		PyObject* py_refcall_tuple = PyList_GetItem(refcalls,i);
		new_entry->refcalls[i].fid = PyLong_AsLong(PyList_GetItem(py_refcall_tuple,0));
		if (PyList_Size(py_refcall_tuple)>1) {
			new_entry->refcalls[i].ismembercall = 1;
			new_entry->refcalls[i].cid = PyLong_AsLong(PyList_GetItem(py_refcall_tuple,1));
			new_entry->refcalls[i].field_index = PyLong_AsLong(PyList_GetItem(py_refcall_tuple,2));
		}
	}

	PyObject* key_refcall_info = PyUnicode_FromString("refcall_info");
	PyObject* refcall_info = PyDict_GetItem(func_entry,key_refcall_info);
	Py_DecRef(key_refcall_info);
	new_entry->refcall_info = calloc(new_entry->refcalls_count,sizeof(struct call_info));
	for (Py_ssize_t i=0; i<PyList_Size(refcall_info); ++i) {
		PyObject* py_single_refcall_info = PyList_GetItem(refcall_info,i);
		struct call_info* single_refcall_info = &new_entry->refcall_info[i];
		single_refcall_info->start = FTDB_ENTRY_STRING(py_single_refcall_info,start);
		single_refcall_info->end = FTDB_ENTRY_STRING(py_single_refcall_info,end);
		single_refcall_info->ord = FTDB_ENTRY_ULONG(py_single_refcall_info,ord);
		single_refcall_info->expr = FTDB_ENTRY_STRING(py_single_refcall_info,expr);
		single_refcall_info->loc = FTDB_ENTRY_STRING(py_single_refcall_info,loc);
		single_refcall_info->args = FTDB_ENTRY_ULONG_ARRAY(py_single_refcall_info,args);
		single_refcall_info->args_count = FTDB_ENTRY_ARRAY_SIZE(py_single_refcall_info,args);
	}
	PyObject* key_refcallrefs = PyUnicode_FromString("refcallrefs");
	PyObject* refcallrefs = PyDict_GetItem(func_entry,key_refcallrefs);
	Py_DecRef(key_refcallrefs);
	new_entry->refcallrefs = calloc(new_entry->refcalls_count,sizeof(struct callref_info));
	for (Py_ssize_t i=0; i<PyList_Size(refcallrefs); ++i) {
		PyObject* py_single_refcallref = PyList_GetItem(refcallrefs,i);
		struct callref_info* single_refcallref = &new_entry->refcallrefs[i];
		single_refcallref->callarg_count = PyList_Size(py_single_refcallref);
		single_refcallref->callarg = calloc(single_refcallref->callarg_count,sizeof(struct callref_data));
		for (unsigned long j=0; j<single_refcallref->callarg_count; ++j) {
			PyObject* py_refcallref_data = PyList_GetItem(py_single_refcallref,j);
			single_refcallref->callarg[j].type = FTDB_ENTRY_ENUM_FROM_STRING(py_refcallref_data,type,exprType);
			single_refcallref->callarg[j].pos = FTDB_ENTRY_ULONG(py_refcallref_data,pos);
			if (single_refcallref->callarg[j].type==EXPR_STRINGLITERAL) {
				single_refcallref->callarg[j].string_literal = FTDB_ENTRY_STRING_OPTIONAL(py_refcallref_data,id);
			}
			else if (single_refcallref->callarg[j].type==EXPR_FLOATLITERAL) {
				single_refcallref->callarg[j].floating_literal = FTDB_ENTRY_FLOAT_OPTIONAL(py_refcallref_data,id);
			}
			else if (single_refcallref->callarg[j].type==EXPR_INTEGERLITERAL) {
				single_refcallref->callarg[j].integer_literal = FTDB_ENTRY_INT64_OPTIONAL(py_refcallref_data,id);
			}
			else if (single_refcallref->callarg[j].type==EXPR_CHARLITERAL) {
				single_refcallref->callarg[j].character_literal = FTDB_ENTRY_INT_OPTIONAL(py_refcallref_data,id);
			}
			else {
				single_refcallref->callarg[j].id = FTDB_ENTRY_ULONG_OPTIONAL(py_refcallref_data,id);
			}
		}
	}

	PyObject* key_switches = PyUnicode_FromString("switches");
	PyObject* switches = PyDict_GetItem(func_entry,key_switches);
	Py_DecRef(key_switches);
	new_entry->switches_count = PyList_Size(switches);
	new_entry->switches = calloc(new_entry->switches_count,sizeof(struct switch_info));
	for (Py_ssize_t i=0; i<PyList_Size(switches); ++i) {
		PyObject* py_switch_info = PyList_GetItem(switches,i);
		struct switch_info* switch_info = &new_entry->switches[i];
		switch_info->condition = FTDB_ENTRY_STRING(py_switch_info,condition);
		switch_info->cases_count = FTDB_ENTRY_ARRAY_SIZE(py_switch_info,cases);
		switch_info->cases = calloc(switch_info->cases_count,sizeof(struct case_info));
		PyObject* key_cases = PyUnicode_FromString("cases");
		PyObject* cases = PyDict_GetItem(py_switch_info,key_cases);
		Py_DecRef(key_cases);
		for (Py_ssize_t j=0; j<PyList_Size(cases); ++j) {
			PyObject* py_case_info = PyList_GetItem(cases,j);
			struct case_info* case_info = &switch_info->cases[j];
			case_info->lhs.expr_value = PyLong_AsLong(PyList_GetItem(py_case_info,0));
			case_info->lhs.enum_code = PyString_get_c_str(PyList_GetItem(py_case_info,1));
			case_info->lhs.macro_code = PyString_get_c_str(PyList_GetItem(py_case_info,2));
			case_info->lhs.raw_code = PyString_get_c_str(PyList_GetItem(py_case_info,3));
			if (PyList_Size(py_case_info)>4) {
				case_info->rhs = calloc(1,sizeof(struct case_data));
				case_info->rhs->expr_value = PyLong_AsLong(PyList_GetItem(py_case_info,4));
				case_info->rhs->enum_code = PyString_get_c_str(PyList_GetItem(py_case_info,5));
				case_info->rhs->macro_code = PyString_get_c_str(PyList_GetItem(py_case_info,6));
				case_info->rhs->raw_code = PyString_get_c_str(PyList_GetItem(py_case_info,7));
			}
		}
	}

	PyObject* key_csmap = PyUnicode_FromString("csmap");
	PyObject* csmap = PyDict_GetItem(func_entry,key_csmap);
	Py_DecRef(key_csmap);
	new_entry->csmap_count = PyList_Size(csmap);
	new_entry->csmap = calloc(new_entry->csmap_count,sizeof(struct csitem));
	for (Py_ssize_t i=0; i<PyList_Size(csmap); ++i) {
		PyObject* py_csitem = PyList_GetItem(csmap,i);
		struct csitem* csitem = &new_entry->csmap[i];
		csitem->pid = FTDB_ENTRY_INT64(py_csitem,pid);
		csitem->id = FTDB_ENTRY_ULONG(py_csitem,id);
		csitem->cf = FTDB_ENTRY_STRING(py_csitem,cf);
	}

	PyObject* key_locals = PyUnicode_FromString("locals");
	PyObject* locals = PyDict_GetItem(func_entry,key_locals);
	Py_DecRef(key_locals);
	new_entry->locals_count = PyList_Size(locals);
	new_entry->locals = calloc(new_entry->locals_count,sizeof(struct local_info));
	for (Py_ssize_t i=0; i<PyList_Size(locals); ++i) {
		PyObject* py_local_info = PyList_GetItem(locals,i);
		struct local_info* local_info = &new_entry->locals[i];
		local_info->id = FTDB_ENTRY_ULONG(py_local_info,id);
		local_info->name = FTDB_ENTRY_STRING(py_local_info,name);
		local_info->isparm = FTDB_ENTRY_BOOL(py_local_info,parm);
		local_info->type = FTDB_ENTRY_ULONG(py_local_info,type);
		local_info->isstatic = FTDB_ENTRY_BOOL(py_local_info,static);
		local_info->isused = FTDB_ENTRY_BOOL(py_local_info,used);
		local_info->location = FTDB_ENTRY_STRING(py_local_info,location);
		local_info->csid = FTDB_ENTRY_INT64_OPTIONAL(py_local_info,csid);
	}

	PyObject* key_derefs = PyUnicode_FromString("derefs");
	PyObject* derefs = PyDict_GetItem(func_entry,key_derefs);
	Py_DecRef(key_derefs);
	new_entry->derefs_count = PyList_Size(derefs);
	new_entry->derefs = calloc(new_entry->derefs_count,sizeof(struct deref_info));
	for (Py_ssize_t i=0; i<PyList_Size(derefs); ++i) {
		PyObject* py_deref_info = PyList_GetItem(derefs,i);
		struct deref_info* deref_info = &new_entry->derefs[i];
		deref_info->kind = FTDB_ENTRY_ENUM_FROM_STRING(py_deref_info,kind,exprType);
		deref_info->offset = FTDB_ENTRY_INT64_OPTIONAL(py_deref_info,offset);
		deref_info->basecnt = FTDB_ENTRY_ULONG_OPTIONAL(py_deref_info,basecnt);
		deref_info->expr = FTDB_ENTRY_STRING(py_deref_info,expr);
		deref_info->csid = FTDB_ENTRY_INT64(py_deref_info,csid);
		deref_info->ord_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info,ord);
		deref_info->ord = FTDB_ENTRY_ULONG_ARRAY(py_deref_info,ord);
		if ((deref_info->kind==EXPR_MEMBER)||(deref_info->kind==EXPR_OFFSETOF)) {
			deref_info->member_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info,member);
			deref_info->member = FTDB_ENTRY_INT64_ARRAY(py_deref_info,member);
			deref_info->type_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info,type);
			deref_info->type = FTDB_ENTRY_ULONG_ARRAY(py_deref_info,type);
		}
		if (deref_info->kind==EXPR_MEMBER) {
			deref_info->access_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info,access);
			deref_info->access = FTDB_ENTRY_ULONG_ARRAY(py_deref_info,access);
			deref_info->shift_count = FTDB_ENTRY_ARRAY_SIZE(py_deref_info,shift);
			deref_info->shift = FTDB_ENTRY_INT64_ARRAY(py_deref_info,shift);
			PyObject* key_mcall = PyUnicode_FromString("mcall");
			if (PyDict_Contains(py_deref_info,key_mcall)) {
				PyObject* mcall = PyDict_GetItem(py_deref_info,key_mcall);
				deref_info->mcall_count = PyList_Size(mcall);
				deref_info->mcall = calloc(deref_info->mcall_count,sizeof(int64_t));
				for (unsigned long j=0; j<deref_info->mcall_count; ++j) {
					deref_info->mcall[j] = PyLong_AsLong(PyList_GetItem(mcall,j));
				}
			}
			Py_DecRef(key_mcall);
		}
		PyObject* key_offsetrefs = PyUnicode_FromString("offsetrefs");
		PyObject* offsetrefs = PyDict_GetItem(py_deref_info,key_offsetrefs);
		Py_DecRef(key_offsetrefs);
		deref_info->offsetrefs_count = PyList_Size(offsetrefs);
		deref_info->offsetrefs = calloc(deref_info->offsetrefs_count,sizeof(struct offsetref_info));
		for (unsigned long j=0; j<deref_info->offsetrefs_count; ++j) {
			PyObject* py_offsetref_info = PyList_GetItem(offsetrefs,j);
			struct offsetref_info* offsetref_info = &deref_info->offsetrefs[j];
			offsetref_info->kind = FTDB_ENTRY_ENUM_FROM_STRING(py_offsetref_info,kind,exprType);
			if (offsetref_info->kind==EXPR_STRING) {
				offsetref_info->id_string = FTDB_ENTRY_STRING(py_offsetref_info,id);
			}
			else if (offsetref_info->kind==EXPR_FLOATING) {
				offsetref_info->id_float = FTDB_ENTRY_FLOAT(py_offsetref_info,id);
			}
			else {
				offsetref_info->id_integer = FTDB_ENTRY_INT64(py_offsetref_info,id);
			}
			offsetref_info->mi = FTDB_ENTRY_ULONG_OPTIONAL(py_offsetref_info,mi);
			offsetref_info->di = FTDB_ENTRY_ULONG_OPTIONAL(py_offsetref_info,di);
			offsetref_info->cast = FTDB_ENTRY_ULONG_OPTIONAL(py_offsetref_info,cast);
		}
	}
	PyObject* key_ifs = PyUnicode_FromString("ifs");
	PyObject* ifs = PyDict_GetItem(func_entry,key_ifs);
	Py_DecRef(key_ifs);
	new_entry->ifs_count = PyList_Size(ifs);
	new_entry->ifs = calloc(new_entry->ifs_count,sizeof(struct if_info));
	for (Py_ssize_t i=0; i<PyList_Size(ifs); ++i) {
		PyObject* py_if_info = PyList_GetItem(ifs,i);
		struct if_info* if_info = &new_entry->ifs[i];
		if_info->csid = FTDB_ENTRY_INT64(py_if_info,csid);
		PyObject* key_refs = PyUnicode_FromString("refs");
		PyObject* if_refs = PyDict_GetItem(py_if_info,key_refs);
		Py_DecRef(key_refs);
		if_info->refs_count = PyList_Size(if_refs);
		if_info->refs = calloc(if_info->refs_count,sizeof(struct ifref_info));
		for (unsigned long j=0; j<if_info->refs_count; ++j) {
			PyObject* py_ifref_info = PyList_GetItem(if_refs,j);
			struct ifref_info* ifref_info = &if_info->refs[j];
			ifref_info->type = FTDB_ENTRY_ENUM_FROM_STRING(py_ifref_info,type,exprType);
			if (ifref_info->type==EXPR_STRINGLITERAL) {
				ifref_info->string_literal = FTDB_ENTRY_STRING_OPTIONAL(py_ifref_info,id);
			}
			else if (ifref_info->type==EXPR_FLOATLITERAL) {
				ifref_info->floating_literal = FTDB_ENTRY_FLOAT_OPTIONAL(py_ifref_info,id);
			}
			else if (ifref_info->type==EXPR_INTEGERLITERAL) {
				ifref_info->integer_literal = FTDB_ENTRY_INT64_OPTIONAL(py_ifref_info,id);
			}
			else if (ifref_info->type==EXPR_CHARLITERAL) {
				ifref_info->character_literal = FTDB_ENTRY_INT_OPTIONAL(py_ifref_info,id);
			}
			else {
				ifref_info->id = FTDB_ENTRY_ULONG_OPTIONAL(py_ifref_info,id);
			}

		}
	}

	PyObject* key_asm = PyUnicode_FromString("asm");
	PyObject* asms = PyDict_GetItem(func_entry,key_asm);
	Py_DecRef(key_asm);
	new_entry->asms_count = PyList_Size(asms);
	new_entry->asms = calloc(new_entry->asms_count,sizeof(struct asm_info));
	for (Py_ssize_t i=0; i<PyList_Size(asms); ++i) {
		PyObject* py_asm_info = PyList_GetItem(asms,i);
		struct asm_info* asm_info = &new_entry->asms[i];
		asm_info->csid = FTDB_ENTRY_INT64(py_asm_info,csid);
		asm_info->str = FTDB_ENTRY_STRING(py_asm_info,str);
	}

	new_entry->globalrefs_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,globalrefs);
	new_entry->globalrefs = FTDB_ENTRY_ULONG_ARRAY(func_entry,globalrefs);

	PyObject* key_globalrefInfo = PyUnicode_FromString("globalrefInfo");
	PyObject* globalrefInfo = PyDict_GetItem(func_entry,key_globalrefInfo);
	Py_DecRef(key_globalrefInfo);
	new_entry->globalrefInfo = calloc(new_entry->globalrefs_count,sizeof(struct globalref_data));
	for (Py_ssize_t i=0; i<PyList_Size(globalrefInfo); ++i) {
		PyObject* py_globalref_data = PyList_GetItem(globalrefInfo,i);
		struct globalref_data* globalref_data = &new_entry->globalrefInfo[i];
		globalref_data->refs_count = PyList_Size(py_globalref_data);
		globalref_data->refs = calloc(globalref_data->refs_count,sizeof(struct globalref_info));
		for (Py_ssize_t j=0; j<PyList_Size(py_globalref_data); ++j) {
			PyObject* py_globalref_info = PyList_GetItem(py_globalref_data,j);
			struct globalref_info* globalref_info = &globalref_data->refs[j];
			globalref_info->start = FTDB_ENTRY_STRING(py_globalref_info,start);
			globalref_info->end = FTDB_ENTRY_STRING(py_globalref_info,end);
		}
	}

	new_entry->funrefs_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,funrefs);
	new_entry->funrefs = FTDB_ENTRY_ULONG_ARRAY(func_entry,funrefs);
	new_entry->refs_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,refs);
	new_entry->refs = FTDB_ENTRY_ULONG_ARRAY(func_entry,refs);
	new_entry->decls_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,decls);
	new_entry->decls = FTDB_ENTRY_ULONG_ARRAY(func_entry,decls);
	new_entry->types_count = FTDB_ENTRY_ARRAY_SIZE(func_entry,types);
	new_entry->types = FTDB_ENTRY_ULONG_ARRAY(func_entry,types);
}

static void libftdb_create_ftdb_funcdecl_entry(PyObject *self, PyObject* funcdecl_entry,struct ftdb_funcdecl_entry* new_entry) {

	new_entry->name = FTDB_ENTRY_STRING(funcdecl_entry,name);
	new_entry->__namespace = FTDB_ENTRY_STRING_OPTIONAL(funcdecl_entry,namespace);
	new_entry->id = FTDB_ENTRY_ULONG(funcdecl_entry,id);
	new_entry->fid = FTDB_ENTRY_ULONG(funcdecl_entry,fid);
	new_entry->nargs = FTDB_ENTRY_ULONG(funcdecl_entry,nargs);
	new_entry->isvariadic = FTDB_ENTRY_BOOL(funcdecl_entry,variadic);
	new_entry->istemplate = FTDB_ENTRY_BOOL_OPTIONAL(funcdecl_entry,template);
	new_entry->linkage = FTDB_ENTRY_ENUM_FROM_STRING(funcdecl_entry,linkage,functionLinkage);
	new_entry->ismember = FTDB_ENTRY_BOOL_OPTIONAL(funcdecl_entry,member);
	new_entry->__class = FTDB_ENTRY_STRING_OPTIONAL(funcdecl_entry,class);
	new_entry->classid = FTDB_ENTRY_ULONG_OPTIONAL(funcdecl_entry,classid);
	new_entry->template_parameters = FTDB_ENTRY_STRING_OPTIONAL(funcdecl_entry,template_parameters);
	new_entry->decl = FTDB_ENTRY_STRING(funcdecl_entry,decl);
	new_entry->signature = FTDB_ENTRY_STRING(funcdecl_entry,signature);
	new_entry->declhash = FTDB_ENTRY_STRING(funcdecl_entry,declhash);
	new_entry->location = FTDB_ENTRY_STRING(funcdecl_entry,location);
	new_entry->refcount = FTDB_ENTRY_ULONG(funcdecl_entry,refcount);
	new_entry->types_count = FTDB_ENTRY_ARRAY_SIZE(funcdecl_entry,types);
	new_entry->types = FTDB_ENTRY_ULONG_ARRAY(funcdecl_entry,types);
}

static void libftdb_create_ftdb_global_entry(PyObject *self, PyObject* global_entry, struct ftdb_global_entry* new_entry) {

	new_entry->name = FTDB_ENTRY_STRING(global_entry,name);
	new_entry->hash = FTDB_ENTRY_STRING(global_entry,hash);
	new_entry->id = FTDB_ENTRY_ULONG(global_entry,id);
	new_entry->def = FTDB_ENTRY_STRING(global_entry,def);
	new_entry->globalrefs_count = FTDB_ENTRY_ARRAY_SIZE(global_entry,globalrefs);
	new_entry->globalrefs = FTDB_ENTRY_ULONG_ARRAY(global_entry,globalrefs);
	new_entry->refs_count = FTDB_ENTRY_ARRAY_SIZE(global_entry,refs);
	new_entry->refs = FTDB_ENTRY_ULONG_ARRAY(global_entry,refs);
	new_entry->funrefs_count = FTDB_ENTRY_ARRAY_SIZE(global_entry,funrefs);
	new_entry->funrefs = FTDB_ENTRY_ULONG_ARRAY(global_entry,funrefs);
	new_entry->decls_count = FTDB_ENTRY_ARRAY_SIZE(global_entry,decls);
	new_entry->decls = FTDB_ENTRY_ULONG_ARRAY(global_entry,decls);
	new_entry->fid = FTDB_ENTRY_ULONG(global_entry,fid);
	new_entry->mids_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(global_entry,mids);
	new_entry->mids = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(global_entry,mids);
	new_entry->file = FTDB_ENTRY_STRING(global_entry,file);
	new_entry->type = FTDB_ENTRY_ULONG(global_entry,type);
	new_entry->linkage = FTDB_ENTRY_ENUM_FROM_STRING(global_entry,linkage,functionLinkage);
	new_entry->location = FTDB_ENTRY_STRING(global_entry,location);
	new_entry->deftype = FTDB_ENTRY_ENUM_FROM_ULONG(global_entry,deftype,globalDefType);
	new_entry->hasinit = FTDB_ENTRY_INT(global_entry,hasinit);
	new_entry->init = FTDB_ENTRY_STRING(global_entry,init);

	PyObject* key_literals = PyUnicode_FromString("literals");
	PyObject* literals = PyDict_GetItem(global_entry,key_literals);
	Py_DecRef(key_literals);
	new_entry->integer_literals = FTDB_ENTRY_INT64_ARRAY(literals,integer);
	new_entry->integer_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,integer);
	new_entry->character_literals = FTDB_ENTRY_UINT_ARRAY(literals,character);
	new_entry->character_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,character);
	new_entry->floating_literals = FTDB_ENTRY_FLOAT_ARRAY(literals,floating);
	new_entry->floating_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,floating);
	new_entry->string_literals = FTDB_ENTRY_STRING_ARRAY(literals,string);
	new_entry->string_literals_count = FTDB_ENTRY_ARRAY_SIZE(literals,string);
}

static int bitfield_compare (const void * a, const void * b)
{
  if ( ((struct bitfield*)a)->index <  ((struct bitfield*)b)->index ) return -1;
  if ( ((struct bitfield*)a)->index >  ((struct bitfield*)b)->index ) return 1;

  return 0;
}

static void libftdb_create_ftdb_type_entry(PyObject *self, PyObject* type_entry, struct ftdb_type_entry* new_entry) {

	new_entry->id = FTDB_ENTRY_ULONG(type_entry,id);
	new_entry->fid = FTDB_ENTRY_ULONG(type_entry,fid);
	new_entry->hash = FTDB_ENTRY_STRING(type_entry,hash);
	new_entry->class_name = FTDB_ENTRY_STRING(type_entry,class);
	new_entry->__class = FTDB_ENTRY_ENUM_FROM_STRING(type_entry,class,TypeClass);
	new_entry->qualifiers = FTDB_ENTRY_STRING(type_entry,qualifiers);
	new_entry->size = FTDB_ENTRY_ULONG(type_entry,size);
	new_entry->str = FTDB_ENTRY_STRING(type_entry,str);
	new_entry->refcount = FTDB_ENTRY_ULONG(type_entry,refcount);
	new_entry->refs_count = FTDB_ENTRY_ARRAY_SIZE(type_entry,refs);
	new_entry->refs = FTDB_ENTRY_ULONG_ARRAY(type_entry,refs);
	new_entry->usedrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,usedrefs);
	new_entry->usedrefs = FTDB_ENTRY_INT64_ARRAY_OPTIONAL(type_entry,usedrefs);
	new_entry->decls_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,decls);
	new_entry->decls = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry,decls);
	new_entry->def = FTDB_ENTRY_STRING_OPTIONAL(type_entry,def);
	new_entry->memberoffsets_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,memberoffsets);
	new_entry->memberoffsets = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry,memberoffsets);
	new_entry->attrrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,attrrefs);
	new_entry->attrrefs = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry,attrrefs);
	new_entry->attrnum = FTDB_ENTRY_ULONG_OPTIONAL(type_entry,attrnum);
	new_entry->name = FTDB_ENTRY_STRING_OPTIONAL(type_entry,name);
	new_entry->isdependent = FTDB_ENTRY_BOOL_OPTIONAL(type_entry,dependent);
	new_entry->globalrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,globalrefs);
	new_entry->globalrefs = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry,globalrefs);
	new_entry->enumvalues_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,values);
	new_entry->enumvalues = FTDB_ENTRY_INT64_ARRAY_OPTIONAL(type_entry,values);
	new_entry->outerfn = FTDB_ENTRY_STRING_OPTIONAL(type_entry,outerfn);
	new_entry->outerfnid = FTDB_ENTRY_ULONG_OPTIONAL(type_entry,outerfnid);
	new_entry->isimplicit = FTDB_ENTRY_BOOL_OPTIONAL(type_entry,implicit);
	new_entry->isunion = FTDB_ENTRY_BOOL_OPTIONAL(type_entry,union);
	new_entry->refnames_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,refnames);
	new_entry->refnames = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(type_entry,refnames);
	new_entry->location = FTDB_ENTRY_STRING_OPTIONAL(type_entry,location);
	new_entry->isvariadic = FTDB_ENTRY_BOOL_OPTIONAL(type_entry,variadic);
	new_entry->funrefs_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,funrefs);
	new_entry->funrefs = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry,funrefs);
	new_entry->useddef_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,useddef);
	new_entry->useddef = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(type_entry,useddef);
	new_entry->defhead = FTDB_ENTRY_STRING_OPTIONAL(type_entry,defhead);
	new_entry->identifiers_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,identifiers);
	new_entry->identifiers = FTDB_ENTRY_STRING_ARRAY_OPTIONAL(type_entry,identifiers);
	new_entry->methods_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(type_entry,methods);
	new_entry->methods = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(type_entry,methods);

	PyObject* key_bitfields = PyUnicode_FromString("bitfields");
	if (PyDict_Contains(type_entry,key_bitfields)) {
		PyObject* bitfields = PyDict_GetItem(type_entry,key_bitfields);
		PyObject* bitfields_indexes = PyDict_Keys(bitfields);
		new_entry->bitfields_count = PyList_Size(bitfields_indexes);
		new_entry->bitfields = malloc(new_entry->bitfields_count*sizeof(struct bitfield));
		for (Py_ssize_t i=0; i<new_entry->bitfields_count; ++i) {
			PyObject* bf_index = PyList_GetItem(bitfields_indexes,i);
			PyObject* bf_int_index = PyLong_FromString((char*)PyString_get_c_str(bf_index),0,10);
			PyObject* bf_value = PyDict_GetItem(bitfields, bf_index);
			new_entry->bitfields[i].index = PyLong_AsUnsignedLong(bf_int_index);
			new_entry->bitfields[i].bitwidth = PyLong_AsUnsignedLong(bf_value);
		}
		Py_DecRef(bitfields_indexes);
		qsort(new_entry->bitfields,new_entry->bitfields_count,sizeof(struct bitfield),bitfield_compare);
	}
	Py_DecRef(key_bitfields);
}

static int member_info_compare (const void * a, const void * b)
{
  if ( ((struct fops_member_info*)a)->index <  ((struct fops_member_info*)b)->index ) return -1;
  if ( ((struct fops_member_info*)a)->index >  ((struct fops_member_info*)b)->index ) return 1;

  return 0;
}

static void libftdb_create_ftdb_fops_var_entry(PyObject *self, PyObject* fops_var_entry, struct ftdb_fops_var_entry* new_entry) {

	new_entry->type = FTDB_ENTRY_STRING(fops_var_entry,type);
	new_entry->name = FTDB_ENTRY_STRING(fops_var_entry,name);
	PyObject* key_members = PyUnicode_FromString("members");
	PyObject* members = PyDict_GetItem(fops_var_entry,key_members);
	PyObject* members_indexes = PyDict_Keys(members);
	new_entry->members_count = PyList_Size(members_indexes);
	new_entry->members = malloc(new_entry->members_count*sizeof(struct fops_member_info));
	for (Py_ssize_t i=0; i<new_entry->members_count; ++i) {
		PyObject* member_index = PyList_GetItem(members_indexes,i);
		const char* member_index_cstr = PyString_get_c_str(member_index);
		PyObject* member_int_index = PyLong_FromString(member_index_cstr,0,10);
		PYASSTR_DECREF(member_index_cstr);
		PyObject* member_fnid = PyDict_GetItem(members, member_index);
		new_entry->members[i].index = PyLong_AsUnsignedLong(member_int_index);
		new_entry->members[i].fnid = PyLong_AsUnsignedLong(member_fnid);
	}
	Py_DecRef(members_indexes);
	qsort(new_entry->members,new_entry->members_count,sizeof(struct fops_member_info),member_info_compare);
	Py_DecRef(key_members);
	new_entry->location = FTDB_ENTRY_STRING(fops_var_entry,location);
}

void fill_matrix_data_entry(PyObject* matrix_data_entry, struct matrix_data* new_entry) {

	for (Py_ssize_t i=0; i<PyList_Size(matrix_data_entry); ++i) {
		PyObject* entry = PyList_GetItem(matrix_data_entry,i);
		PyObject* key_name = PyUnicode_FromString("name");
		PyObject* name = PyDict_GetItem(entry,key_name);
		Py_DecRef(key_name);
		if (!strcmp(PyString_get_c_str(name),"data")) {
			new_entry->data_count = FTDB_ENTRY_ARRAY_SIZE(entry,data);
			new_entry->data = FTDB_ENTRY_ULONG_ARRAY(entry,data);
		}
		else if (!strcmp(PyString_get_c_str(name),"row_ind")) {
			new_entry->row_ind_count = FTDB_ENTRY_ARRAY_SIZE(entry,data);
			new_entry->row_ind = FTDB_ENTRY_ULONG_ARRAY(entry,data);
		}
		else if (!strcmp(PyString_get_c_str(name),"col_ind")) {
			new_entry->col_ind_count = FTDB_ENTRY_ARRAY_SIZE(entry,data);
			new_entry->col_ind = FTDB_ENTRY_ULONG_ARRAY(entry,data);
		}
		else if (!strcmp(PyString_get_c_str(name),"matrix_size")) {
			new_entry->matrix_size = FTDB_ENTRY_ULONG(entry,data);
		}
	}
}

void fill_func_map_entry_entry(PyObject* func_map_entry, struct func_map_entry* new_entry) {

	new_entry->id = FTDB_ENTRY_ULONG(func_map_entry,id);
	new_entry->fids_count = FTDB_ENTRY_ARRAY_SIZE(func_map_entry,fids);
	new_entry->fids = FTDB_ENTRY_ULONG_ARRAY(func_map_entry,fids);
}

void fill_size_dep_item_entry(PyObject* size_dep_item, struct size_dep_item* new_entry) {

	new_entry->id = FTDB_ENTRY_ULONG(size_dep_item,id);
	new_entry->add = FTDB_ENTRY_ULONG(size_dep_item,add);
}

void fill_init_data_item_entry(PyObject* data_item_entry, struct init_data_item* new_entry) {

	new_entry->id = FTDB_ENTRY_ULONG_OPTIONAL(data_item_entry,id);
	new_entry->name_count = FTDB_ENTRY_ARRAY_SIZE(data_item_entry,name);
	new_entry->name = FTDB_ENTRY_STRING_ARRAY(data_item_entry,name);
	new_entry->size = FTDB_ENTRY_ULONG_OPTIONAL(data_item_entry,size);
        new_entry->size_dep = FTDB_ENTRY_TYPE_OPTIONAL(data_item_entry,size_dep,size_dep_item);
	new_entry->nullterminated = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry,nullterminated);
	new_entry->tagged = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry,tagged);
	new_entry->fuzz = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry,fuzz);
	new_entry->pointer = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry,pointer);
        new_entry->min_value = FTDB_ENTRY_INT64_OPTIONAL(data_item_entry,min_value);
        new_entry->max_value = FTDB_ENTRY_INT64_OPTIONAL(data_item_entry,max_value);
        new_entry->value = FTDB_ENTRY_INT64_OPTIONAL(data_item_entry,value);
        new_entry->user_name = FTDB_ENTRY_STRING_OPTIONAL(data_item_entry,user_name);
}

void fill_init_data_entry_entry(PyObject* init_data_entry, struct init_data_entry* new_entry) {

	new_entry->name = FTDB_ENTRY_STRING(init_data_entry,name);
	new_entry->order_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(init_data_entry,order);
	new_entry->order = FTDB_ENTRY_ULONG_ARRAY_OPTIONAL(init_data_entry,order);
	new_entry->interface = FTDB_ENTRY_STRING(init_data_entry,interface);
	new_entry->items_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(init_data_entry,items);
	new_entry->items = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(init_data_entry,items,init_data_item,new_entry->items_count);
}

void fill_known_data_entry_entry(PyObject* known_data_entry, struct known_data_entry* new_entry) {

	new_entry->version = FTDB_ENTRY_STRING(known_data_entry,version);
	new_entry->func_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry,func_ids);
	new_entry->func_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry,func_ids);
	new_entry->builtin_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry,builtin_ids);
	new_entry->builtin_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry,builtin_ids);
	new_entry->asm_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry,asm_ids);
	new_entry->asm_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry,asm_ids);
	new_entry->lib_funcs_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry,lib_funcs);
	new_entry->lib_funcs = FTDB_ENTRY_STRING_ARRAY(known_data_entry,lib_funcs);
	new_entry->lib_funcs_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry,lib_funcs_ids);
	new_entry->lib_funcs_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry,lib_funcs_ids);
	new_entry->always_inc_funcs_ids_count = FTDB_ENTRY_ARRAY_SIZE(known_data_entry,always_inc_funcs_ids);
	new_entry->always_inc_funcs_ids = FTDB_ENTRY_ULONG_ARRAY(known_data_entry,always_inc_funcs_ids);
	new_entry->source_root = FTDB_ENTRY_STRING(known_data_entry,source_root);
}

void fill_BAS_item_entry(PyObject* BAS_item_entry, struct BAS_item* new_entry) {

	new_entry->loc = FTDB_ENTRY_STRING(BAS_item_entry,loc);
	new_entry->entries_count = FTDB_ENTRY_ARRAY_SIZE(BAS_item_entry,entries);
	new_entry->entries = FTDB_ENTRY_STRING_ARRAY(BAS_item_entry,entries);
}

static void destroy_ftdb(struct ftdb* ftdb) {

	// TODO
}

static inline void libftdb_create_macroinfo_entry(PyObject* py_macroinfo_item, struct macroinfo_item* macroinfo_item) {

	macroinfo_item->name = FTDB_ENTRY_STRING(py_macroinfo_item,name);
	macroinfo_item->occurences_count = FTDB_ENTRY_ARRAY_SIZE(py_macroinfo_item,occurences);
	macroinfo_item->occurences = calloc(macroinfo_item->occurences_count,sizeof(struct macro_occurence));
	PyObject* key_occurences = PyUnicode_FromString("occurences");
	PyObject* py_occurences = PyDict_GetItem(py_macroinfo_item,key_occurences);
	Py_DecRef(key_occurences);
	for (Py_ssize_t i=0; i<PyList_Size(py_occurences); ++i) {
		PyObject* py_occurence = PyList_GetItem(py_occurences,i);
		struct macro_occurence* occurence = &macroinfo_item->occurences[i];
		occurence->loc = FTDB_ENTRY_STRING(py_occurence,loc);
		occurence->expanded = FTDB_ENTRY_STRING(py_occurence,expanded);
		occurence->fid = FTDB_ENTRY_ULONG(py_occurence,fid);
	}
}

PyObject * libftdb_create_ftdb(PyObject *self, PyObject *args) {

	PyObject* dbJSON = PyTuple_GetItem(args,0);
	PyObject* dbfn = PyTuple_GetItem(args,1);
	int show_stats = 0;
	if (PyTuple_Size(args)>2) {
		PyObject* show_stats_arg = PyTuple_GetItem(args,2);
		if (show_stats_arg==Py_True) {
			show_stats = 1;
		}
	}
	struct ftdb ftdb = {};

	VALIDATE_FTDB_ENTRY(dbJSON,funcs);
	VALIDATE_FTDB_ENTRY(dbJSON,funcdecls);
	VALIDATE_FTDB_ENTRY(dbJSON,unresolvedfuncs);
	VALIDATE_FTDB_ENTRY(dbJSON,globals);
	VALIDATE_FTDB_ENTRY(dbJSON,types);
	VALIDATE_FTDB_ENTRY(dbJSON,fops);
	VALIDATE_FTDB_ENTRY(dbJSON,version);
	VALIDATE_FTDB_ENTRY(dbJSON,module);
	VALIDATE_FTDB_ENTRY(dbJSON,directory);
	VALIDATE_FTDB_ENTRY(dbJSON,release);

	PyObject* key_funcs = PyUnicode_FromString("funcs");
	PyObject* funcs = PyDict_GetItem(dbJSON,key_funcs);
	Py_DecRef(key_funcs);
	ftdb.funcs = calloc(PyList_Size(funcs),sizeof(struct ftdb_func_entry));
	ftdb.funcs_count = PyList_Size(funcs);
	for (Py_ssize_t i=0; i<PyList_Size(funcs); ++i) {
		libftdb_create_ftdb_func_entry(self,PyList_GetItem(funcs,i),&ftdb.funcs[i]);
		ftdb.funcs[i].__index = i;
		if (PyErr_Occurred()) {
			printf("Exception while processing function entry at index %ld\n",i);
			PyErr_PrintEx(0);
			PyErr_Clear();
		}
	}

	PyObject* key_funcdecls= PyUnicode_FromString("funcdecls");
	PyObject* funcdecls = PyDict_GetItem(dbJSON,key_funcdecls);
	Py_DecRef(key_funcdecls);
	ftdb.funcdecls = calloc(PyList_Size(funcdecls),sizeof(struct ftdb_funcdecl_entry));
	ftdb.funcdecls_count = PyList_Size(funcdecls);
	for (Py_ssize_t i=0; i<PyList_Size(funcdecls); ++i) {
		libftdb_create_ftdb_funcdecl_entry(self,PyList_GetItem(funcdecls,i),&ftdb.funcdecls[i]);
		ftdb.funcdecls[i].__index = i;
	}

	PyObject* key_unresolvedfuncs= PyUnicode_FromString("unresolvedfuncs");
	PyObject* unresolvedfuncs = PyDict_GetItem(dbJSON,key_unresolvedfuncs);
	Py_DecRef(key_unresolvedfuncs);
	ftdb.unresolvedfuncs = calloc(PyList_Size(unresolvedfuncs),sizeof(struct ftdb_unresolvedfunc_entry));
	ftdb.unresolvedfuncs_count = PyList_Size(unresolvedfuncs);
	for (Py_ssize_t i=0; i<PyList_Size(unresolvedfuncs); ++i) {
		PyObject* py_unresolvedfunc_entry = PyList_GetItem(unresolvedfuncs,i);
		struct ftdb_unresolvedfunc_entry* unresolvedfunc_entry = &ftdb.unresolvedfuncs[i];
		unresolvedfunc_entry->name = FTDB_ENTRY_STRING(py_unresolvedfunc_entry,name);
		unresolvedfunc_entry->id = FTDB_ENTRY_ULONG(py_unresolvedfunc_entry,id);
	}

	PyObject* key_globals = PyUnicode_FromString("globals");
	PyObject* globals = PyDict_GetItem(dbJSON,key_globals);
	Py_DecRef(key_globals);
	ftdb.globals = calloc(PyList_Size(globals),sizeof(struct ftdb_global_entry));
	ftdb.globals_count = PyList_Size(globals);
	for (Py_ssize_t i=0; i<PyList_Size(globals); ++i) {
		libftdb_create_ftdb_global_entry(self,PyList_GetItem(globals,i),&ftdb.globals[i]);
		ftdb.globals[i].__index = i;
	}

	PyObject* key_types = PyUnicode_FromString("types");
	PyObject* types = PyDict_GetItem(dbJSON,key_types);
	Py_DecRef(key_types);
	ftdb.types = calloc(PyList_Size(types),sizeof(struct ftdb_type_entry));
	ftdb.types_count = PyList_Size(types);
	for (Py_ssize_t i=0; i<PyList_Size(types); ++i) {
		libftdb_create_ftdb_type_entry(self,PyList_GetItem(types,i),&ftdb.types[i]);
		ftdb.types[i].__index = i;
	}

	PyObject* key_fops = PyUnicode_FromString("fops");
	PyObject* fops = PyDict_GetItem(dbJSON,key_fops);
	Py_DecRef(key_fops);
	ftdb.fops.member_count = FTDB_ENTRY_ULONG(fops,membern);
	ftdb.fops.vars_count = FTDB_ENTRY_ARRAY_SIZE(fops,vars);
	ftdb.fops.vars = calloc(ftdb.fops.vars_count,sizeof(struct ftdb_fops_var_entry));
	PyObject* key_vars = PyUnicode_FromString("vars");
	PyObject* vars = PyDict_GetItem(fops,key_vars);
	Py_DecRef(key_vars);
	for (Py_ssize_t i=0; i<ftdb.fops.vars_count; ++i) {
		libftdb_create_ftdb_fops_var_entry(self,PyList_GetItem(vars,i),&ftdb.fops.vars[i]);
		ftdb.fops.vars[i].__index = i;
	}

	PyObject* key_source_info = PyUnicode_FromString("source_info");
	PyObject* key_sources = PyUnicode_FromString("sources");
	if (PyDict_Contains(dbJSON, key_source_info)) {
		PyObject* sources = PyDict_GetItem(dbJSON,key_source_info);
		ftdb.sourceindex_table = calloc(PyList_Size(sources),sizeof(const char*));
		ftdb.sourceindex_table_count = PyList_Size(sources);
		for (Py_ssize_t i=0; i<PyList_Size(sources); ++i) {
			PyObject* single_source_map = PyList_GetItem(sources,i);
			PyObject* py_source_path = FTDB_ENTRY_PYOBJECT(single_source_map,name);
			PyObject* py_source_id = FTDB_ENTRY_PYOBJECT(single_source_map,id);
			stringRefMap_insert(&ftdb.sourcemap, PyString_get_c_str(py_source_path), PyLong_AsUnsignedLong(py_source_id));
			ftdb.sourceindex_table[PyLong_AsUnsignedLong(py_source_id)] = PyString_get_c_str(py_source_path);
		}
	}
	else if (PyDict_Contains(dbJSON, key_sources)) {
		PyObject* sources = PyDict_GetItem(dbJSON,key_sources);
		ftdb.sourceindex_table = calloc(PyList_Size(sources),sizeof(const char*));
		ftdb.sourceindex_table_count = PyList_Size(sources);
		for (Py_ssize_t i=0; i<PyList_Size(sources); ++i) {
			PyObject* single_source_legacy_map = PyList_GetItem(sources,i);
			PyObject* sslm_keys = PyDict_Keys(single_source_legacy_map);
			PyObject* py_source_path = PyList_GetItem(sslm_keys,0);
			PyObject* py_source_id = PyDict_GetItem(single_source_legacy_map,py_source_path);
			Py_DecRef(sslm_keys);
			stringRefMap_insert(&ftdb.sourcemap, PyString_get_c_str(py_source_path), PyLong_AsUnsignedLong(py_source_id));
			ftdb.sourceindex_table[PyLong_AsUnsignedLong(py_source_id)] = PyString_get_c_str(py_source_path);
		}
	}
	else {
		PyErr_SetString(libftdb_ftdbError,"ERROR: missing ftdb table \"source_info\"/\"sources\"");
		Py_DecRef(key_source_info);
		Py_DecRef(key_sources);
		Py_RETURN_NONE;
	}
	Py_DecRef(key_source_info);
	Py_DecRef(key_sources);

	PyObject* key_module_info = PyUnicode_FromString("module_info");
	PyObject* key_modules = PyUnicode_FromString("modules");
	if (PyDict_Contains(dbJSON, key_module_info)) {
		PyObject* modules = PyDict_GetItem(dbJSON,key_module_info);
		ftdb.moduleindex_table = calloc(PyList_Size(modules),sizeof(const char*));
		ftdb.moduleindex_table_count = PyList_Size(modules);
		for (Py_ssize_t i=0; i<PyList_Size(modules); ++i) {
			PyObject* single_module_map = PyList_GetItem(modules,i);
			PyObject* py_module_path = FTDB_ENTRY_PYOBJECT(single_module_map,name);
			PyObject* py_module_id = FTDB_ENTRY_PYOBJECT(single_module_map,id);
			stringRefMap_insert(&ftdb.modulemap, PyString_get_c_str(py_module_path), PyLong_AsUnsignedLong(py_module_id));
			ftdb.moduleindex_table[PyLong_AsUnsignedLong(py_module_id)] = PyString_get_c_str(py_module_path);
		}
	}
	else if (PyDict_Contains(dbJSON, key_modules)) {
		PyObject* modules = PyDict_GetItem(dbJSON,key_modules);
		ftdb.moduleindex_table = calloc(PyList_Size(modules),sizeof(const char*));
		ftdb.moduleindex_table_count = PyList_Size(modules);
		for (Py_ssize_t i=0; i<PyList_Size(modules); ++i) {
			PyObject* single_module_legacy_map = PyList_GetItem(modules,i);
			PyObject* smlm_keys = PyDict_Keys(single_module_legacy_map);
			PyObject* py_module_path = PyList_GetItem(smlm_keys,0);
			PyObject* py_module_id = PyDict_GetItem(single_module_legacy_map,py_module_path);
			Py_DecRef(smlm_keys);
			stringRefMap_insert(&ftdb.modulemap, PyString_get_c_str(py_module_path), PyLong_AsUnsignedLong(py_module_id));
			ftdb.moduleindex_table[PyLong_AsUnsignedLong(py_module_id)] = PyString_get_c_str(py_module_path);
		}
	}
	Py_DecRef(key_module_info);
	Py_DecRef(key_modules);

	PyObject* key_macroinfo = PyUnicode_FromString("macroinfo");
	if (PyDict_Contains(dbJSON, key_macroinfo)) {
		PyObject* macroinfo = PyDict_GetItem(dbJSON,key_macroinfo);
		ftdb.macroinfo_count = FTDB_ENTRY_ARRAY_SIZE(dbJSON,macroinfo);
		ftdb.macroinfo = calloc(ftdb.macroinfo_count,sizeof(struct macroinfo_item));
		for (Py_ssize_t i=0; i<PyList_Size(macroinfo); ++i) {
			PyObject* py_macroinfo_item = PyList_GetItem(macroinfo,i);
			struct macroinfo_item* macroinfo_item = &ftdb.macroinfo[i];
			libftdb_create_macroinfo_entry(py_macroinfo_item,macroinfo_item);
			stringRefMap_insert(&ftdb.macromap, macroinfo_item->name, i);
		}
	}
	Py_DecRef(key_macroinfo);

	ftdb.version = FTDB_ENTRY_STRING(dbJSON,version);
	ftdb.module = FTDB_ENTRY_STRING(dbJSON,module);
	ftdb.directory = FTDB_ENTRY_STRING(dbJSON,directory);
	ftdb.release = FTDB_ENTRY_STRING(dbJSON,release);

	ftdb.funcs_tree_calls_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_calls_no_asm,matrix_data);
	ftdb.funcs_tree_calls_no_known = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_calls_no_known,matrix_data);
	ftdb.funcs_tree_calls_no_known_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_calls_no_known_no_asm,matrix_data);
	ftdb.funcs_tree_func_calls = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_func_calls,matrix_data);
	ftdb.funcs_tree_func_refs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_func_refs,matrix_data);
	ftdb.funcs_tree_funrefs_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_funrefs_no_asm,matrix_data);
	ftdb.funcs_tree_funrefs_no_known = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_funrefs_no_known,matrix_data);
	ftdb.funcs_tree_funrefs_no_known_no_asm = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,funcs_tree_funrefs_no_known_no_asm,matrix_data);
	ftdb.globs_tree_globalrefs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,globs_tree_globalrefs,matrix_data);
	ftdb.types_tree_refs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,types_tree_refs,matrix_data);
	ftdb.types_tree_usedrefs = FTDB_ENTRY_TYPE_OPTIONAL(dbJSON,types_tree_usedrefs,matrix_data);
	ftdb.static_funcs_map_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON,static_funcs_map);
	ftdb.static_funcs_map = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON,static_funcs_map,func_map_entry,ftdb.static_funcs_map_count);
	ftdb.init_data_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON,init_data);
	ftdb.init_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON,init_data,init_data_entry,ftdb.init_data_count);
	ftdb.known_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON,known_data,known_data_entry,1);
	ftdb.BAS_data_count = FTDB_ENTRY_ARRAY_SIZE_OPTIONAL(dbJSON,BAS);
	ftdb.BAS_data = FTDB_ENTRY_TYPE_ARRAY_OPTIONAL(dbJSON,BAS,BAS_item,ftdb.BAS_data_count);

	int ok = ftdb_maps(&ftdb,show_stats);
	(void)ok;

	printf("funcs count: %ld\n",PyList_Size(funcs));
	printf("funcs entry count: %zu\n",ftdb.funcs_count);
	printf("funcdecls count: %ld\n",PyList_Size(funcdecls));
	printf("funcdecls entry count: %zu\n",ftdb.funcdecls_count);
	printf("unresolvedfuncs count: %ld\n",PyList_Size(unresolvedfuncs));
	printf("unresolvedfuncs entry count: %zu\n",ftdb.unresolvedfuncs_count);
	printf("globals count: %ld\n",PyList_Size(globals));
	printf("globals entry count: %zu\n",ftdb.globals_count);
	printf("types count: %ld\n",PyList_Size(types));
	printf("types entry count: %zu\n",ftdb.types_count);
	printf("fops vars count: %ld\n",PyList_Size(vars));
	printf("fops vars entry count: %zu\n",ftdb.fops.vars_count);
	printf("macroinfo entry count: %zu\n",ftdb.macroinfo_count);
	if (ftdb.funcs_tree_calls_no_asm) printf("funcs_tree_calls_no_asm: OK\n");
	if (ftdb.funcs_tree_calls_no_known) printf("funcs_tree_calls_no_known: OK\n");
	if (ftdb.funcs_tree_calls_no_known_no_asm) printf("funcs_tree_calls_no_known_no_asm: OK\n");
	if (ftdb.funcs_tree_func_calls) printf("funcs_tree_func_calls: OK\n");
	if (ftdb.funcs_tree_func_refs) printf("funcs_tree_func_refs: OK\n");
	if (ftdb.funcs_tree_funrefs_no_asm) printf("funcs_tree_funrefs_no_asm: OK\n");
	if (ftdb.funcs_tree_funrefs_no_known) printf("funcs_tree_funrefs_no_known: OK\n");
	if (ftdb.funcs_tree_funrefs_no_known_no_asm) printf("funcs_tree_funrefs_no_known_no_asm: OK\n");
	if (ftdb.globs_tree_globalrefs) printf("globs_tree_globalrefs: OK\n");
	if (ftdb.types_tree_refs) printf("types_tree_refs: OK\n");
	if (ftdb.types_tree_usedrefs) printf("types_tree_usedrefs: OK\n");
	if (ftdb.static_funcs_map) printf("static_funcs_map [%lu]: OK\n",ftdb.static_funcs_map_count);
	if (ftdb.init_data) printf("init_data [%lu]: OK\n",ftdb.init_data_count);
	if (ftdb.known_data) printf("known_data: OK\n");
	if (ftdb.BAS_data) printf("BAS_data [%lu]: OK\n",ftdb.BAS_data_count);

	const char* dbfn_s =  PyString_get_c_str(dbfn);
	FILE* out = fopen(dbfn_s, "w");
	if (!out) {
		printf("Couldn't create flatten image file: %s\n",dbfn_s);
		PYASSTR_DECREF(dbfn_s);
		Py_RETURN_FALSE;
	}
	PYASSTR_DECREF(dbfn_s);

	flatten_init();
	FOR_ROOT_POINTER(&ftdb,
		UNDER_ITER_HARNESS2(
			FLATTEN_STRUCT2_ITER(ftdb,&ftdb);
		);
	);

	int err;
	if ((err = flatten_write(out)) != 0) {
		printf("flatten_write(): %d\n",err);
		Py_RETURN_FALSE;
	}

	flatten_fini();
	fclose(out);

	destroy_ftdb(&ftdb);

	Py_RETURN_NONE;
}

PyObject * libftdb_parse_c_fmt_string(PyObject *self, PyObject *args) {

	PyObject* py_fmt = PyTuple_GetItem(args,0);
	const char* fmt =  PyString_get_c_str(py_fmt);
	enum format_type* par_type = malloc(256*sizeof(enum format_type));
	size_t n = vsnprintf_parse_format(&par_type,256,fmt);

	if (n<0) {
		free(par_type);
		Py_RETURN_NONE;
	}

	PyObject* argv = PyList_New(0);
	for (int i=0; i<n; ++i) {
		enum format_type v = par_type[i];
		PyList_Append(argv,PyLong_FromLong((long)v));
	}

	free(par_type);
	PYASSTR_DECREF(fmt);
	return argv;
}

PyMODINIT_FUNC
PyInit_libftdb(void)
{
    PyObject* m;

    m = PyModule_Create(&libftdb_module);
    if (m==0) return 0;

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

	if (PyType_Ready(&libftdb_ftdbMacroinfoType) < 0)
		return 0;
	Py_XINCREF(&libftdb_ftdbMacroinfoType);

	if (PyType_Ready(&libftdb_ftdbMacroinfoIterType) < 0)
		return 0;
	Py_XINCREF(&libftdb_ftdbMacroinfoIterType);

    if (PyModule_AddObject(m, "ftdb", (PyObject *)&libftdb_ftdbType)<0) {
    	Py_DECREF(m);
    	return 0;
    }

    libftdb_ftdbError = PyErr_NewException("libftdb.error",0,0);
    Py_XINCREF(libftdb_ftdbError);
    if (PyModule_AddObject(m, "error", libftdb_ftdbError) < 0) {
    	Py_XDECREF(libftdb_ftdbError);
		Py_CLEAR(libftdb_ftdbError);
		Py_DECREF(m);
    }

    return m;
}
