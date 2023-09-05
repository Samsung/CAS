#ifndef __PYETRACE_H_
#define __PYETRACE_H_

#define PY_SSIZE_T_CLEAN
#ifdef __cplusplus
extern "C" {
#endif
#include <Python.h>
#include <structmember.h>
#include <signal.h>
#include <fnmatch.h>
#include <time.h>
#include <stdbool.h>
#include <string.h>

// Necessary to use "rbtree.h" library
#ifndef container_of
#define container_of(ptr, type, member) ({			\
  	const __typeof__( ((type *)0)->member ) *__mptr = (ptr);	\
  	(type *)( (char *)__mptr - offsetof(type,member) );})
#endif

#include "rbtree.h"
#include "utils.h"
#include "nfsdb.h"
#include "ftdb_entry.h"

#include <unflatten.hpp>

#define LIBETRACE_EMPTY_STRING_HANDLE 0

typedef int (*nfsdb_entry_openfile_filter_func)(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
typedef struct {
	const char* nfsdb_entry_openfile_filter_funcname;
	nfsdb_entry_openfile_filter_func fp;
} nfsdb_entry_openfile_filter_func_array_t;

extern nfsdb_entry_openfile_filter_func_array_t nfsdb_entry_openfile_filter_func_array[];

typedef int (*nfsdb_entry_command_filter_func)(void *self, const struct nfsdb_entry* data, const void* arg);
typedef struct {
	const char* nfsdb_entry_command_filter_funcname;
	nfsdb_entry_command_filter_func fp;
} nfsdb_entry_command_filter_func_array_t;

extern nfsdb_entry_command_filter_func_array_t nfsdb_entry_command_filter_func_array[];

int libetrace_nfsdb_entry_openfile_filter_contains_path(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_matches_wc(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_matches_re(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_is_class(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_file_exists(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_file_not_exists(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_dir_exists(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_has_access(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_at_source_root(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_not_at_source_root(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_source_type(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);
int libetrace_nfsdb_entry_openfile_filter_in_path_list(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index);

int libetrace_nfsdb_entry_command_filter_cwd_contains_path(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cwd_matches_wc(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cwd_matches_re(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cmd_has_string(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cmd_matches_wc(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cmd_matches_re(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_bin_contains_path(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_bin_matches_wc(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_bin_matches_re(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_has_ppid(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_is_class(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_bin_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_bin_not_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cwd_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_cwd_not_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_in_binaries_list(void *self, const struct nfsdb_entry* data, const void* arg);
int libetrace_nfsdb_entry_command_filter_in_pids_list(void *self, const struct nfsdb_entry* data, const void* arg);


#define LIBETRACE_NFSDB_ENTRY_OPENFILE_FILTER_FUNC_ARRAY_SIZE \
		(sizeof(nfsdb_entry_openfile_filter_func_array)/sizeof(nfsdb_entry_openfile_filter_func_array_t))

#define LIBETRACE_NFSDB_ENTRY_COMMAND_FILTER_FUNC_ARRAY_SIZE \
		(sizeof(nfsdb_entry_command_filter_func_array)/sizeof(nfsdb_entry_command_filter_func_array_t))

enum file_class {
	FILE_CLASS_LINKED = 0x0001, /* 1 */
	FILE_CLASS_LINKED_STATIC = 0x0002, /* 2 */
	FILE_CLASS_LINKED_SHARED = 0x0004, /* 4 */
	FILE_CLASS_LINKED_EXE = 0x0008, /* 8 */
	FILE_CLASS_COMPILED = 0x0010, /* 16 */
	FILE_CLASS_PLAIN = 0x0020, /* 32 */
	FILE_CLASS_COMPILER = 0x0040, /* 64 */
	FILE_CLASS_LINKER = 0x0080, /* 128 */
	FILE_CLASS_BINARY = 0x0100, /* 256 */
	FILE_CLASS_SYMLINK = 0x0200, /* 512 */
	FILE_CLASS_NOSYMLINK = 0x0400, /* 1024 */
};

enum file_source_type {
	FILE_SOURCE_TYPE_C = 0x0001, /* 1 */
	FILE_SOURCE_TYPE_CPP = 0x0002, /* 2 */
	FILE_SOURCE_TYPE_OTHER = 0x0004, /* 4 */
};

enum file_access {
	ACCESS_READ,
	ACCESS_WRITE,
	ACCESS_RW,
	GLOBAL_ACCESS_READ,
	GLOBAL_ACCESS_WRITE,
	GLOBAL_ACCESS_RW,
};

#define FILE_FILTER_SIZE 7

struct file_filter {
	struct {
		nfsdb_entry_openfile_filter_func func;
		const void* arg;
	} filter_set [FILE_FILTER_SIZE];
	int negate;
};

enum command_class {
	FILE_CLASS_COMMAND = 0x0001, /* 1 */
	FILE_CLASS_COMPILING = 0x0002, /* 2 */
	FILE_CLASS_LINKING = 0x0004, /* 4 */
};

#define COMMAND_FILTER_SIZE 6

struct command_filter {
	struct {
		nfsdb_entry_command_filter_func func;
		const void* arg;
	} filter_set [COMMAND_FILTER_SIZE];
	int negate;
};

void libetrace_nfsdb_entry_precompute_linked_file(struct nfsdb* nfsdb, struct nfsdb_entry* entry);
void libetrace_nfsdb_entry_precompute_compilation_info_files(struct nfsdb* nfsdb, struct nfsdb_entry* entry);
void libetrace_nfsdb_entry_precompute_compilation_info_headers(struct nfsdb* nfsdb, struct nfsdb_entry* entry);
void libetrace_nfsdb_entry_precompute_compilation_info_objects(struct nfsdb* nfsdb, struct nfsdb_entry* entry);

const char* libetrace_nfsdb_string_handle_join(const struct nfsdb* nfsdb, unsigned long* argv, unsigned long argv_count, const char* sep);

PyObject * libetrace_is_LLVM_BC_file(PyObject *self, PyObject *args);
PyObject * libetrace_is_ELF_file(PyObject *self, PyObject *args);
PyObject * libetrace_pytools_is_ELF_or_LLVM_BC_file(PyObject *self, PyObject *args);
PyObject * libetrace_precompute_command_patterns(PyObject *self, PyObject *args, PyObject* kwargs);
PyObject * libetrace_parse_compiler_triple_hash(PyObject *self, PyObject *args);
PyObject * libetrace_create_nfsdb(PyObject *self, PyObject *args, PyObject* kwargs);

unsigned long nfsdb_has_unique_keys(const struct nfsdb* nfsdb);
int nfsdb_maps(struct nfsdb* nfsdb, int show_stats);
int libetrace_nfsdb_entry_is_linking_internal(const struct nfsdb_entry * entry);
int libetrace_nfsdb_entry_has_compilations_internal(const struct nfsdb_entry * entry);

#ifdef __cplusplus
}
#endif

typedef int64_t upid_t;
#define GENERIC_ARG_PID_FMT "%" PRId64
#define PY_ARG_PID_FMT "l"

PyObject * libetrace_create_nfsdb(PyObject *self, PyObject *args, PyObject* kwargs);
PyObject * libetrace_parse_nfsdb(PyObject *self, PyObject *args);
extern PyObject *libetrace_nfsdbError;

static PyMethodDef libetrace_methods[] = {
	{"is_LLVM_BC_file", (PyCFunction)libetrace_is_LLVM_BC_file, METH_VARARGS,"Checks whether the given file has a proper LLVM bitcode header"},
	{"is_ELF_file", (PyCFunction)libetrace_is_ELF_file, METH_VARARGS,"Checks whether the given file has a proper ELF header"},
	{"is_ELF_or_LLVM_BC_file", (PyCFunction)libetrace_pytools_is_ELF_or_LLVM_BC_file, METH_VARARGS,"Checks whether the given file has a proper ELF or LLVM bitcode header"},
	{"precompute_command_patterns", (PyCFunction)libetrace_precompute_command_patterns, METH_VARARGS|METH_KEYWORDS,"Precompute command patterns for file dependency processing"},
	{"parse_compiler_triple_hash", (PyCFunction)libetrace_parse_compiler_triple_hash, METH_VARARGS,"Parse compiler -### output for clang"},
	{"create_nfsdb", (PyCFunction)libetrace_create_nfsdb, METH_VARARGS | METH_KEYWORDS,""},
	{"parse_nfsdb", (PyCFunction)libetrace_parse_nfsdb, METH_VARARGS,""},
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

#ifdef __cplusplus
__attribute__((unused))
#endif
static struct PyModuleDef libetrace_module = {
    PyModuleDef_HEAD_INIT,
    "libetrace",   				/* name of module */
    "etrace Python tooling",	/* module documentation, may be NULL */
    -1,							/* size of per-interpreter state of the module,
                 	 	 	 	   or -1 if the module keeps state in global variables. */
	libetrace_methods,
};

typedef struct {
    PyObject_HEAD
	int verbose;
    int debug;
    int init_done;

	CUnflatten unflatten;
	CUnflatten unflatten_deps;

    const struct nfsdb* nfsdb;
    const struct nfsdb_deps* nfsdb_deps;
    PyObject* libetrace_nfsdb_entry_openfile_filterMap;
    PyObject* libetrace_nfsdb_entry_command_filterMap;
    PyObject* reMatchFunction;
} libetrace_nfsdb_object;

void libetrace_nfsdb_dealloc(libetrace_nfsdb_object* self);
PyObject* libetrace_nfsdb_repr(PyObject* self);
PyObject* libetrace_nfsdb_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
int libetrace_nfsdb_init(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwds);
int libetrace_nfsdb_bool(PyObject* self);
Py_ssize_t libetrace_nfsdb_sq_length(PyObject* self);
PyObject* libetrace_nfsdb_sq_item(PyObject* self, Py_ssize_t index);
PyObject* libetrace_nfsdb_mp_subscript(PyObject* self, PyObject* slice);
PyObject* libetrace_nfsdb_getiter(PyObject* self);
PyObject* libetrace_nfsdb_load(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwargs);
PyObject* libetrace_nfsdb_load_deps(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwargs);
PyObject* libetrace_nfsdb_iter(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_opens_iter(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_opens_list(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_opens_paths(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_filtered_opens_paths(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_filtered_opens_paths_iter(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_filtered_opens(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_filtered_opens_iter(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_filtered_execs(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_filtered_execs_iter(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_pcp_list(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_pid_list(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_bpath_list(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_fpath_list(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_linked_modules(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_linked_module_paths(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_path_exists(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_path_read(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_path_write(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_path_regular(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_path_symlinked(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_symlink_paths(libetrace_nfsdb_object *self, PyObject *args);
PyObject* libetrace_nfsdb_create_deps_cache(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_precompute_command_patterns(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_get_filemap(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_get_source_root(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_get_dbversion(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_module_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_module_dependencies_count(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_reverse_module_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
PyObject* libetrace_nfsdb_filemap_has_path(libetrace_nfsdb_object *self, PyObject *args);
int libetrace_nfsdb_entry_has_shared_argv(const struct nfsdb_entry * entry);

#ifdef __cplusplus
extern "C" {
#endif
PyObject* libetrace_nfsdb_file_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs);
int parser_main(int argc, char** argv);
#ifdef __cplusplus
}
#endif

static PyMethodDef libetrace_nfsdb_methods[] = {
	{"load",(PyCFunction)libetrace_nfsdb_load,METH_VARARGS|METH_KEYWORDS,"Load the database cache file"},
	{"load_deps",(PyCFunction)libetrace_nfsdb_load_deps,METH_VARARGS|METH_KEYWORDS,"Load the dependency database cache file"},
	{"iter",(PyCFunction)libetrace_nfsdb_iter,METH_VARARGS,"Returns the cache iterator"},
	{"opens_iter",(PyCFunction)libetrace_nfsdb_opens_iter,METH_VARARGS,"Returns the open files iterator across entire database"},
	{"opens",(PyCFunction)libetrace_nfsdb_opens_list,METH_VARARGS,"Returns the list of all opened files across entire database"},
	{"opens_paths",(PyCFunction)libetrace_nfsdb_opens_paths,METH_VARARGS,"Returns the list of all unique opened paths across entire database"},
	{"filtered_paths",(PyCFunction)libetrace_nfsdb_filtered_opens_paths,METH_VARARGS|METH_KEYWORDS,"Returns the list of all unique opened paths across entire database filtered by a given set of filters"},
	{"filtered_paths_iter",(PyCFunction)libetrace_nfsdb_filtered_opens_paths_iter,METH_VARARGS|METH_KEYWORDS,"Returns the iterator to the unique opened paths across entire database filtered by a given set of filters"},
	{"filtered_opens",(PyCFunction)libetrace_nfsdb_filtered_opens,METH_VARARGS|METH_KEYWORDS,"Returns the list of all unique opened files across entire database filtered by a given set of filters"},
	{"filtered_opens_iter",(PyCFunction)libetrace_nfsdb_filtered_opens_iter,METH_VARARGS|METH_KEYWORDS,"Returns the iterator to the unique opened files across entire database filtered by a given set of filters"},
	{"filtered_execs",(PyCFunction)libetrace_nfsdb_filtered_execs,METH_VARARGS|METH_KEYWORDS,"Returns the list of all executions across entire database filtered by a given set of filters"},
	{"filtered_execs_iter",(PyCFunction)libetrace_nfsdb_filtered_execs_iter,METH_VARARGS|METH_KEYWORDS,"Returns the iterator to the all executions across entire database filtered by a given set of filters"},
	{"pcp_list",(PyCFunction)libetrace_nfsdb_pcp_list,METH_VARARGS,"Returns the list of command patterns to be precomputed"},
	{"pids",(PyCFunction)libetrace_nfsdb_pid_list,METH_VARARGS,"Returns the list of all unique pids in the database"},
	{"bpaths",(PyCFunction)libetrace_nfsdb_bpath_list,METH_VARARGS,"Returns the list of all unique binary paths in the database"},
	{"fpaths",(PyCFunction)libetrace_nfsdb_fpath_list,METH_VARARGS,"Returns the list of all unique opened file paths in the database"},
	{"linked_modules",(PyCFunction)libetrace_nfsdb_linked_modules,METH_VARARGS,"Returns the list of all linked modules present in the database"},
	{"linked_module_paths",(PyCFunction)libetrace_nfsdb_linked_module_paths,METH_VARARGS,"Returns the list of all linked module paths present in the database"},
	{"fdeps",(PyCFunction)libetrace_nfsdb_file_dependencies,METH_VARARGS|METH_KEYWORDS,"Returns the list of file dependencies for a given file"},
	{"mdeps",(PyCFunction)libetrace_nfsdb_module_dependencies,METH_VARARGS|METH_KEYWORDS,"Returns the list of file dependencies for a given module"},
	{"mdeps_count",(PyCFunction)libetrace_nfsdb_module_dependencies_count,METH_VARARGS|METH_KEYWORDS,"Returns the count of file dependencies for a given module"},
	{"rdeps",(PyCFunction)libetrace_nfsdb_reverse_module_dependencies,METH_VARARGS|METH_KEYWORDS,"Returns the list of reverse module dependencies for a given file"},
	{"path_exists",(PyCFunction)libetrace_nfsdb_path_exists,METH_VARARGS,"Returns True if a given path exists after the build"},
	{"path_read",(PyCFunction)libetrace_nfsdb_path_read,METH_VARARGS,"Returns True if a given path was opened for read during the build"},
	{"path_write",(PyCFunction)libetrace_nfsdb_path_write,METH_VARARGS,"Returns True if a given path was opened for write during the build"},
	{"path_regular",(PyCFunction)libetrace_nfsdb_path_regular,METH_VARARGS,"Returns True if a given path is a regular file (which implies that path exists after the build)"},
	{"path_symlinked",(PyCFunction)libetrace_nfsdb_path_symlinked,METH_VARARGS,"Returns True if a given path was reached through a symbolic link"},
	{"symlink_paths",(PyCFunction)libetrace_nfsdb_symlink_paths,METH_VARARGS,"Returns list of original symlink paths for a given resolved path"},
	{"create_deps_cache", (PyCFunction)libetrace_nfsdb_create_deps_cache, METH_VARARGS | METH_KEYWORDS,""},
	{"precompute_command_patterns",(PyCFunction)libetrace_nfsdb_precompute_command_patterns, METH_VARARGS|METH_KEYWORDS,"Precompute command patterns for file dependency processing"},
	{"filemap_has_path",(PyCFunction)libetrace_nfsdb_filemap_has_path,METH_VARARGS,"Returns True if a given opened path exists in the database"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyGetSetDef libetrace_nfsdb_getset[] = {
	{"filemap",libetrace_nfsdb_get_filemap,0,"nfsdb filemap object (maps a path to the corresponding openfile entry (or the nfsdb entry where the file was opened)",0},
	{"source_root",libetrace_nfsdb_get_source_root,0,"nfsdb database source root",0},
	{"dbversion",libetrace_nfsdb_get_dbversion,0,"nfsdb database version string",0},
	{0,0,0,0,0},
};

static PyMemberDef libetrace_nfsdb_members[] = {
    {0}  /* Sentinel */
};

static PyNumberMethods libetrace_nfsdb_number_methods = {
		.nb_bool = libetrace_nfsdb_bool,
};

static PySequenceMethods libetrace_nfsdb_sequence_methods = {
		.sq_length = libetrace_nfsdb_sq_length,
		.sq_item = libetrace_nfsdb_sq_item,
};

static PyMappingMethods libetrace_nfsdb_mapping_methods = {
		.mp_subscript = libetrace_nfsdb_mp_subscript,
};

#ifdef __cplusplus
__attribute__((unused))
#endif
static PyTypeObject libetrace_nfsdbType = {
	PyVarObject_HEAD_INIT(NULL, 0)
    "libetrace.nfsdb",	               			/* tp_name */
	sizeof(libetrace_nfsdb_object), 			/* tp_basicsize */
	0,                              			/* tp_itemsize */
	/* Methods to implement standard operations */
	(destructor)libetrace_nfsdb_dealloc,      	/* tp_dealloc */
	0,                              			/* tp_vectorcall_offset */
	0,                              			/* tp_getattr */
	0,                              			/* tp_setattr */
	0,                              			/* tp_as_async */
	(reprfunc)libetrace_nfsdb_repr,    			/* tp_repr */
	/* Method suites for standard classes */
	&libetrace_nfsdb_number_methods,        	/* tp_as_number */
	&libetrace_nfsdb_sequence_methods,   		/* tp_as_sequence */
	&libetrace_nfsdb_mapping_methods,  			/* tp_as_mapping */
	/* More standard operations (here for binary compatibility) */
	0,                              			/* tp_hash */
	0,                              			/* tp_call */
	0,                              			/* tp_str */
	0,                              			/* tp_getattro */
	0,                              			/* tp_setattro */
	/* Functions to access object as input/output buffer */
	0,                              			/* tp_as_buffer */
	/* Flags to define presence of optional/expanded features */
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,	/* tp_flags */
	"libetrace nfsdb object",                   /* tp_doc */
	/* Assigned meaning in release 2.0 */
	/* call function for all accessible objects */
	0,                              			/* tp_traverse */
	/* delete references to contained objects */
	0,                              			/* tp_clear */
	/* Assigned meaning in release 2.1 */
	/* rich comparisons */
	0,                              			/* tp_richcompare */
	/* weak reference enabler */
	0,                              			/* tp_weaklistoffset */
	/* Iterators */
	libetrace_nfsdb_getiter,           			/* tp_iter */
	0,                              			/* tp_iternext */
	/* Attribute descriptor and subclassing stuff */
	libetrace_nfsdb_methods,           			/* tp_methods */
	libetrace_nfsdb_members,           			/* tp_members */
	libetrace_nfsdb_getset,  	     			/* tp_getset */
	0,                              			/* tp_base */
	0,                              			/* tp_dict */
	0,                              			/* tp_descr_get */
	0,                              			/* tp_descr_set */
	0,                              			/* tp_dictoffset */
	(initproc)libetrace_nfsdb_init,    			/* tp_init */
	0,                              			/* tp_alloc */
	libetrace_nfsdb_new,            			/* tp_new */
};

typedef struct {
    PyObject_HEAD
	const struct nfsdb_entry* entry;
    unsigned long nfsdb_index;
    const struct nfsdb* nfsdb;
} libetrace_nfsdb_entry_object;

void libetrace_nfsdb_entry_dealloc(libetrace_nfsdb_entry_object* self);
PyObject* libetrace_nfsdb_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libetrace_nfsdb_entry_repr(PyObject* self);
PyObject* libetrace_nfsdb_entry_json(libetrace_nfsdb_entry_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_has_pcp(libetrace_nfsdb_entry_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_pcp_index(libetrace_nfsdb_entry_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_is_wrapped(libetrace_nfsdb_entry_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_has_compilations(libetrace_nfsdb_entry_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_is_linking(libetrace_nfsdb_entry_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_filtered_openfiles(PyObject* self, PyObject *args);
PyObject* libetrace_nfsdb_entry_accessed_openfiles(PyObject* self, PyObject *args);
PyObject* libetrace_nfsdb_entry_get_argvn(PyObject* self, PyObject *args);
PyObject* libetrace_nfsdb_entry_get_eid(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_ptr(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_etime(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_parent_eid(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_parent(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_childs(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_next_entry(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_prev_entry(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_child_cids(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_binary(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_cwd(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_bpath(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_argv(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_command(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_return_code(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_openfiles(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_openpaths(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_openfiles_with_children(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_openpaths_with_children(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_pcp(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_wrapper_pid(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_pipe_eids(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_linked_file(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_linked_path(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_linked_ptr(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_linked_type(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_get_compilation_info(PyObject* self, void* closure);
Py_hash_t libetrace_nfsdb_entry_hash(PyObject *o);
PyObject* libetrace_nfsdb_entry_richcompare(PyObject *self, PyObject *other, int op);

static PyMethodDef libetrace_nfsdbEntry_methods[] = {
	{"json",(PyCFunction)libetrace_nfsdb_entry_json,METH_VARARGS,"Returns the json representation of the nfsdb entry"},
	{"has_pcp",(PyCFunction)libetrace_nfsdb_entry_has_pcp,METH_VARARGS,"Returns true if the given nfsdb entry has pcp array populated"},
	{"pcp_index",(PyCFunction)libetrace_nfsdb_entry_pcp_index,METH_VARARGS,"Returns list of indexes for matching patterns for this nfsdb entry"},
	{"is_wrapped",(PyCFunction)libetrace_nfsdb_entry_is_wrapped,METH_VARARGS,"Returns true if the given execution is wrapped by other call"},
	{"has_compilations",(PyCFunction)libetrace_nfsdb_entry_has_compilations,METH_VARARGS,"Returns true if the given execution has compilation entries"},
	{"is_linking",(PyCFunction)libetrace_nfsdb_entry_is_linking,METH_VARARGS,"Returns true if the given execution has linker entry"},
	{"open_stat",(PyCFunction)libetrace_nfsdb_entry_filtered_openfiles,METH_VARARGS,"Returns the list of filtered opened files (by file type) for this execution"},
	{"open_access",(PyCFunction)libetrace_nfsdb_entry_accessed_openfiles,METH_VARARGS,"Returns the list of filtered opened files (by access type) for this execution"},
	{"argvn",(PyCFunction)libetrace_nfsdb_entry_get_argvn,METH_VARARGS,"nfsdb entry n'th argv"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyMemberDef libetrace_nfsdbEntry_members[] = {
    {0}  /* Sentinel */
};

static PyGetSetDef libetrace_nfsdbEntry_getset[] = {
	{"eid",libetrace_nfsdb_entry_get_eid,0,"nfsdb entry eid value",0},
	{"etime",libetrace_nfsdb_entry_get_etime,0,"nfsdb entry etime value",0},
	{"parent_eid",libetrace_nfsdb_entry_get_parent_eid,0,"nfsdb entry parent eid value",0},
	{"parent",libetrace_nfsdb_entry_get_parent,0,"nfsdb entry parent entry",0},
	{"child_cids",libetrace_nfsdb_entry_get_child_cids,0,"nfsdb entry child cids list",0},
	{"childs",libetrace_nfsdb_entry_get_childs,0,"nfsdb entry childs list",0},
	{"next",libetrace_nfsdb_entry_get_next_entry,0,"get the next execution entry in the same process (if this is the last execution, returns None)",0},
	{"prev",libetrace_nfsdb_entry_get_prev_entry,0,"get the prev execution entry in the same process (if this is the first execution, returns None)",0},
	{"binary",libetrace_nfsdb_entry_get_binary,0,"nfsdb entry binary value",0},
	{"cwd",libetrace_nfsdb_entry_get_cwd,0,"nfsdb entry cwd value",0},
	{"bpath",libetrace_nfsdb_entry_get_bpath,0,"nfsdb entry binary path value",0},
	{"argv",libetrace_nfsdb_entry_get_argv,0,"nfsdb entry argv list",0},
	{"cmd",libetrace_nfsdb_entry_get_command,0,"nfsdb entry command string",0},
	{"return_code",libetrace_nfsdb_entry_get_return_code,0,"return code of the entire process this nfsdb entry is a part of",0},
	{"opens",libetrace_nfsdb_entry_get_openfiles,0,"nfsdb entry open files list",0},
	{"openpaths",libetrace_nfsdb_entry_get_openpaths,0,"nfsdb entry open file paths list",0},
	{"opens_with_children",libetrace_nfsdb_entry_get_openfiles_with_children,0,"nfsdb entry open files list in the current process and all its children",0},
	{"openpaths_with_children",libetrace_nfsdb_entry_get_openpaths_with_children,0,"nfsdb entry open file paths list in the current process and all its children",0},
	{"pcp",libetrace_nfsdb_entry_get_pcp,0,"nfsdb entry for precomputed command patterns",0},
	{"wpid",libetrace_nfsdb_entry_get_wrapper_pid,0,"nfsdb entry wrapper pid",0},
	{"pipe_eids",libetrace_nfsdb_entry_get_pipe_eids,0,"nfsdb entry pipe eid values",0},
	{"linked_file",libetrace_nfsdb_entry_get_linked_file,0,"nfsdb entry linked file entry",0},
	{"linked_path",libetrace_nfsdb_entry_get_linked_path,0,"nfsdb entry linked file path",0},
	{"linked_ptr",libetrace_nfsdb_entry_get_linked_ptr,0,"nfsdb low-level pointer representation of the linked file",0},
	{"linked_type",libetrace_nfsdb_entry_get_linked_type,0,"nfsdb entry linked file type",0},
	{"compilation_info",libetrace_nfsdb_entry_get_compilation_info,0,"nfsdb entry compilation info",0},
	{"ptr",libetrace_nfsdb_entry_get_ptr,0,"nfsdb entry low level index value",0},
	{0,0,0,0,0},
};

static PyTypeObject libetrace_nfsdbEntryType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbEntry",
	.tp_basicsize = sizeof(libetrace_nfsdbEntryType),
	.tp_dealloc = (destructor)libetrace_nfsdb_entry_dealloc,
	.tp_repr = (reprfunc)libetrace_nfsdb_entry_repr,
	.tp_hash = (hashfunc)libetrace_nfsdb_entry_hash,
	.tp_doc = "libetrace nfsdbEntry object",
	.tp_richcompare = libetrace_nfsdb_entry_richcompare,
	.tp_methods = libetrace_nfsdbEntry_methods,
	.tp_members = libetrace_nfsdbEntry_members,
	.tp_getset = &libetrace_nfsdbEntry_getset[0],
	.tp_new = libetrace_nfsdb_entry_new,
};

typedef struct {
    PyObject_HEAD
	unsigned long start;
    unsigned long step;
    unsigned long end;
    const struct nfsdb* nfsdb;
} libetrace_nfsdb_iter_object;

void libetrace_nfsdb_iter_dealloc(libetrace_nfsdb_iter_object* self);
PyObject* libetrace_nfsdb_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libetrace_nfsdb_iter_sq_length(PyObject* self);
PyObject* libetrace_nfsdb_iter_next(PyObject *self);

static PySequenceMethods libetrace_nfsdbIter_sequence_methods = {
		.sq_length = libetrace_nfsdb_iter_sq_length,
};

static PyTypeObject libetrace_nfsdbIterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbIter",
	.tp_basicsize = sizeof(libetrace_nfsdbIterType),
	.tp_dealloc = (destructor)libetrace_nfsdb_iter_dealloc,
	.tp_as_sequence = &libetrace_nfsdbIter_sequence_methods,
	.tp_doc = "libetrace nfsdb iterator",
	.tp_iter = PyObject_SelfIter,
	.tp_iternext = libetrace_nfsdb_iter_next,
	.tp_new = libetrace_nfsdb_iter_new,
};

typedef struct {
    PyObject_HEAD
	unsigned long start;
    unsigned long step;
    unsigned long end;
    unsigned long open_index;
    const struct nfsdb* nfsdb;
} libetrace_nfsdb_opens_iter_object;

void libetrace_nfsdb_opens_iter_dealloc(libetrace_nfsdb_opens_iter_object* self);
PyObject* libetrace_nfsdb_opens_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libetrace_nfsdb_opens_iter_sq_length(PyObject* self);
PyObject* libetrace_nfsdb_opens_iter_next(PyObject *self);

static PySequenceMethods libetrace_nfsdbOpensIter_sequence_methods = {
		.sq_length = libetrace_nfsdb_opens_iter_sq_length,
};

static PyTypeObject libetrace_nfsdbOpensIterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbOpensIter",
	.tp_basicsize = sizeof(libetrace_nfsdbOpensIterType),
	.tp_dealloc = (destructor)libetrace_nfsdb_opens_iter_dealloc,
	.tp_as_sequence = &libetrace_nfsdbOpensIter_sequence_methods,
	.tp_doc = "libetrace nfsdb openfile iterator",
	.tp_iter = PyObject_SelfIter,
	.tp_iternext = libetrace_nfsdb_opens_iter_next,
	.tp_new = libetrace_nfsdb_opens_iter_new,
};

typedef struct {
    PyObject_HEAD
    libetrace_nfsdb_object* nfsdb;
    unsigned long patch_size;
    struct file_filter** cflts;
    size_t* cflts_size;
    struct file_filter* fflts[1];
    size_t fflts_size[1];
    struct file_filter* fast_filter;
    Py_ssize_t filter_count;
    struct rb_node* filemap_node;
} libetrace_nfsdb_filtered_opens_paths_iter_object;

void libetrace_nfsdb_filtered_opens_paths_iter_dealloc(libetrace_nfsdb_filtered_opens_paths_iter_object* self);
PyObject* libetrace_nfsdb_filtered_opens_paths_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libetrace_nfsdb_filtered_opens_paths_iter_sq_length(PyObject* self);
PyObject* libetrace_nfsdb_filtered_opens_paths_iter_next(PyObject *self);

static PySequenceMethods libetrace_nfsdbFilteredOpensPathsIter_sequence_methods = {
	.sq_length = libetrace_nfsdb_filtered_opens_paths_iter_sq_length,
};

static PyTypeObject libetrace_nfsdbFilteredOpensPathsIterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbFilteredOpensPathsIter",
	.tp_basicsize = sizeof(libetrace_nfsdbFilteredOpensPathsIterType),
	.tp_dealloc = (destructor)libetrace_nfsdb_filtered_opens_paths_iter_dealloc,
	.tp_as_sequence = &libetrace_nfsdbFilteredOpensPathsIter_sequence_methods,
	.tp_doc = "libetrace nfsdb filtered opens paths iterator",
	.tp_iter = PyObject_SelfIter,
	.tp_iternext = libetrace_nfsdb_filtered_opens_paths_iter_next,
	.tp_new = libetrace_nfsdb_filtered_opens_paths_iter_new,
};

typedef struct {
    PyObject_HEAD
    libetrace_nfsdb_object* nfsdb;
    unsigned long patch_size;
    unsigned long path_index;	/* Index of a nfsdbEntry in a given path to flush next */
    struct file_filter** cflts;
    size_t* cflts_size;
    struct file_filter* fflts[1];
    size_t fflts_size[1];
    struct file_filter* fast_filter;
    Py_ssize_t filter_count;
    struct rb_node* filemap_node;
} libetrace_nfsdb_filtered_opens_iter_object;

void libetrace_nfsdb_filtered_opens_iter_dealloc(libetrace_nfsdb_filtered_opens_iter_object* self);
PyObject* libetrace_nfsdb_filtered_opens_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libetrace_nfsdb_filtered_opens_iter_sq_length(PyObject* self);
PyObject* libetrace_nfsdb_filtered_opens_iter_next(PyObject *self);

static PySequenceMethods libetrace_nfsdbFilteredOpensIter_sequence_methods = {
		.sq_length = libetrace_nfsdb_filtered_opens_iter_sq_length,
};

static PyTypeObject libetrace_nfsdbFilteredOpensIterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbFilteredOpensIter",
	.tp_basicsize = sizeof(libetrace_nfsdbFilteredOpensIterType),
	.tp_dealloc = (destructor)libetrace_nfsdb_filtered_opens_iter_dealloc,
	.tp_as_sequence = &libetrace_nfsdbFilteredOpensIter_sequence_methods,
	.tp_doc = "libetrace nfsdb filtered opens iterator",
	.tp_iter = PyObject_SelfIter,
	.tp_iternext = libetrace_nfsdb_filtered_opens_iter_next,
	.tp_new = libetrace_nfsdb_filtered_opens_iter_new,
};

typedef struct {
    PyObject_HEAD
    libetrace_nfsdb_object* nfsdb;
    unsigned long patch_size;
    struct command_filter** cflts;
    size_t* cflts_size;
    struct command_filter* fflts[1];
    size_t fflts_size[1];
    struct command_filter* fast_filter;
    Py_ssize_t filter_count;
    unsigned long command_index;
} libetrace_nfsdb_filtered_commands_iter_object;

void libetrace_nfsdb_filtered_commands_iter_dealloc(libetrace_nfsdb_filtered_commands_iter_object* self);
PyObject* libetrace_nfsdb_filtered_commands_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
Py_ssize_t libetrace_nfsdb_filtered_commands_iter_sq_length(PyObject* self);
PyObject* libetrace_nfsdb_filtered_commands_iter_next(PyObject *self);

static PySequenceMethods libetrace_nfsdbFilteredCommandsIter_sequence_methods = {
		.sq_length = libetrace_nfsdb_filtered_commands_iter_sq_length,
};

static PyTypeObject libetrace_nfsdbFilteredCommandsIterType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbFilteredCommandsIter",
	.tp_basicsize = sizeof(libetrace_nfsdbFilteredCommandsIterType),
	.tp_dealloc = (destructor)libetrace_nfsdb_filtered_commands_iter_dealloc,
	.tp_as_sequence = &libetrace_nfsdbFilteredCommandsIter_sequence_methods,
	.tp_doc = "libetrace nfsdb filtered commands iterator",
	.tp_iter = PyObject_SelfIter,
	.tp_iternext = libetrace_nfsdb_filtered_commands_iter_next,
	.tp_new = libetrace_nfsdb_filtered_commands_iter_new,
};

typedef struct {
    PyObject_HEAD
	unsigned long pid;
    unsigned long exeidx;
} libetrace_nfsdb_entry_eid_object;

void libetrace_nfsdb_entry_eid_dealloc(libetrace_nfsdb_entry_eid_object* self);
PyObject* libetrace_nfsdb_entry_eid_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libetrace_nfsdb_entry_eid_get_index(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_eid_repr(PyObject* self);
PyObject* libetrace_nfsdb_entry_eid_richcompare(PyObject *self, PyObject *other, int op);

static PyMemberDef libetrace_nfsdb_entry_eid_members[] = {
	{"pid",T_ULONG,offsetof(libetrace_nfsdb_entry_eid_object,pid),READONLY},
	{0},  /* Sentinel */
};

static PyGetSetDef libetrace_nfsdbEntryEid_getset[] = {
	{"index",libetrace_nfsdb_entry_eid_get_index,0,"nfsdb eid entry index value",0},
	{0,0,0,0,0},
};

static PyTypeObject libetrace_nfsdbEntryEidType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbEntryEid",
	.tp_basicsize = sizeof(libetrace_nfsdbEntryEidType),
	.tp_dealloc = (destructor)libetrace_nfsdb_entry_eid_dealloc,
	.tp_repr = (reprfunc)libetrace_nfsdb_entry_eid_repr,
	.tp_doc = "libetrace nfsdb entry extended id type",
	.tp_richcompare = libetrace_nfsdb_entry_eid_richcompare,
	.tp_members = libetrace_nfsdb_entry_eid_members,
	.tp_getset = libetrace_nfsdbEntryEid_getset,
	.tp_new = libetrace_nfsdb_entry_eid_new,
};

typedef struct {
    PyObject_HEAD
	unsigned long pid;
    unsigned long flags;
} libetrace_nfsdb_entry_cid_object;

void libetrace_nfsdb_entry_cid_dealloc(libetrace_nfsdb_entry_cid_object* self);
PyObject* libetrace_nfsdb_entry_cid_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libetrace_nfsdb_entry_cid_repr(PyObject* self);

static PyMemberDef libetrace_nfsdb_entry_cid_members[] = {
	{"pid",T_ULONG,offsetof(libetrace_nfsdb_entry_cid_object,pid),READONLY},
	{"flags",T_ULONG,offsetof(libetrace_nfsdb_entry_cid_object,flags),READONLY},
	{0},  /* Sentinel */
};

static PyTypeObject libetrace_nfsdbEntryCidType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbEntryCid",
	.tp_basicsize = sizeof(libetrace_nfsdbEntryCidType),
	.tp_dealloc = (destructor)libetrace_nfsdb_entry_cid_dealloc,
	.tp_repr = (reprfunc)libetrace_nfsdb_entry_cid_repr,
	.tp_doc = "libetrace nfsdb entry child id type",
	.tp_members = libetrace_nfsdb_entry_cid_members,
	.tp_new = libetrace_nfsdb_entry_cid_new,
};

typedef struct {
    PyObject_HEAD
	unsigned long path;
    unsigned long mode;
    unsigned long size;
    unsigned long original_path;
    const struct nfsdb* nfsdb;
    unsigned long parent; /* Parent nfsdb entry that contains this openfile */
    unsigned long index; /* Index of this openfile in the parent nfsdb entry */
    unsigned long opaque; /* Index to the nfsdb entry with particular opaque type (i.e. compilation or linking)
                             which references this openfile through its opaque type specific functionality
                             (i.e. reference to compiled file used by the compilation nfsdb entry) */
} libetrace_nfsdb_entry_openfile_object;

void libetrace_nfsdb_entry_openfile_dealloc(libetrace_nfsdb_entry_openfile_object* self);
PyObject* libetrace_nfsdb_entry_openfile_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libetrace_nfsdb_entry_openfile_json(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_repr(PyObject* self);
PyObject* libetrace_nfsdb_entry_openfile_get_path(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_openfile_get_original_path(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_openfile_get_parent(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_openfile_get_opaque(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_openfile_get_size(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_openfile_get_ptr(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_openfile_is_read(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_write(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_reg(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_dir(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_chr(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_block(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_symlink(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_fifo(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_socket(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_compiled(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_linked(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_linked_static(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_linked_shared(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_linked_exe(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_plain(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_binary(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_compiler(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_is_linker(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_exists(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_path_modified(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
PyObject* libetrace_nfsdb_entry_openfile_has_opaque_entry(libetrace_nfsdb_entry_openfile_object *self, PyObject *args);
Py_hash_t libetrace_nfsdb_entry_openfile_hash(PyObject *o);
PyObject* libetrace_nfsdb_entry_openfile_richcompare(PyObject *self, PyObject *other, int op);
libetrace_nfsdb_entry_openfile_object* libetrace_nfsdb_create_openfile_entry(const struct nfsdb* nfsdb,
		const struct nfsdb_entry* entry,unsigned long entry_index, unsigned long nfsdb_index);

static PyMemberDef libetrace_nfsdb_entry_openfile_members[] = {
	{"mode",T_ULONG,offsetof(libetrace_nfsdb_entry_openfile_object,mode),READONLY},
	{0},  /* Sentinel */
};

static PyGetSetDef libetrace_nfsdbEntryOpenfile_getset[] = {
	{"path",libetrace_nfsdb_entry_openfile_get_path,0,"nfsdb entry openfile path value",0},
	{"original_path",libetrace_nfsdb_entry_openfile_get_original_path,0,"nfsdb entry openfile original path value (the same as path if original path doesn't differ from resolved path)",0},
	{"parent",libetrace_nfsdb_entry_openfile_get_parent,0,"nfsdb entry that contains this openfile entry",0},
	{"opaque",libetrace_nfsdb_entry_openfile_get_opaque,0,"opaque nfsdb entry for this openfile",0},
	{"size",libetrace_nfsdb_entry_openfile_get_size,0,"Returns the size of the existing file (otherwise returns None)",0},
	{"ptr",libetrace_nfsdb_entry_openfile_get_ptr,0,"low level information about this openfile entry (tuple with nfsdb index and open index in the nfsdb entry)",0},
	{0,0,0,0,0},
};

static PyMethodDef libetrace_nfsdbEntryOpenfile_methods[] = {
	{"json",(PyCFunction)libetrace_nfsdb_entry_openfile_json,METH_VARARGS,"Returns the json representation of the openfile entry"},
	{"is_read",(PyCFunction)libetrace_nfsdb_entry_openfile_is_read,METH_VARARGS,"Returns true if the file was opened for read"},
	{"is_write",(PyCFunction)libetrace_nfsdb_entry_openfile_is_write,METH_VARARGS,"Returns true if the file was opened for write"},
	{"is_reg",(PyCFunction)libetrace_nfsdb_entry_openfile_is_reg,METH_VARARGS,"Returns true if the file is a regular file"},
	{"is_dir",(PyCFunction)libetrace_nfsdb_entry_openfile_is_dir,METH_VARARGS,"Returns true if the file entry is a directory"},
	{"is_chr",(PyCFunction)libetrace_nfsdb_entry_openfile_is_chr,METH_VARARGS,"Returns true if the file is a character device"},
	{"is_block",(PyCFunction)libetrace_nfsdb_entry_openfile_is_block,METH_VARARGS,"Returns true if the file is a block device"},
	{"is_symlink",(PyCFunction)libetrace_nfsdb_entry_openfile_is_symlink,METH_VARARGS,"Returns true if the file is a symbolic link"},
	{"is_fifo",(PyCFunction)libetrace_nfsdb_entry_openfile_is_fifo,METH_VARARGS,"Returns true if the file is a FIFO (named pipe)"},
	{"is_socket",(PyCFunction)libetrace_nfsdb_entry_openfile_is_socket,METH_VARARGS,"Returns true if the file is a socket"},
	{"is_compiled",(PyCFunction)libetrace_nfsdb_entry_openfile_is_compiled,METH_VARARGS,"Returns true if the file was compiled"},
	{"is_linked",(PyCFunction)libetrace_nfsdb_entry_openfile_is_linked,METH_VARARGS,"Returns true if the file was linked"},
	{"is_linked_static",(PyCFunction)libetrace_nfsdb_entry_openfile_is_linked_static,METH_VARARGS,"Returns true if the file was a archived static library"},
	{"is_linked_shared",(PyCFunction)libetrace_nfsdb_entry_openfile_is_linked_shared,METH_VARARGS,"Returns true if the file was a linked shared library"},
	{"is_linked_exe",(PyCFunction)libetrace_nfsdb_entry_openfile_is_linked_exe,METH_VARARGS,"Returns true if the file was linked executable"},
	{"is_plain",(PyCFunction)libetrace_nfsdb_entry_openfile_is_plain,METH_VARARGS,"Returns true if the file is a plain file (not compiled nor linked nor binary nor symlink)"},
	{"is_binary",(PyCFunction)libetrace_nfsdb_entry_openfile_is_binary,METH_VARARGS,"Returns true if the file is an executed binary"},
	{"is_compiler",(PyCFunction)libetrace_nfsdb_entry_openfile_is_compiler,METH_VARARGS,"Returns true if the file is a compiler"},
	{"is_linker",(PyCFunction)libetrace_nfsdb_entry_openfile_is_linker,METH_VARARGS,"Returns true if the file is a linker"},
	{"exists",(PyCFunction)libetrace_nfsdb_entry_openfile_exists,METH_VARARGS,"Returns true if the file exists after the build"},
	{"path_modified",(PyCFunction)libetrace_nfsdb_entry_openfile_path_modified,METH_VARARGS,"Returns true if the original and resolved paths differ"},
	{"has_opaque_entry",(PyCFunction)libetrace_nfsdb_entry_openfile_has_opaque_entry,METH_VARARGS,"Returns true if the openfile has the opaque entry"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject libetrace_nfsdbEntryOpenfileType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbEntryOpenfile",
	.tp_basicsize = sizeof(libetrace_nfsdbEntryOpenfileType),
	.tp_dealloc = (destructor)libetrace_nfsdb_entry_openfile_dealloc,
	.tp_repr = (reprfunc)libetrace_nfsdb_entry_openfile_repr,
	.tp_hash = (hashfunc)libetrace_nfsdb_entry_openfile_hash,
	.tp_doc = "libetrace nfsdb entry openfile type",
	.tp_richcompare = libetrace_nfsdb_entry_openfile_richcompare,
	.tp_methods = libetrace_nfsdbEntryOpenfile_methods,
	.tp_members = libetrace_nfsdb_entry_openfile_members,
	.tp_getset = libetrace_nfsdbEntryOpenfile_getset,
	.tp_new = libetrace_nfsdb_entry_openfile_new,
};

typedef struct {
    PyObject_HEAD
	struct compilation_info* ci;
    const struct nfsdb* nfsdb;
    unsigned long parent; /* Parent nfsdb entry that contains this compilation info */
} libetrace_nfsdb_entry_compilation_info;

void libetrace_nfsdb_entry_compilation_info_dealloc(libetrace_nfsdb_entry_compilation_info* self);
PyObject* libetrace_nfsdb_entry_compilation_info_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libetrace_nfsdb_entry_compilation_info_get_compiled_files(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_compiled_file_paths(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_object_files(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_object_file_paths(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_headers(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_header_paths(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_include_paths(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_type(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_defines(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_get_undefs(PyObject* self, void* closure);
PyObject* libetrace_nfsdb_entry_compilation_info_is_integrated(libetrace_nfsdb_entry_compilation_info *self, PyObject *args);

static PyMemberDef libetrace_nfsdb_entry_compilation_info_members[] = {
	{0},  /* Sentinel */
};

static PyGetSetDef libetrace_nfsdbEntryCompilationInfo_getset[] = {
	{"file_paths",libetrace_nfsdb_entry_compilation_info_get_compiled_file_paths,0,"list of compiled file paths from the compilation info object",0},
	{"files",libetrace_nfsdb_entry_compilation_info_get_compiled_files,0,"list of compiled files from the compilation info object",0},
	{"object_paths",libetrace_nfsdb_entry_compilation_info_get_object_file_paths,0,"list of object file paths produced by this compilation",0},
	{"objects",libetrace_nfsdb_entry_compilation_info_get_object_files,0,"list of object files produced by this compilation",0},
	{"header_paths",libetrace_nfsdb_entry_compilation_info_get_header_paths,0,"list of file paths included at the command line by this compilation",0},
	{"headers",libetrace_nfsdb_entry_compilation_info_get_headers,0,"list of files included at the command line by this compilation",0},
	{"ipaths",libetrace_nfsdb_entry_compilation_info_get_include_paths,0,"list of include paths used by this compilation",0},
	{"type",libetrace_nfsdb_entry_compilation_info_get_type,0,"source type used by this compilation",0},
	{"defs",libetrace_nfsdb_entry_compilation_info_get_defines,0,"list of internal or command line preprocessor definitions used by this compilation",0},
	{"undefs",libetrace_nfsdb_entry_compilation_info_get_undefs,0,"list of command lien preprocessor undefs used by this compilation",0},
	{0,0,0,0,0},
};

static PyMethodDef libetrace_nfsdbEntryCompilationInfo_methods[] = {
	{"is_integrated",(PyCFunction)libetrace_nfsdb_entry_compilation_info_is_integrated,METH_VARARGS,"Returns true if the driver invocation is in the same process as an actual compilation (integrated compiler)"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

static PyTypeObject libetrace_nfsdbEntryCompilationInfoType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.nfsdbEntryCompilationInfo",
	.tp_basicsize = sizeof(libetrace_nfsdbEntryCompilationInfoType),
	.tp_dealloc = (destructor)libetrace_nfsdb_entry_compilation_info_dealloc,
	.tp_doc = "libetrace nfsdb entry compilation info type",
	.tp_methods = libetrace_nfsdbEntryCompilationInfo_methods,
	.tp_members = libetrace_nfsdb_entry_compilation_info_members,
	.tp_getset = libetrace_nfsdbEntryCompilationInfo_getset,
	.tp_new = libetrace_nfsdb_entry_compilation_info_new,
};

typedef struct {
    PyObject_HEAD
    const struct nfsdb* nfsdb;
} libetrace_nfsdb_filemap;

void libetrace_nfsdb_filemap_dealloc(libetrace_nfsdb_filemap* self);
PyObject* libetrace_nfsdb_filemap_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds);
PyObject* libetrace_nfsdb_filemap_mp_subscript(PyObject* self, PyObject* slice);

static PyMemberDef libetrace_nfsdb_filemap_members[] = {
	{0},  /* Sentinel */
};

static PyGetSetDef libetrace_nfsdb_filemap_getset[] = {
	{0,0,0,0,0},
};

static PyMappingMethods libetrace_nfsdb_filemap_mapping_methods = {
		.mp_subscript = libetrace_nfsdb_filemap_mp_subscript,
};

static PyTypeObject libetrace_nfsdbFileMapType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "libetrace.fileMapObject",
	.tp_basicsize = sizeof(libetrace_nfsdbFileMapType),
	.tp_dealloc = (destructor)libetrace_nfsdb_filemap_dealloc,
	.tp_as_mapping = &libetrace_nfsdb_filemap_mapping_methods,
	.tp_doc = "libetrace nfsdb filemap type",
	.tp_members = libetrace_nfsdb_filemap_members,
	.tp_getset = libetrace_nfsdb_filemap_getset,
	.tp_new = libetrace_nfsdb_filemap_new,
};

#endif /* __PYETRACE_H_ */
