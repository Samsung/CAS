#ifndef __COMPILER_H__
#define __COMPILER_H__

#include <Python.h>
#include "utils.h"

/* The returned value is allocated and need to be freed by PYASSTR_DECREF otherwise memory leak will happen */
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

#define DEFINE_COMPILER_TYPE(name) \
static PyTypeObject libetrace_compiler_##name##Type = { \
    PyVarObject_HEAD_INIT(NULL, 0)	\
    "libetrace_compiler."#name,                      	/*tp_name*/	\
    sizeof(libetrace_compiler_##name##Type),        	/*tp_basicsize*/	\
    0,                                              	/*tp_itemsize*/	\
    (destructor)libetrace_compiler_##name##_dealloc, 	/*tp_dealloc*/	\
	0,                              					/* tp_vectorcall_offset */	\
	0,                              					/* tp_getattr */	\
	0,                              					/* tp_setattr */	\
	0,                              					/* tp_as_async */	\
	(reprfunc)libetrace_compiler_##name##_repr,			/* tp_repr */	\
	&libetrace_compiler_##name##_number_methods,      	/* tp_as_number */	\
	&libetrace_compiler_##name##_sequence_methods,   	/* tp_as_sequence */	\
	&libetrace_compiler_##name##_mapping_methods,	 	/* tp_as_mapping */	\
	0,                              					/* tp_hash */	\
	0,                              					/* tp_call */	\
	0,                              					/* tp_str */	\
	0,                              					/* tp_getattro */	\
	0,                              					/* tp_setattro */	\
	0,                              					/* tp_as_buffer */	\
	Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,			/* tp_flags */	\
	"libetrace "#name" compiler object",               	/* tp_doc */ \
	0,                              					/* tp_traverse */	\
	0,                              					/* tp_clear */	\
	0,                              					/* tp_richcompare */	\
	0,                              					/* tp_weaklistoffset */	\
	0,						           					/* tp_iter */	\
	0,                              					/* tp_iternext */	\
	libetrace_compiler_##name##_methods,               	/* tp_methods */ \
	libetrace_compiler_##name##_members,               	/* tp_members */ \
	libetrace_compiler_##name##_getset,  	     		/* tp_getset */	\
	0,                              					/* tp_base */	\
	0,                              					/* tp_dict */	\
	0,                              					/* tp_descr_get */	\
	0,                              					/* tp_descr_set */	\
	0,                              					/* tp_dictoffset */	\
    (initproc)libetrace_compiler_##name##_init,         /* tp_init */ \
	0,                              					/* tp_alloc */	\
	libetrace_compiler_##name##_new,                   	/* tp_new */ \
}

#define DEFINE_COMPILER_OBJECT(name) \
\
typedef struct { \
    PyObject_HEAD \
    int verbose; \
    int debug; \
    int debug_compilations; \
} libetrace_compiler_##name##_object

#define DEFINE_COMPILER(name) \
\
DEFINE_COMPILER_OBJECT(name); \
\
PyObject* libetrace_compiler_##name##_get_compilations(libetrace_compiler_##name##_object* self, PyObject* args, PyObject* kwargs ); \
PyObject* libetrace_compiler_##name##_repr(PyObject* self);	\
 \
static PyObject* libetrace_compiler_##name##_new(PyTypeObject* subtype, PyObject* args, PyObject* kwds) { \
 \
	libetrace_compiler_##name##_object* self; \
    self = (libetrace_compiler_##name##_object*)subtype->tp_alloc(subtype, 0); \
    return (PyObject *)self; \
} \
 \
static void libetrace_compiler_##name##_dealloc(libetrace_compiler_##name##_object* self) { \
 \
 	PyTypeObject *tp = Py_TYPE(self);	\
 	tp->tp_free(self);	\
} \
 \
static int libetrace_compiler_##name##_init(libetrace_compiler_##name##_object* self, PyObject* args, PyObject* kwds) { \
 \
	return PyArg_ParseTuple(args,"iii",&self->verbose,&self->debug,&self->debug_compilations); \
} \
 \
\
static PyMemberDef libetrace_compiler_##name##_members[] = { \
        {(char*)"verbose", T_INT, offsetof(libetrace_compiler_##name##_object, verbose), 0, (char*)"Verbalize action"}, \
        {(char*)"debug", T_INT, offsetof(libetrace_compiler_##name##_object, debug), 0, (char*)"Print debug information"}, \
		{(char*)"debug_compilations", T_INT, offsetof(libetrace_compiler_##name##_object, debug_compilations), 0, (char*)"More verbose debug messages regarding compilations"}, \
        {0}  /* Sentinel */ \
}; \
\
static PyMethodDef libetrace_compiler_##name##_methods[] = { \
        {"get_compilations", (PyCFunction)libetrace_compiler_##name##_get_compilations,METH_VARARGS|METH_KEYWORDS,"Returns list of input files for each compilation"}, \
        {0}  /* Sentinel */ \
}; \
\
static PyGetSetDef libetrace_compiler_##name##_getset[] = {	\
	{0,0,0,0,0},	\
};	\
\
static PyNumberMethods libetrace_compiler_##name##_number_methods = {	\
};	\
\
static PySequenceMethods libetrace_compiler_##name##_sequence_methods = {	\
};	\
\
static PyMappingMethods libetrace_compiler_##name##_mapping_methods = {	\
};	\
DEFINE_COMPILER_TYPE(name)

#define DEFINE_COMPILER_GET_COMPILATIONS(name)\
	PyObject* libetrace_compiler_##name##_get_compilations(libetrace_compiler_##name##_object* self, PyObject* args, PyObject* kwargs )

#define DEFINE_COMPILER_REPR(name)\
	PyObject* libetrace_compiler_##name##_repr(PyObject* self)

#define COMPILER_READY(name)\
	if (PyType_Ready(&libetrace_compiler_##name##Type) < 0) \
		return 0

#define COMPILER_REGISTER(name)\
	Py_INCREF(&libetrace_compiler_##name##Type);\
	PyModule_AddObject(m, #name, (PyObject *)&libetrace_compiler_##name##Type);

#define DEBUG_GCC_INPUT_FILES   0
#define DEBUG_COMMANDS  0
#define PRINT_PROGRESS   1
#define DEBUG_CLANG_COMPILERS 0

#define STRJOIN(l)				(PyString_get_c_str(PyUnicode_Join(PyUnicode_FromString(" "),l)))
#define REPR(o)					(PyString_get_c_str(PyObject_Str(o)))

enum compiler_phase {
    PP_SEEN = 1,
    LD_SEEN = 2,
};

extern volatile int interrupt;
extern void intHandler(int v);

extern unsigned long progress_max_g;
extern unsigned long progress_g;

static inline void update_progress(unsigned long i) {

    unsigned long progress = (unsigned long)((((double)i)/progress_max_g)*100);

    if (progress>progress_g) {
        progress_g = progress;
        printf("\rDone: %lu%%",progress);
        fflush(stdout);
    }
}

struct buffer {
    char* data;
    size_t size;
    size_t alloc_size;
};

static inline void buffer_init(struct buffer* buff, size_t init_size) {
    buff->data = malloc(init_size);
    assert(buff->data && "Out of memory");
    buff->size = 0;
    buff->alloc_size = init_size;
}

static inline void buffer_destroy(struct buffer* buff) {
    free(buff->data);
}

static inline size_t buffer_size(struct buffer* buff) {
    return buff->size;
}

static inline void buffer_resize(struct buffer* buff, size_t new_size) {
    buff->data = realloc(buff->data,new_size);
    assert(buff->data && "Out of memory");
    buff->alloc_size = new_size;
}

static inline char* buffer_alloc(struct buffer* buff, size_t size) {
    if (buff->size+size>buff->alloc_size) {
        buffer_resize(buff,2*(buff->size+size));
    }
    return buff->data+buff->size;
}

static inline char* buffer_advance(struct buffer* buff, size_t size) {
    buff->size = buff->size+size;
    return buff->data+buff->size;
}

static inline void buffer_clear(struct buffer* buff) {
    buff->size = 0;
}

static inline void buffer_append(struct buffer* buff, const unsigned char* data, size_t data_size) {

    if (buff->size+data_size>buff->alloc_size) {
        buffer_resize(buff,2*(buff->size+data_size));
    }
    memcpy(buff->data+buff->size,data,data_size);
	buff->size = buff->size+data_size;
}

static inline void buffer_prints(struct buffer* buff) {
    size_t u;
    for (u=0; u<buff->size; ++u) printf("%c",buff->data[u]);
}

struct job {
    int index;
    int pid;
    struct pollfd* pfd;
    struct buffer buff;
    int input;
    PyObject* comp;
    PyObject* fns;
};

static inline const char *get_filename_ext(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if(!dot || dot == filename) return "";
    return dot + 1;
}

char * strnstr(const char *s, const char *find, size_t slen);
int run_child(const char* bin, const char* cwd, PyObject* args, int* pfd, int child_read_fd);
PyObject* splitn(const char* str, size_t size);
PyObject* check_file_access(PyObject* fns, const char* cwd);
int strtab_contains(const char** tab,size_t num,char* s);
Py_ssize_t PySequence_Contains_Prefix(PyObject* seq,PyObject* prefix,int* match);
int rmrf(char *path);

#endif /* __COMPILER_H__ */

