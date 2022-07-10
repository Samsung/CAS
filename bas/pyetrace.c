#include "pyetrace.h"
#include "libflat.h"
#include "compiler.h"

#include <fcntl.h>
#include <stdbool.h>

DEFINE_COMPILER(gcc);
DEFINE_COMPILER(clang);
DEFINE_COMPILER(armcc);

volatile int interrupt;
void intHandler(int v) {
	interrupt = 1;
}

nfsdb_entry_filter_func_array_t nfsdb_entry_filter_func_array[] = {
		{"has_comp_info",libetrace_nfsdb_entry_filter_has_comp_info},
		{"has_linked_file",libetrace_nfsdb_entry_filter_has_linked_file},
		{"has_command",libetrace_nfsdb_entry_filter_has_command},
};

// TODO: memory leaks in compilers

PyObject *libetrace_nfsdbError;

static int is_LLVM_BC(const char* buffer, const size_t size) {
	if(size < 20)
		return false;
	return !memcmp(buffer, "\xde\xc0\x43\x42", 4);
}

static int is_ELF(const char* header, const size_t size) {
	if(size < 64)
		return false;

	if(!memcmp(header, "\x46\x4c\x45\x7f", 4))
		return false;

	if (header[4] == 1) {
		if (header[5] == 1) {
			if ((*(uint16_t*)(&header[0x28]))!=52) {
				goto err;
			}
		}
		else if (header[5] == 2) {
			if ((((uint16_t)header[0x29] << 8) | ((uint16_t)header[0x28])) != 52) {
				goto err;
			}
		}
		else {
			goto err;
		}
	}
	else if (header[4] == 2) {
		if (header[5] == 1) {
			if ((*(uint16_t*)(&header[0x34])) != 64) {
				goto err;
			}
		}
		else if (header[5]==2) {
			if ((((uint16_t)header[0x35] << 8) | ((uint16_t)header[0x34])) != 64) {
				goto err;
			}
		}
		else {
			goto err;
		}
	}
	else {
		goto err;
	}

	if (header[6] != 1 || header[7] > 0x11)
		return false;
	return true;

err:
	return false;
}

PyObject * libetrace_is_LLVM_BC_file(PyObject *self, PyObject *args) {

	const char* key;

	if (!PyArg_ParseTuple(args, "s", &key)) {
		PyErr_SetString(PyExc_TypeError, "Invalid argument for LLVM BC file check");
		return 0;
	}

	FILE* f = fopen(key, "r");
	if (!f) {
		Py_RETURN_FALSE;
	}

	unsigned char header[20];
	if (fread(header, 1, 20, f) != 20) {
		goto err;
	}

	if(!is_LLVM_BC((const char*)header, 20))
		goto err;

	fclose(f);
	Py_RETURN_TRUE;

err:
	fclose(f);
	Py_RETURN_FALSE;
}

PyObject * libetrace_is_ELF_file(PyObject *self, PyObject *args) {

	const char* key;

	if (!PyArg_ParseTuple(args,"s",&key)) {
		PyErr_SetString(PyExc_TypeError,"Invalid argument for ELF file check");
		return 0;
	}

	FILE* f = fopen(key,"r");
	if (!f) {
		Py_RETURN_FALSE;
	}

	unsigned char header[65];
	if (fread(header, 1, 64, f) != 64) {
		goto err;
	}

	if(!is_ELF((const char*)header, 64))
		goto err;

	fclose(f);
	Py_RETURN_TRUE;

err:
	fclose(f);
	Py_RETURN_FALSE;
}

PyObject * libetrace_pytools_is_ELF_or_LLVM_BC_file(PyObject *self, PyObject *args) {

	const char* key;

	if (!PyArg_ParseTuple(args,"s",&key)) {
		PyErr_SetString(PyExc_TypeError,"Invalid argument for ELF_or_LLVM_BC file check");
		return 0;
	}

	FILE* f = fopen(key,"r");
	if (!f) {
		Py_RETURN_FALSE;
	}

	unsigned char header[65];
	if (fread(header, 1, 64, f) != 64) {
		goto err;
	}

	if(!is_LLVM_BC((const char*)header, 64) && !is_ELF((const char*)header, 64))
		goto err;

	fclose(f);
	Py_RETURN_TRUE;

err:
	fclose(f);
	Py_RETURN_FALSE;
}

static int pattern_match_single(const char* p, const char* exclude_pattern) {

	if (!fnmatch(exclude_pattern,p,0)) {
		return 1;
	}
	return 0;
}

PyObject * libetrace_precompute_command_patterns(PyObject *self, PyObject *args, PyObject* kwargs) {

	int debug = 0;
	PyObject* py_debug = PyUnicode_FromString("debug");

	if (kwargs) {
		if (PyDict_Contains(kwargs, py_debug)) {
			PyObject* debugv = PyDict_GetItem(kwargs,py_debug);
			debug = PyObject_IsTrue(debugv);
		}
	}

	Py_DecRef(py_debug);

	DBG(debug,"--- libetrace_precompute_command_patterns()\n");

    struct sigaction act;
    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, 0);

    const char** excl_commands = 0;
    size_t excl_commands_size = 0;
    PyObject* exclude_commands = PyTuple_GetItem(args,0);
    PyObject* pm = PyTuple_GetItem(args,1);

    if (exclude_commands && (PyList_Size(exclude_commands)>0)) {
		excl_commands = malloc(PyList_Size(exclude_commands)*sizeof(const char*));
		excl_commands_size = PyList_Size(exclude_commands);
		int u;
		for (u=0; u<PyList_Size(exclude_commands); ++u) {
			excl_commands[u] = PyString_get_c_str(PyList_GetItem(exclude_commands,u));
			DBG(debug,"        exclude command pattern: %s\n",excl_commands[u]);
		}
	}

    DBG(1,"--- Computing number of commands to process...\n");
    unsigned long cmd_count = 0;
    PyObject* vL = PyDict_Values(pm);
    for (Py_ssize_t u=0; u<PyList_Size(vL); ++u) {
    	cmd_count+=PyList_Size(PyList_GetItem(vL,u));
    }
    Py_DecRef(vL);

    DBG(1,"--- Number of commands: %lu, patterns: %lu\n",cmd_count,excl_commands_size);
    for (size_t i=0; i<excl_commands_size; ++i) {
    	DBG(1,"--- PATTERN[%zu]:  %s\n",i,excl_commands[i]);
    }

    if (excl_commands_size<=0) {
    	// Nothing to do
    	Py_RETURN_NONE;
    }

    PyObject* pcp_map = PyDict_New();

    DBG(1,"Precomputing exclude command patterns...");

    unsigned long cmdi = 0;
    PyObject* keys = PyDict_Keys(pm);
    PyObject* vkey = PyUnicode_FromString("v");
    for (Py_ssize_t u=0; u<PyList_Size(keys); ++u) {
    	long pid = PyLong_AsLong(PyList_GetItem(keys,u));
		PyObject* eL = PyDict_GetItem(pm, PyList_GetItem(keys,u));
		for (Py_ssize_t v=0; v<PyList_Size(eL); ++v) {
			PyObject* e = PyList_GetItem(eL,v);
			PyObject* cmdv = PyDict_GetItem(e,vkey);
			PyObject* cmds = PyUnicode_Join(PyUnicode_FromString(" "),cmdv);
			cmdi++;
			if ((cmdi%1000)==0) {
				DBG(1,"\rPrecomputing exclude command patterns...%lu%%",(cmdi*100)/cmd_count);
			}
			size_t bsize = (excl_commands_size-1)/8+1;
			unsigned char* b = calloc(bsize,1);
			assert(b!=0 && "Out of memory for allocating precompute pattern map");
			size_t k;
			for (k=0; k<excl_commands_size; ++k) {
				unsigned byte_index = k/8;
				unsigned bit_index = k%8;
				if (pattern_match_single(PyString_get_c_str(cmds),excl_commands[k])) {
					b[byte_index]|=0x01<<(7-bit_index);
				}
			}
			Py_DecRef(cmds);
			PyObject* pidext = PyTuple_New(2);
			PyTuple_SetItem(pidext, 0,Py_BuildValue("l",pid));
			PyTuple_SetItem(pidext, 1,Py_BuildValue("l",v));
			PyDict_SetItem(pcp_map, pidext, PyBytes_FromStringAndSize((const char*)b,bsize));
			free(b);
		}
		if (interrupt) {
			goto interrupted;
		}
	}
    DBG(1,"\n");
	Py_DecRef(keys);
    Py_DecRef(vkey);
	free(excl_commands);
	return pcp_map;
interrupted:
	Py_DecRef(keys);
    Py_DecRef(vkey);
	free(excl_commands);
	Py_DecRef(pcp_map);
	Py_RETURN_NONE;
}

PyObject * libetrace_parse_compiler_triple_hash(PyObject *self, PyObject *args) {

	const char* s;
	if (!PyArg_ParseTuple(args,"s",&s)) {
		PyErr_SetString(PyExc_TypeError,"Invalid argument for parse_compiler_triple_hash");
		return 0;
	}

	PyObject* argv = PyList_New(0);

	int in_quote=0;
	const char* p = s;
	while(*p) {
		if (*p=='\\') {
			p+=2;
			continue;
		}
		if (*p=='"') {
			in_quote = 1-in_quote;
		}
		if (isspace(*p)) {
			if (!in_quote) {
				if (p-s>1) {
					PyList_Append(argv,PyUnicode_FromStringAndSize(s,p-s));
				}
				s = p+1;
			}
		}
		p++;
	}
	if (p>s) {
		PyList_Append(argv,PyUnicode_FromStringAndSize(s,p-s));
	}

	return argv;
}

const char* joinpath(const char* cwd, const char* path) {

	if (path[0]==0) {
		return "";
	}
	if (path[0]=='/') {
		return normpath(path);
	}
	else {
		char* new = malloc(strlen(cwd)+1+strlen(path)+1);
		size_t cwdL = strlen(cwd);
		size_t pathL = strlen(path);
		memcpy(new,cwd,cwdL);
		new[cwdL] = '/';
		memcpy(new+cwdL+1,path,pathL);
		new[cwdL+1+pathL] = 0;
		const char* r = normpath(new);
		free(new);
		return r;
	}
}

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(eid);
FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(cid);
FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(pp_def);
FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(nfsdb_entry_file_index);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(openfile,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,original_path,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(compilation_info,
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,compiled_list,ATTR(compiled_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(nfsdb_entry_file_index,compiled_index,ATTR(compiled_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,include_paths,ATTR(include_paths_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(pp_def,pp_defs,ATTR(pp_defs_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(pp_def,pp_udefs,ATTR(pp_udefs_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,header_list,ATTR(header_list_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(nfsdb_entry_file_index,header_index,ATTR(header_list_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,object_list,ATTR(object_list_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(nfsdb_entry_file_index,object_index,ATTR(object_list_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(nfsdb_entry,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(cid,child_ids,ATTR(child_ids_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,argv,ATTR(argv_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(openfile,open_files,ATTR(open_files_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned char,pcp,ATTR(pcp_count));
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(eid,pipe_eids,ATTR(pipe_eids_count));
	AGGREGATE_FLATTEN_STRUCT2_ITER(compilation_info,compilation_info);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,linked_file,1);
);

FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node);

static inline const struct nfsdb_entryMap_node* nfsdb_entryMap_remove_color(const struct nfsdb_entryMap_node* ptr) {
	return (const struct nfsdb_entryMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* nfsdb_entryMap_add_color(struct flatten_pointer* fptr, const struct nfsdb_entryMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER_ITER(nfsdb_entryMap_node,node.__rb_parent_color,
			nfsdb_entryMap_remove_color,nfsdb_entryMap_add_color);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node,node.rb_left);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(struct nfsdb_entry*,entry_list,ATTR(entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(entry_list),ATTR(entry_count),
		FLATTEN_STRUCT2_ITER(nfsdb_entry,entry);
	);
);

FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(ulongMap_node);

static inline const struct ulongMap_node* ulongMap_remove_color(const struct ulongMap_node* ptr) {
	return (const struct ulongMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* ulongMap_add_color(struct flatten_pointer* fptr, const struct ulongMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(ulongMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER_ITER(ulongMap_node,node.__rb_parent_color,
			ulongMap_remove_color,ulongMap_add_color);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,node.rb_left);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,value_list,ATTR(value_count));
);

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

FUNCTION_DECLARE_FLATTEN_STRUCT2_ITER(nfsdb_fileMap_node);

static inline const struct nfsdb_fileMap_node* nfsdb_fileMap_remove_color(const struct nfsdb_fileMap_node* ptr) {
	return (const struct nfsdb_fileMap_node*)( (uintptr_t)ptr & ~3 );
}

static inline struct flatten_pointer* nfsdb_fileMap_add_color(struct flatten_pointer* fptr, const struct nfsdb_fileMap_node* ptr) {
	fptr->offset |= (size_t)((uintptr_t)ptr & 3);
	return fptr;
}

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(nfsdb_fileMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT2_MIXED_POINTER_ITER(nfsdb_fileMap_node,node.__rb_parent_color,
			nfsdb_fileMap_remove_color,nfsdb_fileMap_add_color);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_fileMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_fileMap_node,node.rb_left);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(struct nfsdb_entry*,rd_entry_list,ATTR(rd_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(rd_entry_list),ATTR(rd_entry_count),
		FLATTEN_STRUCT2_ITER(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,rd_entry_index,ATTR(rd_entry_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(struct nfsdb_entry*,wr_entry_list,ATTR(wr_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(wr_entry_list),ATTR(wr_entry_count),
		FLATTEN_STRUCT2_ITER(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,wr_entry_index,ATTR(wr_entry_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(struct nfsdb_entry*,rw_entry_list,ATTR(rw_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(rw_entry_list),ATTR(rw_entry_count),
		FLATTEN_STRUCT2_ITER(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(unsigned long,rw_entry_index,ATTR(rw_entry_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(nfsdb,
	AGGREGATE_FLATTEN_STRUCT2_ARRAY_ITER(nfsdb_entry,nfsdb,ATTR(nfsdb_count));
	AGGREGATE_FLATTEN_STRING2(source_root);
	AGGREGATE_FLATTEN_STRING2(dbversion);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,string_table,ATTR(string_count));
	FOREACH_POINTER(const char*,s,ATTR(string_table),ATTR(string_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_TYPE2_ARRAY(uint32_t,string_size_table,ATTR(string_count));
	AGGREGATE_FLATTEN_TYPE2_ARRAY(const char*,pcp_pattern_list,ATTR(pcp_pattern_list_size));
	FOREACH_POINTER(const char*,s,ATTR(pcp_pattern_list),ATTR(pcp_pattern_list_size),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node,procmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node,bmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,forkmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,revforkmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,pipemap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,wrmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,rdmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(stringRefMap_node,revstringmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_fileMap_node,filemap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(nfsdb_entryMap_node,linkedmap.rb_node);
);

FUNCTION_DEFINE_FLATTEN_STRUCT2_ITER(nfsdb_deps,
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,depmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT2_ITER(ulongMap_node,ddepmap.rb_node);
);

unsigned long string_table_add(struct nfsdb* nfsdb, PyObject* s, PyObject* stringMap) {

	static unsigned long stringIndex = 0;

	if (PyDict_Contains(stringMap, s)) {
		return PyLong_AsUnsignedLong(PyDict_GetItem(stringMap, s));
	}
	else {
		PyDict_SetItem(stringMap, s, PyLong_FromUnsignedLong(stringIndex));
		if (nfsdb->string_count>=nfsdb->string_table_size) {
			const char** n = realloc(nfsdb->string_table,((nfsdb->string_table_size*3)/2)*sizeof(const char*));
			assert(n);
			nfsdb->string_table = n;
			uint32_t* m = realloc(nfsdb->string_size_table,((nfsdb->string_table_size*3)/2)*sizeof(uint32_t));
			assert(m);
			nfsdb->string_size_table = m;
			nfsdb->string_table_size = ((nfsdb->string_table_size*3)/2);
		}
		if (PyUnicode_Check(s)) {
			nfsdb->string_table[stringIndex] = PyString_get_c_str(s);
			nfsdb->string_size_table[stringIndex] = strlen(nfsdb->string_table[stringIndex]);
		}
		else if (PyUnicode_Check(s)) {
			PyObject* us = PyUnicode_AsASCIIString(s);
			if (us) {
				nfsdb->string_table[stringIndex] = PyString_get_c_str(us);
				nfsdb->string_size_table[stringIndex] = strlen(nfsdb->string_table[stringIndex]);
			}
			else {
				/* Failed to decode an Unicode object */
				nfsdb->string_table[stringIndex] = "";
				nfsdb->string_size_table[stringIndex] = 0;
			}
		}
		else {
			assert(0);
		}
		nfsdb->string_count++;
		return stringIndex++;
	}

}

static inline int match_openfile_path(struct openfile* openfile, unsigned long fhandle) {

	if (openfile->path==fhandle) {
		return 1;
	}
	else {
		if ((openfile->original_path) && (*openfile->original_path==fhandle)) {
			return 1;
		}
	}

	return 0;
}

static void destroy_nfsdb(struct nfsdb* nfsdb) {

	// TODO
}

typedef int (*scan_openfile_cb)(struct nfsdb_entry* root_entry, const struct nfsdb_entry* open_entry,
		unsigned long open_index, const void* arg);

static int scan_openfile_list_with_children(const struct nfsdb* nfsdb, const struct nfsdb_entry* entry,
		scan_openfile_cb cb, struct nfsdb_entry* root_entry, const void* arg) {

	for (unsigned long i=0; i<entry->open_files_count; ++i) {
		if (cb(root_entry,entry,i,arg)) return 1;
	}

	for (unsigned long i=0; i<entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->child_ids[i].pid);
		for (unsigned long j=0; j<node->entry_count; ++j) {
			struct nfsdb_entry* child_entry = node->entry_list[j];
			if (scan_openfile_list_with_children(nfsdb,child_entry,cb,root_entry,arg)) return 1;
		}
	}

	return 0;
}

static int scan_wr_openfile_list_with_children(const struct nfsdb* nfsdb, const struct nfsdb_entry* entry,
		scan_openfile_cb cb, struct nfsdb_entry* root_entry, const void* arg) {

	for (unsigned long i=0; i<entry->open_files_count; ++i) {
		struct openfile* openfile = &entry->open_files[i];
		if ((openfile->mode&0x03)>0) {
			if (cb(root_entry,entry,i,arg)) return 1;
		}
	}

	for (unsigned long i=0; i<entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->child_ids[i].pid);
		for (unsigned long j=0; j<node->entry_count; ++j) {
			struct nfsdb_entry* child_entry = node->entry_list[j];
			if (scan_wr_openfile_list_with_children(nfsdb,child_entry,cb,root_entry,arg)) return 1;
		}
	}

	return 0;
}

static int scan_openfile_precompute_linked_file(struct nfsdb_entry* entry, const struct nfsdb_entry* open_entry,
		unsigned long open_index, const void* arg) {

	struct openfile* openfile = &open_entry->open_files[open_index];

	if (match_openfile_path(openfile,*entry->linked_file)) {
		entry->linked_index.nfsdb_index = open_entry->nfsdb_index;
		entry->linked_index.open_index = open_index;
		return 1;
	}

	return 0;
}

void libetrace_nfsdb_entry_precompute_linked_file(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		scan_wr_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_linked_file,entry,0);
	}
}

static int scan_openfile_precompute_compiled_file(struct nfsdb_entry* entry, const struct nfsdb_entry* open_entry,
		unsigned long open_index, const void* arg) {

	unsigned long* compiled_index = (unsigned long*)arg;
	struct openfile* openfile = &open_entry->open_files[open_index];

	unsigned long compiled_handle = entry->compilation_info->compiled_list[*compiled_index];
	if (match_openfile_path(openfile,compiled_handle)) {
		entry->compilation_info->compiled_index[*compiled_index].nfsdb_index = open_entry->nfsdb_index;
		entry->compilation_info->compiled_index[*compiled_index].open_index = open_index;
		return 1;
	}

	return 0;
}

static int scan_openfile_precompute_header_file(struct nfsdb_entry* entry, const struct nfsdb_entry* open_entry,
		unsigned long open_index, const void* arg) {

	unsigned long* compiled_index = (unsigned long*)arg;
	struct openfile* openfile = &open_entry->open_files[open_index];

	unsigned long header_handle = entry->compilation_info->header_list[*compiled_index];
	if (match_openfile_path(openfile,header_handle)) {
		entry->compilation_info->header_index[*compiled_index].nfsdb_index = open_entry->nfsdb_index;
		entry->compilation_info->header_index[*compiled_index].open_index = open_index;
		return 1;
	}

	return 0;
}

static int scan_openfile_precompute_object_file(struct nfsdb_entry* entry, const struct nfsdb_entry* open_entry,
		unsigned long open_index, const void* arg) {

	unsigned long* compiled_index = (unsigned long*)arg;
	struct openfile* openfile = &open_entry->open_files[open_index];

	unsigned long object_handle = entry->compilation_info->object_list[*compiled_index];
	if (match_openfile_path(openfile,object_handle)) {
		entry->compilation_info->object_index[*compiled_index].nfsdb_index = open_entry->nfsdb_index;
		entry->compilation_info->object_index[*compiled_index].open_index = open_index;
		return 1;
	}

	return 0;
}

void libetrace_nfsdb_entry_precompute_compilation_info_files(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		for (unsigned long u=0; u<entry->compilation_info->compiled_count; ++u) {
			scan_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_compiled_file,entry,&u);
		}
	}
}

void libetrace_nfsdb_entry_precompute_compilation_info_headers(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		for (unsigned long u=0; u<entry->compilation_info->header_list_count; ++u) {
			scan_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_header_file,entry,&u);
		}
	}
}

void libetrace_nfsdb_entry_precompute_compilation_info_objects(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		for (unsigned long u=0; u<entry->compilation_info->object_list_count; ++u) {
			scan_wr_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_object_file,entry,&u);
		}
	}
}

PyObject * libetrace_create_nfsdb(PyObject *self, PyObject *args) {

	PyObject* stringMap = PyDict_New();
	PyObject* nfsdbJSON = PyTuple_GetItem(args,0);
	PyObject* source_root = PyTuple_GetItem(args,1);
	PyObject* dbversion = PyTuple_GetItem(args,2);
	PyObject* pcp_patterns = PyTuple_GetItem(args,3);
	PyObject* dbfn = PyTuple_GetItem(args,4);
	int show_stats = 0;
	if (PyTuple_Size(args)>5) {
		PyObject* show_stats_arg = PyTuple_GetItem(args,5);
		if (show_stats_arg==Py_True) {
			show_stats = 1;
		}
	}
	struct nfsdb nfsdb = {};
	nfsdb.source_root = PyString_get_c_str(source_root);
	nfsdb.dbversion = PyString_get_c_str(dbversion);
	nfsdb.string_table_size = PyList_Size(nfsdbJSON);
	nfsdb.string_table = malloc(nfsdb.string_table_size*sizeof(const char*));
	nfsdb.string_size_table = malloc(nfsdb.string_table_size*sizeof(uint32_t));
	nfsdb.string_count = 0;
	unsigned long empty_string_id = string_table_add(&nfsdb, PyUnicode_FromString(""), stringMap);
	assert(empty_string_id==LIBETRACE_EMPTY_STRING_HANDLE);
	nfsdb.pcp_pattern_list_size = PyList_Size(pcp_patterns);
	nfsdb.pcp_pattern_list = malloc(nfsdb.pcp_pattern_list_size*sizeof(const char*));
	for (Py_ssize_t i=0; i<nfsdb.pcp_pattern_list_size; ++i) {
		PyObject* p = PyList_GetItem(pcp_patterns,i);
		nfsdb.pcp_pattern_list[i] = PyString_get_c_str(p);
	}
	nfsdb.nfsdb_count = PyList_Size(nfsdbJSON);
	nfsdb.nfsdb = calloc(nfsdb.nfsdb_count,sizeof(struct nfsdb_entry));

	PyObject* osModuleString = PyUnicode_FromString((char*)"os.path");
	PyObject* osModule = PyImport_Import(osModuleString);
	PyObject* pathJoinFunction = PyObject_GetAttrString(osModule,(char*)"join");

	/* Fill all the nfsdb entries in the database */
	for (ssize_t i=0; i<PyList_Size(nfsdbJSON); ++i) {
		struct nfsdb_entry* new_entry = &nfsdb.nfsdb[i];
		new_entry->nfsdb_index = i;
		PyObject* item = PyList_GetItem(nfsdbJSON,i);
		/* pid */
		new_entry->eid.pid = PyLong_AsUnsignedLong(PyDict_GetItem(item, PyUnicode_FromString("p")));
		new_entry->eid.exeidx = PyLong_AsUnsignedLong(PyDict_GetItem(item, PyUnicode_FromString("x")));
		/* elapsed time */
		if (PyDict_Contains(item, PyUnicode_FromString("e"))) {
			new_entry->etime = PyLong_AsUnsignedLong(PyDict_GetItem(item, PyUnicode_FromString("e")));
		}
		else {
			new_entry->etime = ULONG_MAX;
		}
		/* parent pid */
		PyObject* parentEntry = PyDict_GetItem(item, PyUnicode_FromString("r"));
		new_entry->parent_eid.pid = (unsigned long)PyLong_AsLong(PyDict_GetItem(parentEntry, PyUnicode_FromString("p")));
		new_entry->parent_eid.exeidx = PyLong_AsUnsignedLong(PyDict_GetItem(parentEntry, PyUnicode_FromString("x")));
		/* childs */
		PyObject* childEntryList = PyDict_GetItem(item, PyUnicode_FromString("c"));
		new_entry->child_ids_count = PyList_Size(childEntryList);
		new_entry->child_ids = malloc(new_entry->child_ids_count*sizeof(struct cid));
		for (ssize_t u=0; u<PyList_Size(childEntryList); ++u) {
			PyObject* childEntry = PyList_GetItem(childEntryList,u);
			new_entry->child_ids[u].pid = PyLong_AsUnsignedLong(PyDict_GetItem(childEntry, PyUnicode_FromString("p")));
			new_entry->child_ids[u].flags = PyLong_AsUnsignedLong(PyDict_GetItem(childEntry, PyUnicode_FromString("f")));
		}
		/* binary */
		new_entry->binary = string_table_add(&nfsdb, PyDict_GetItem(item, PyUnicode_FromString("b")), stringMap);
		/* cwd */
		new_entry->cwd = string_table_add(&nfsdb, PyDict_GetItem(item, PyUnicode_FromString("w")), stringMap);
		/* <cwd>/<binary> */
		new_entry->bpath = string_table_add(&nfsdb, PyUnicode_FromString(joinpath(nfsdb.string_table[new_entry->cwd],
				nfsdb.string_table[new_entry->binary])), stringMap);
		/* argv */
		PyObject* argvEntryList = PyDict_GetItem(item, PyUnicode_FromString("v"));
		new_entry->argv_count = PyList_Size(argvEntryList);
		new_entry->argv = malloc(new_entry->argv_count*sizeof(unsigned long));
		for (ssize_t u=0; u<PyList_Size(argvEntryList); ++u) {
			new_entry->argv[u] = string_table_add(&nfsdb, PyList_GetItem(argvEntryList,u), stringMap);
		}
		/* Open files */
		PyObject* openEntryList = PyDict_GetItem(item, PyUnicode_FromString("o"));
		new_entry->open_files_count = PyList_Size(openEntryList);
		new_entry->open_files = malloc(new_entry->open_files_count*sizeof(struct openfile));
		for (ssize_t u=0; u<PyList_Size(openEntryList); ++u) {
			PyObject* openEntry = PyList_GetItem(openEntryList,u);
			PyObject* openPath = PyDict_GetItem(openEntry, PyUnicode_FromString("p"));
			PyObject* originalPath = 0;
			if (PyDict_Contains(openEntry, PyUnicode_FromString("o"))) {
				originalPath = PyDict_GetItem(openEntry, PyUnicode_FromString("o"));
			}
			new_entry->open_files[u].path = string_table_add(&nfsdb, openPath, stringMap);
			new_entry->open_files[u].mode = PyLong_AsUnsignedLong(PyDict_GetItem(openEntry, PyUnicode_FromString("m")));
			new_entry->open_files[u].size = PyLong_AsUnsignedLong(PyDict_GetItem(openEntry, PyUnicode_FromString("s")));
			new_entry->open_files[u].original_path = 0;
			if (originalPath) {
				new_entry->open_files[u].original_path = malloc(sizeof(unsigned long));
				*(new_entry->open_files[u].original_path) = string_table_add(&nfsdb, originalPath, stringMap);
			}
		}
		/* precomputed command patterns */
		if (PyDict_Contains(item, PyUnicode_FromString("n"))) {
			PyObject* pcpEntry = PyDict_GetItem(item, PyUnicode_FromString("n"));
			new_entry->pcp = base64_decode (PyString_get_c_str(pcpEntry), &new_entry->pcp_count);
		}
		/* wrapper pid */
		if (PyDict_Contains(item, PyUnicode_FromString("m"))) {
			new_entry->wrapper_pid = PyLong_AsUnsignedLong(PyDict_GetItem(item, PyUnicode_FromString("m")));
		}
		else {
			new_entry->wrapper_pid = ULONG_MAX;
		}
		/* pipe pids */
		PyObject* pipeEntryList = PyDict_GetItem(item, PyUnicode_FromString("i"));
		new_entry->pipe_eids_count = PyList_Size(pipeEntryList);
		new_entry->pipe_eids = malloc(new_entry->pipe_eids_count*sizeof(struct eid));
		for (ssize_t u=0; u<PyList_Size(pipeEntryList); ++u) {
			PyObject* pipeEntry = PyList_GetItem(pipeEntryList,u);
			new_entry->pipe_eids[u].pid = PyLong_AsUnsignedLong(PyDict_GetItem(pipeEntry, PyUnicode_FromString("p")));
			new_entry->pipe_eids[u].exeidx = PyLong_AsUnsignedLong(PyDict_GetItem(pipeEntry, PyUnicode_FromString("x")));
		}
		/* compilation information */
		if (PyDict_Contains(item, PyUnicode_FromString("d"))) {
			PyObject* compilationEntry = PyDict_GetItem(item, PyUnicode_FromString("d"));
			new_entry->compilation_info = calloc(1,sizeof(struct compilation_info));
			/* compiled list */
			PyObject* compiledEntryList = PyDict_GetItem(compilationEntry, PyUnicode_FromString("f"));
			new_entry->compilation_info->compiled_count = PyList_Size(compiledEntryList);
			new_entry->compilation_info->compiled_list = malloc(new_entry->compilation_info->compiled_count*sizeof(unsigned long));
			for (ssize_t u=0; u<PyList_Size(compiledEntryList); ++u) {
				new_entry->compilation_info->compiled_list[u] =
						string_table_add(&nfsdb, PyList_GetItem(compiledEntryList,u), stringMap);
			}
			new_entry->compilation_info->compiled_index = malloc(new_entry->compilation_info->compiled_count*sizeof(struct nfsdb_entry_file_index));
			/* include path list */
			PyObject* ipathEntryList = PyDict_GetItem(compilationEntry, PyUnicode_FromString("i"));
			new_entry->compilation_info->include_paths_count = PyList_Size(ipathEntryList);
			new_entry->compilation_info->include_paths = malloc(new_entry->compilation_info->include_paths_count*sizeof(unsigned long));
			for (ssize_t u=0; u<PyList_Size(ipathEntryList); ++u) {
				new_entry->compilation_info->include_paths[u] =
						string_table_add(&nfsdb, PyList_GetItem(ipathEntryList,u), stringMap);
			}
			/* preprocessor defs */
			PyObject* pdefsEntryList = PyDict_GetItem(compilationEntry, PyUnicode_FromString("d"));
			new_entry->compilation_info->pp_defs_count = PyList_Size(pdefsEntryList);
			new_entry->compilation_info->pp_defs = malloc(new_entry->compilation_info->pp_defs_count*sizeof(struct pp_def));
			for (ssize_t u=0; u<PyList_Size(pdefsEntryList); ++u) {
				PyObject* pdefsEntry = PyList_GetItem(pdefsEntryList,u);
				new_entry->compilation_info->pp_defs[u].name =
						string_table_add(&nfsdb, PyDict_GetItem(pdefsEntry, PyUnicode_FromString("n")), stringMap);
				new_entry->compilation_info->pp_defs[u].value =
						string_table_add(&nfsdb, PyDict_GetItem(pdefsEntry, PyUnicode_FromString("v")), stringMap);
			}
			/* preprocessor undefs */
			PyObject* pudefsEntryList = PyDict_GetItem(compilationEntry, PyUnicode_FromString("u"));
			new_entry->compilation_info->pp_udefs_count = PyList_Size(pudefsEntryList);
			new_entry->compilation_info->pp_udefs = malloc(new_entry->compilation_info->pp_udefs_count*sizeof(struct pp_def));
			for (ssize_t u=0; u<PyList_Size(pudefsEntryList); ++u) {
				PyObject* pudefsEntry = PyList_GetItem(pudefsEntryList,u);
				new_entry->compilation_info->pp_udefs[u].name =
						string_table_add(&nfsdb, PyDict_GetItem(pudefsEntry, PyUnicode_FromString("n")), stringMap);
				new_entry->compilation_info->pp_udefs[u].value =
						string_table_add(&nfsdb, PyDict_GetItem(pudefsEntry, PyUnicode_FromString("v")), stringMap);
			}
			/* header list */
			PyObject* headerEntryList = PyDict_GetItem(compilationEntry, PyUnicode_FromString("h"));
			new_entry->compilation_info->header_list_count = PyList_Size(headerEntryList);
			new_entry->compilation_info->header_list = malloc(new_entry->compilation_info->header_list_count*sizeof(unsigned long));
			for (ssize_t u=0; u<PyList_Size(headerEntryList); ++u) {
				new_entry->compilation_info->header_list[u] =
						string_table_add(&nfsdb, PyList_GetItem(headerEntryList,u), stringMap);
			}
			new_entry->compilation_info->header_index = malloc(new_entry->compilation_info->header_list_count*sizeof(struct nfsdb_entry_file_index));
			/* compilation type */
			new_entry->compilation_info->compilation_type = PyLong_AsLong(PyDict_GetItem(compilationEntry, PyUnicode_FromString("s")));
			/* object list */
			PyObject* objectEntryList = PyDict_GetItem(compilationEntry, PyUnicode_FromString("o"));
			new_entry->compilation_info->object_list_count = PyList_Size(objectEntryList);
			new_entry->compilation_info->object_list = malloc(new_entry->compilation_info->object_list_count*sizeof(unsigned long));
			for (ssize_t u=0; u<PyList_Size(objectEntryList); ++u) {
				new_entry->compilation_info->object_list[u] =
						string_table_add(&nfsdb, PyList_GetItem(objectEntryList,u), stringMap);
			}
			new_entry->compilation_info->object_index = malloc(new_entry->compilation_info->object_list_count*sizeof(struct nfsdb_entry_file_index));
			/* integrated or not */
			new_entry->compilation_info->integrated_compilation =
					(PyLong_AsLong(PyDict_GetItem(compilationEntry, PyUnicode_FromString("p")))==0);
		}
		/* linked file */
		if (PyDict_Contains(item, PyUnicode_FromString("l"))) {
			unsigned long* linked_file = calloc(1,sizeof(unsigned long));
			*linked_file = string_table_add(&nfsdb, PyDict_GetItem(item, PyUnicode_FromString("l")), stringMap);
			new_entry->linked_file = linked_file;
		}
		/* linked type */
		if (PyDict_Contains(item, PyUnicode_FromString("l"))) {
			new_entry->linked_type = PyLong_AsUnsignedLong(PyDict_GetItem(item, PyUnicode_FromString("t")));
		}
	}

	/* Create auxiliary maps used by the Python API */
	int ok = nfsdb_maps(&nfsdb,show_stats);
	(void)ok;

	/* Precompute the values of openfile entry locations for some specific files
	 *  like linked file, compiled file etc.
	 */
	for (unsigned long u=0; u<nfsdb.nfsdb_count; ++u) {
		struct nfsdb_entry* entry = &nfsdb.nfsdb[u];
		if (entry->linked_file) {
			libetrace_nfsdb_entry_precompute_linked_file(&nfsdb,entry);
		}
		if (entry->compilation_info) {
			libetrace_nfsdb_entry_precompute_compilation_info_files(&nfsdb,entry);
			libetrace_nfsdb_entry_precompute_compilation_info_headers(&nfsdb,entry);
			libetrace_nfsdb_entry_precompute_compilation_info_objects(&nfsdb,entry);
		}
	}

	FILE* out = fopen(PyString_get_c_str(dbfn), "w");
	if (!out) {
		printf("Couldn't create flatten image file: %s\n",PyString_get_c_str(dbfn));
		Py_RETURN_FALSE;
	}

	flatten_init();
	FOR_ROOT_POINTER(&nfsdb,
		UNDER_ITER_HARNESS2(
			FLATTEN_STRUCT2_ITER(nfsdb,&nfsdb);
		);
	);

	int err;
	if ((err = flatten_write(out)) != 0) {
		printf("flatten_write(): %d\n",err);
		Py_RETURN_FALSE;
	}

	flatten_fini();
	fclose(out);

	Py_DECREF(pathJoinFunction);
	Py_DECREF(osModule);
	Py_DECREF(osModuleString);

	destroy_nfsdb(&nfsdb);

	Py_RETURN_TRUE;
}

static void destroy_nfsdb_deps(struct nfsdb_deps* nfsdb_deps) {

	// TODO
}

PyObject* libetrace_nfsdb_create_deps_cache(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* depmap = PyTuple_GetItem(args,0);
	PyObject* ddepmap = PyTuple_GetItem(args,1);
	PyObject* dbfn = PyTuple_GetItem(args,2);
	int show_stats = 0;
	if (PyTuple_Size(args)>3) {
		PyObject* show_stats_arg = PyTuple_GetItem(args,3);
		if (show_stats_arg==Py_True) {
			show_stats = 1;
		}
	}
	struct nfsdb_deps nfsdb_deps = {};

	/* Make a cache string table object */
	PyObject* stringTable = PyDict_New();
	for (unsigned long u=0; u<self->nfsdb->string_count; ++u) {
		PyObject* s = PyUnicode_FromString(self->nfsdb->string_table[u]);
		PyDict_SetItem(stringTable, s, PyLong_FromUnsignedLong(u));
	}

	PyObject* depmap_keys = PyDict_Keys(depmap);
	unsigned long depmap_vals = 0;
	for (Py_ssize_t i=0; i<PyList_Size(depmap_keys); ++i) {
		PyObject* mpath = PyList_GetItem(depmap_keys,i);
		assert(PyDict_Contains(stringTable, mpath));
		PyObject* mpath_handle = PyDict_GetItem(stringTable, mpath);
		PyObject* deps = PyDict_GetItem(depmap,mpath);
		unsigned long* values = malloc(PyList_Size(deps)*2*sizeof(unsigned long));
		depmap_vals+=PyList_Size(deps);
		for (Py_ssize_t j=0; j<PyList_Size(deps); ++j) {
			PyObject* dtuple = PyList_GetItem(deps,j);
			values[2*j+0] = PyLong_AsUnsignedLong(PyList_GetItem(dtuple,0));
			values[2*j+1] = PyLong_AsUnsignedLong(PyList_GetItem(dtuple,1));
		}
		ulongMap_insert(&nfsdb_deps.depmap, PyLong_AsUnsignedLong(mpath_handle), values, 2*PyList_Size(deps));
	}
	if (show_stats) {
		printf("depmap entries: %ld\n",PyList_Size(depmap_keys));
		printf("depmap values: %ld\n",depmap_vals);
	}
	Py_DecRef(depmap_keys);

	PyObject* ddepmap_keys = PyDict_Keys(ddepmap);
	unsigned long ddepmap_vals = 0;
	for (Py_ssize_t i=0; i<PyList_Size(ddepmap_keys); ++i) {
		PyObject* mpath = PyList_GetItem(ddepmap_keys,i);
		assert(PyDict_Contains(stringTable, mpath));
		PyObject* mpath_handle = PyDict_GetItem(stringTable, mpath);
		PyObject* ddeps = PyDict_GetItem(ddepmap,mpath);
		unsigned long* values = malloc(PyList_Size(ddeps)*2*sizeof(unsigned long));
		ddepmap_vals+=PyList_Size(ddeps);
		for (Py_ssize_t j=0; j<PyList_Size(ddeps); ++j) {
			PyObject* ddtuple = PyList_GetItem(ddeps,j);
			values[2*j+0] = PyLong_AsUnsignedLong(PyList_GetItem(ddtuple,0));
			values[2*j+1] = PyLong_AsUnsignedLong(PyList_GetItem(ddtuple,1));
		}
		ulongMap_insert(&nfsdb_deps.ddepmap, PyLong_AsUnsignedLong(mpath_handle), values, 2*PyList_Size(ddeps));
	}
	if (show_stats) {
		printf("ddepmap entries: %ld\n",PyList_Size(ddepmap_keys));
		printf("ddepmap values: %ld\n",ddepmap_vals);
	}
	Py_DecRef(ddepmap_keys);

	FILE* out = fopen(PyString_get_c_str(dbfn), "w");
	if (!out) {
		printf("Couldn't create flatten image file: %s\n",PyString_get_c_str(dbfn));
		Py_RETURN_FALSE;
	}

	flatten_init();
	FOR_ROOT_POINTER(&nfsdb_deps,
		UNDER_ITER_HARNESS2(
			FLATTEN_STRUCT2_ITER(nfsdb_deps,&nfsdb_deps);
		);
	);

	int err;
	if ((err = flatten_write(out)) != 0) {
		printf("flatten_write(): %d\n",err);
		Py_RETURN_FALSE;
	}

	flatten_fini();
	fclose(out);


	destroy_nfsdb_deps(&nfsdb_deps);

	Py_DecRef(stringTable);
	Py_RETURN_TRUE;
}

PyObject * libetrace_parse_nfsdb(PyObject *self, PyObject *args) {

	int argc = 1;
	Py_ssize_t nargs = PyTuple_Size(args);

	if (nargs>=1) {
		argc += nargs;
	}

	const char** argv = malloc(argc*sizeof(char**));
	argv[0] = "libetrace.parse_nfsdb";

	for (Py_ssize_t i=0; i<nargs; ++i) {
		PyObject* arg = PyTuple_GetItem(args,i);
		if (!PyUnicode_Check(arg)) {
			free(argv);
			Py_RETURN_NONE;
		}
		argv[1+i] = PyString_get_c_str(arg);
	}

	int r = parser_main(argc,(char**)argv);

	for (Py_ssize_t i=0; i<nargs; ++i) {
		PYASSTR_DECREF(argv[1+i]);
	}
	free(argv);

	return PyLong_FromLong(r);
}

void libetrace_nfsdb_dealloc(libetrace_nfsdb_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    if (self->init_done) {
    	unflatten_fini();
    }
    Py_DecRef(self->libetrace_nfsdb_entry_filterMap);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_object* self;

    self = (libetrace_nfsdb_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	/* Use the 'load' function to initialize the cache */
    	self->init_done = 0;
    	self->nfsdb = 0;
    	self->libetrace_nfsdb_entry_filterMap = PyDict_New();
    	for (unsigned long i=0; i<LIBETRACE_NFSDB_ENTRY_FILTER_FUNC_ARRAY_SIZE; ++i) {
    		PyDict_SetItem(self->libetrace_nfsdb_entry_filterMap,
    				PyUnicode_FromString(nfsdb_entry_filter_func_array[i].nfsdb_entry_filter_funcname),
					PyLong_FromUnsignedLong((unsigned long)nfsdb_entry_filter_func_array[i].fp));
    	}
    }

    return (PyObject *)self;
}

int libetrace_nfsdb_init(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwds) {

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

PyObject* libetrace_nfsdb_repr(PyObject* self) {

	static char repr[1024];

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	const char* inited = "initialized";
	const char* ninited = "not initialized";
	int written = snprintf(repr,1024,"<nfsdb object at %lx : %s",(uintptr_t)self,__self->init_done?inited:ninited);
	written+=snprintf(repr+written,1024-written,"|debug:%d;verbose:%d|",__self->debug,__self->verbose);
	written+=snprintf(repr+written,1024-written,"entry_count:%lu;string_count:%lu>",__self->nfsdb->nfsdb_count,
			__self->nfsdb->string_count);

	return PyUnicode_FromString(repr);
}

int libetrace_nfsdb_bool(PyObject* self) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	return __self->init_done;
}

Py_ssize_t libetrace_nfsdb_sq_length(PyObject* self) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	return __self->nfsdb->nfsdb_count;
}

PyObject* libetrace_nfsdb_sq_item(PyObject* self, Py_ssize_t index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,index);
	PyObject *entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
	Py_DECREF(args);
	return entry;
}

static PyObject* libetrace_nfsdb_entry_list(libetrace_nfsdb_object* self, struct nfsdb_entryMap_node* node) {

	PyObject* rL = PyList_New(0);
	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* entry = node->entry_list[i];
		PyObject* item = libetrace_nfsdb_sq_item((PyObject*)self,entry->nfsdb_index);
		PYLIST_ADD_PYOBJECT(rL,item);
	}
	return rL;
}

PyObject* libetrace_nfsdb_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (PyLong_Check(slice)) {
		return libetrace_nfsdb_sq_item(self,PyLong_AsLong(slice));
	}
	else if (PyTuple_Check(slice)) {
		ASSERT_WITH_NFSDB_ERROR(PyTuple_Size(slice)>=1,"Invalid subscript argument");
		PyObject* pypid = PyTuple_GetItem(slice,0);
		ASSERT_WITH_NFSDB_ERROR(PyLong_Check(pypid),"Invalid subscript argument");
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->procmap,PyLong_AsLong(pypid));
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Invalid pid key [%ld] at nfsdb entry",PyLong_AsLong(pypid));
		if (PyTuple_Size(slice)<2) {
			/* Return the list of all executions for a given pid */
			return libetrace_nfsdb_entry_list(__self,node);
		}
		else {
			/* Return specific execution given pid:exeidx value */
			PyObject* pyexeidx = PyTuple_GetItem(slice,1);
			ASSERT_WITH_NFSDB_ERROR(PyLong_Check(pyexeidx),"Invalid subscript argument");
			ASSERT_WITH_NFSDB_FORMAT_ERROR(PyLong_AsLong(pyexeidx)<node->entry_count,
					"nfsdb entry index [%ld] out of range",PyLong_AsLong(pyexeidx));
			struct nfsdb_entry* entry = node->entry_list[PyLong_AsLong(pyexeidx)];
			return libetrace_nfsdb_sq_item(self,entry->nfsdb_index);
		}
	}
	else if (PyUnicode_Check(slice)) {
		const char* bpath = PyString_get_c_str(slice);
		struct stringRefMap_node* srefnode = stringRefMap_search(&__self->nfsdb->revstringmap, bpath);
		if (!srefnode) {
			snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid binary path key [%s] at nfsdb entry",bpath);
			PyErr_SetString(libetrace_nfsdbError, errmsg);
			PYASSTR_DECREF(bpath);
			return 0;
		}
		PYASSTR_DECREF(bpath);
		unsigned long hbpath = srefnode->value;
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->bmap,hbpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at binary path handle [%lu]",hbpath);
		return libetrace_nfsdb_entry_list(__self,node);
	}
	else if (PyObject_IsInstance(slice, (PyObject *)&libetrace_nfsdbEntryEidType)) {
		libetrace_nfsdb_entry_eid_object* eid = (libetrace_nfsdb_entry_eid_object*)slice;
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->procmap,eid->pid);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Invalid pid key [%ld] at nfsdb entry",eid->pid);
		if (eid->exeidx==ULONG_MAX) {
			/* Return the list of all executions for a given pid */
			return libetrace_nfsdb_entry_list(__self,node);
		}
		else {
			/* Return specific execution given pid:exeidx value */
			ASSERT_WITH_NFSDB_FORMAT_ERROR(eid->exeidx<node->entry_count,
					"nfsdb entry index [%ld] out of range",eid->exeidx);
			struct nfsdb_entry* entry = node->entry_list[eid->exeidx];
			return libetrace_nfsdb_sq_item(self,entry->nfsdb_index);
		}
	}
	else if (!PySlice_Check(slice)) {
		PyErr_SetString(libetrace_nfsdbError, "Invalid subscript argument");
		return 0;
	}

	/* Now we have a real slice with numbers */
	Py_ssize_t start,end,step;
	if (PySlice_Unpack(slice,&start,&end,&step)) {
		PyErr_SetString(libetrace_nfsdbError, "Cannot decode slice argument");
		return 0;
	}

	PyObject* rL = PyList_New(0);
	for (Py_ssize_t i=start; i<((end<__self->nfsdb->nfsdb_count)?(end):(__self->nfsdb->nfsdb_count)); i+=step) {
		PyObject* entry = libetrace_nfsdb_sq_item(self,i);
		PYLIST_ADD_PYOBJECT(rL,entry);
	}

	return rL;
}

PyObject* libetrace_nfsdb_getiter(PyObject* self) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	PyObject* args = PyTuple_New(4);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)self);
	PYTUPLE_SET_ULONG(args,1,0); /* start index */
	PYTUPLE_SET_ULONG(args,2,1); /* step */
	PYTUPLE_SET_ULONG(args,3,__self->nfsdb->nfsdb_count); /* end */
	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbIterType, args);
	Py_DecRef(args);

	return iter;
}

PyObject* libetrace_nfsdb_iter(libetrace_nfsdb_object *self, PyObject *args) {

	Py_ssize_t start = 0;
	Py_ssize_t step = 1;
	Py_ssize_t end = self->nfsdb->nfsdb_count;

	if (PyTuple_Size(args)>0) {
		start = PyLong_AsLong(PyTuple_GetItem(args,0));
	}

	if (PyTuple_Size(args)>1) {
		step = PyLong_AsLong(PyTuple_GetItem(args,1));
	}

	if (PyTuple_Size(args)>2) {
		end = PyLong_AsLong(PyTuple_GetItem(args,2));
	}

	PyObject* iter_args = PyTuple_New(4);
	PYTUPLE_SET_ULONG(iter_args,0,(uintptr_t)self);
	PYTUPLE_SET_ULONG(iter_args,1,start); /* start index */
	PYTUPLE_SET_ULONG(iter_args,2,step); /* step */
	PYTUPLE_SET_ULONG(iter_args,3,end); /* end */
	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbIterType, iter_args);
	Py_DecRef(iter_args);

	return iter;
}

int libetrace_nfsdb_entry_filter_has_comp_info(const struct nfsdb_entry* entry) {

	if (entry->compilation_info) {
		return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_filter_has_linked_file(const struct nfsdb_entry* entry) {

	if (entry->linked_file) {
		return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_filter_has_command(const struct nfsdb_entry* entry) {

	if ((entry->bpath!=LIBETRACE_EMPTY_STRING_HANDLE)&&(entry->argv_count>0)) {
		return 1;
	}

	return 0;
}

PyObject* libetrace_nfsdb_filtered_iter(libetrace_nfsdb_object *self, PyObject* args, PyObject* kwargs) {

	Py_ssize_t start = 0;
	Py_ssize_t step = 1;
	Py_ssize_t end = self->nfsdb->nfsdb_count;

	if (PyTuple_Size(args)>0) {
		start = PyLong_AsLong(PyTuple_GetItem(args,0));
	}

	if (PyTuple_Size(args)>1) {
		step = PyLong_AsLong(PyTuple_GetItem(args,1));
	}

	if (PyTuple_Size(args)>2) {
		end = PyLong_AsLong(PyTuple_GetItem(args,2));
	}

	PyObject* filterList = PyList_New(0);
	if (kwargs) {
		PyObject* kwargs_keys = PyDict_Keys(kwargs);
		for (Py_ssize_t i=0; i<PyList_Size(kwargs_keys); ++i) {
			PyObject* kwkey = PyList_GetItem(kwargs_keys,i);
			PyObject* kwval = PyDict_GetItem(kwargs, kwkey);
			if (PyObject_IsTrue(kwval)) {
				if (PyDict_Contains(self->libetrace_nfsdb_entry_filterMap,kwkey)) {
					PyObject* py_fp = PyDict_GetItem(self->libetrace_nfsdb_entry_filterMap,kwkey);
					PyList_Append(filterList,py_fp);
				}
			}
		}
		Py_DecRef(kwargs_keys);
	}

	PyObject* iter_args = PyTuple_New(5);
	PYTUPLE_SET_ULONG(iter_args,0,(uintptr_t)self);
	PYTUPLE_SET_ULONG(iter_args,1,start); /* start index */
	PYTUPLE_SET_ULONG(iter_args,2,step); /* step */
	PYTUPLE_SET_ULONG(iter_args,3,end); /* end */
	PyTuple_SetItem(iter_args,4,filterList); /* list of entry filters */
	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbFilteredIterType, iter_args);
	Py_DecRef(iter_args);

	return iter;
}

PyObject* libetrace_nfsdb_opens_iter(libetrace_nfsdb_object *self, PyObject *args) {

	Py_ssize_t start = 0;
	Py_ssize_t step = 1;
	Py_ssize_t end = self->nfsdb->nfsdb_count;
	unsigned long open_index = 0;

	if (PyTuple_Size(args)>0) {
		start = PyLong_AsLong(PyTuple_GetItem(args,0));
	}

	if (PyTuple_Size(args)>1) {
		step = PyLong_AsLong(PyTuple_GetItem(args,1));
	}

	if (PyTuple_Size(args)>2) {
		end = PyLong_AsLong(PyTuple_GetItem(args,2));
	}

	if (PyTuple_Size(args)>3) {
		open_index = PyLong_AsUnsignedLong(PyTuple_GetItem(args,3));
	}

	PyObject* iter_args = PyTuple_New(5);
	PYTUPLE_SET_ULONG(iter_args,0,(uintptr_t)self);
	PYTUPLE_SET_ULONG(iter_args,1,start); /* start index */
	PYTUPLE_SET_ULONG(iter_args,2,step); /* step */
	PYTUPLE_SET_ULONG(iter_args,3,end); /* end */
	PYTUPLE_SET_ULONG(iter_args,4,open_index); /* end */
	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbOpensIterType, iter_args);
	Py_DecRef(iter_args);

	return iter;
}

PyObject* libetrace_nfsdb_pcp_list(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* pcp = PyList_New(0);

	for (unsigned long i=0; i<self->nfsdb->pcp_pattern_list_size; ++i) {
		const char* p = self->nfsdb->pcp_pattern_list[i];
		PYLIST_ADD_STRING(pcp,p);
	}

	return pcp;
}

PyObject* libetrace_nfsdb_pid_list(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* pids = PyList_New(0);

	struct rb_node * p = rb_first(&self->nfsdb->procmap);
	while(p) {
		struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
		PYLIST_ADD_ULONG(pids,data->key);
		p = rb_next(p);
	}

	return pids;
}

PyObject* libetrace_nfsdb_bpath_list(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* bpaths = PyList_New(0);

	struct rb_node * p = rb_first(&self->nfsdb->bmap);
	while(p) {
		struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
		PYLIST_ADD_STRING(bpaths,self->nfsdb->string_table[data->key]);
		p = rb_next(p);
	}

	return bpaths;
}

PyObject* libetrace_nfsdb_fpath_list(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* fpaths = PyList_New(0);

	struct rb_node * p = rb_first(&self->nfsdb->filemap);
	while(p) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		PYLIST_ADD_STRING(fpaths,self->nfsdb->string_table[data->key]);
		p = rb_next(p);
	}

	return fpaths;
}

PyObject* libetrace_nfsdb_linked_module_paths(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* linked_modules = PyList_New(0);

	struct rb_node * p = rb_first(&self->nfsdb->linkedmap);
	while(p) {
		struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
		PyObject* lm = PyTuple_New(2);
		PYTUPLE_SET_STR(lm,0,self->nfsdb->string_table[data->key]);
		PYTUPLE_SET_LONG(lm,1,data->entry_list[0]->linked_type);
		PyList_Append(linked_modules,lm);
		p = rb_next(p);
	}

	return linked_modules;
}

PyObject* libetrace_nfsdb_linked_modules(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* linked_modules = PyList_New(0);

	struct rb_node * p = rb_first(&self->nfsdb->linkedmap);
	while(p) {
		struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
		const struct nfsdb_entry* entry = data->entry_list[0];
		if (entry->linked_file) {
			libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(self->nfsdb,
					entry, entry->linked_index.open_index, entry->linked_index.nfsdb_index);
			PyObject* lm = PyTuple_New(2);
			PyTuple_SetItem(lm,0,(PyObject*)openfile);
			PYTUPLE_SET_LONG(lm,1,data->entry_list[0]->linked_type);
			PyList_Append(linked_modules,lm);
		}
		p = rb_next(p);
	}

	return linked_modules;
}

PyObject* libetrace_nfsdb_path_exists(libetrace_nfsdb_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	PyObject* path = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(path)) {
		const char* fpath = PyString_get_c_str(path);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
		PYASSTR_DECREF(fpath);
		if (!srefnode) {
			Py_RETURN_FALSE;
		}
		unsigned long hfpath = srefnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at file path handle [%lu]",hfpath);
		if (node->rd_entry_count>0) {
			struct nfsdb_entry* entry = node->rd_entry_list[0];
			unsigned long index = node->rd_entry_index[0];
			if ((entry->open_files[index].mode&0x40)!=0) Py_RETURN_TRUE;
		}
		else if (node->rw_entry_count>0) {
			struct nfsdb_entry* entry = node->rw_entry_list[0];
			unsigned long index = node->rw_entry_index[0];
			if ((entry->open_files[index].mode&0x40)!=0) Py_RETURN_TRUE;
		}
		else if (node->wr_entry_count>0) {
			struct nfsdb_entry* entry = node->wr_entry_list[0];
			unsigned long index = node->wr_entry_index[0];
			if ((entry->open_files[index].mode&0x40)!=0) Py_RETURN_TRUE;
		}
		Py_RETURN_FALSE;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid argument type (not a string)");
	return 0;
}

PyObject* libetrace_nfsdb_path_regular(libetrace_nfsdb_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	PyObject* path = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(path)) {
		const char* fpath = PyString_get_c_str(path);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
		PYASSTR_DECREF(fpath);
		if (!srefnode) {
			Py_RETURN_FALSE;
		}
		unsigned long hfpath = srefnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at file path handle [%lu]",hfpath);
		if (node->rd_entry_count>0) {
			struct nfsdb_entry* entry = node->rd_entry_list[0];
			unsigned long index = node->rd_entry_index[0];
			unsigned long mode = entry->open_files[index].mode;
			if (((mode&0x40)!=0)&&(((mode&0x3C)>>2)==0x8)) Py_RETURN_TRUE;
		}
		else if (node->rw_entry_count>0) {
			struct nfsdb_entry* entry = node->rw_entry_list[0];
			unsigned long index = node->rw_entry_index[0];
			unsigned long mode = entry->open_files[index].mode;
			if (((mode&0x40)!=0)&&(((mode&0x3C)>>2)==0x8)) Py_RETURN_TRUE;
		}
		else if (node->wr_entry_count>0) {
			struct nfsdb_entry* entry = node->wr_entry_list[0];
			unsigned long index = node->wr_entry_index[0];
			unsigned long mode = entry->open_files[index].mode;
			if (((mode&0x40)!=0)&&(((mode&0x3C)>>2)==0x8)) Py_RETURN_TRUE;
		}
		Py_RETURN_FALSE;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid argument type (not a string)");
	return 0;
}

PyObject* libetrace_nfsdb_path_read(libetrace_nfsdb_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	PyObject* path = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(path)) {
		const char* fpath = PyString_get_c_str(path);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
		PYASSTR_DECREF(fpath);
		if (!srefnode) {
			Py_RETURN_FALSE;
		}
		unsigned long hfpath = srefnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at file path handle [%lu]",hfpath);
		if ((node->rd_entry_count>0)||(node->rw_entry_count>0)) {
			Py_RETURN_TRUE;
		}
		Py_RETURN_FALSE;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid argument type (not a string)");
	return 0;
}

PyObject* libetrace_nfsdb_path_write(libetrace_nfsdb_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	PyObject* path = PyTuple_GetItem(args,0);

	if (PyUnicode_Check(path)) {
		const char* fpath = PyString_get_c_str(path);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
		PYASSTR_DECREF(fpath);
		if (!srefnode) {
			Py_RETURN_FALSE;
		}
		unsigned long hfpath = srefnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at file path handle [%lu]",hfpath);
		if ((node->rw_entry_count>0)||(node->wr_entry_count>0)) {
			Py_RETURN_TRUE;
		}
		Py_RETURN_FALSE;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid argument type (not a string)");
	return 0;
}

PyObject* libetrace_nfsdb_get_filemap(PyObject* self, void* closure) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	PyObject* args = PyTuple_New(1);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PyObject *filemap = PyObject_CallObject((PyObject *) &libetrace_nfsdbFileMapType, args);
	Py_DECREF(args);

	return filemap;
}

PyObject* libetrace_nfsdb_get_source_root(PyObject* self, void* closure) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	return PyUnicode_FromString(__self->nfsdb->source_root);
}

PyObject* libetrace_nfsdb_get_dbversion(PyObject* self, void* closure) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	return PyUnicode_FromString(__self->nfsdb->dbversion);
}

PyObject* libetrace_nfsdb_load(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwargs ) {

    const char* cache_filename = ".nfsdb.img";

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

    DBG( debug, "--- libetrace_nfsdb_load(\"%s\")\n",cache_filename);

    if (self->init_done) {
    	PyErr_SetString(libetrace_nfsdbError, "nfsdb cache already initialized");
    	goto done;
	}

    if (no_map_memory) {
    	FILE* in = fopen(cache_filename, "rb");
		if (!in) {
			PyErr_SetString(libetrace_nfsdbError, "Cannot open cache file");
			goto done;
		}
		unflatten_init();
		if (quiet)
			flatten_set_option(option_silent);
		if (unflatten_read(in)) {
			PyErr_SetString(libetrace_nfsdbError, "Failed to read cache file");
			goto done;
		}
		fclose(in);
    }
    else {
    	int map_fd = open(cache_filename,O_RDWR);
    	if (map_fd<0) {
    		PyErr_SetString(libetrace_nfsdbError, "Cannot open cache file");
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
    		PyErr_SetString(libetrace_nfsdbError, "Failed to map cache file");
    		goto done;
    	}
    }

    self->nfsdb = ROOT_POINTER_NEXT(const struct nfsdb*);
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

PyObject* libetrace_nfsdb_load_deps(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwargs) {

	const char* cache_filename = ".nfsdb.deps.img";

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

	DBG( debug, "--- libetrace_nfsdb_load_deps(\"%s\")\n",cache_filename);

	if (self->nfsdb_deps) {
		PyErr_SetString(libetrace_nfsdbError, "nfsdb dependency cache already initialized");
		goto done;
	}

	if (no_map_memory) {
		FILE* in = fopen(cache_filename, "rb");
		if (!in) {
			PyErr_SetString(libetrace_nfsdbError, "Cannot open cache file");
			goto done;
		}
		unflatten_init();
		if (quiet)
			flatten_set_option(option_silent);
		if (unflatten_read(in)) {
			PyErr_SetString(libetrace_nfsdbError, "Failed to read cache file");
			goto done;
		}
		fclose(in);
	}
	else {
		int map_fd = open(cache_filename,O_RDWR);
		if (map_fd<0) {
			PyErr_SetString(libetrace_nfsdbError, "Cannot open cache file");
			goto done;
		}
		unflatten_init();
		if (quiet)
			flatten_set_option(option_silent);
		int map_err;
		if (!mp_safe) {
			/* Try to map the cache file to the previously used address
			 * When it fails map to new address and update the file address
			 * (we're not going concurrently here)
			 */
			map_err = unflatten_map(map_fd,0);
		}
		else {
			/* Try to map the cache file to the previously used address
			 * When it fails map to new address privately
			 * (this will incur a small penalty on updating all the image pointers
			 *  but can save us some trouble when running concurrently)
			 */
			map_err = unflatten_map_private(map_fd,0);
		}
		close(map_fd);
		if (map_err) {
			PyErr_SetString(libetrace_nfsdbError, "Failed to map cache file");
			goto done;
		}
	}

	self->nfsdb_deps = ROOT_POINTER_NEXT(const struct nfsdb_deps*);
	err = 1;

done:
	Py_DecRef(py_debug);
	Py_DecRef(py_quiet);
	Py_DecRef(py_no_map_memory);
	PYASSTR_DECREF(cache_filename);
	if (!err) Py_RETURN_FALSE;
	Py_RETURN_TRUE;
}

PyObject* libetrace_nfsdb_module_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	PyObject* module_path = PyTuple_GetItem(args,0);
	PyObject* py_direct = PyUnicode_FromString("direct");
	int direct = 0;

	if (kwargs) {
		if (PyDict_Contains(kwargs,py_direct)) {
			direct = PyLong_AsLong(PyDict_GetItem(kwargs,py_direct));
		}
	}

	if (!self->nfsdb_deps) {
		PyErr_SetString(libetrace_nfsdbError, "nfsdb dependency cache not initialized");
		Py_DecRef(py_direct);
		return 0;
	}

	const struct rb_root* dmap = 0;
	if (direct) {
		dmap = &self->nfsdb_deps->ddepmap;
	}
	else {
		dmap = &self->nfsdb_deps->depmap;
	}

	PyObject* mL = PyList_New(0);

	if (PyUnicode_Check(module_path)) {
		const char* mpath = PyString_get_c_str(module_path);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, mpath);
		PYASSTR_DECREF(mpath);
		if (!srefnode) {
			goto done;
		}
		unsigned long hmpath = srefnode->value;
		struct ulongMap_node* node = ulongMap_search(dmap, hmpath);
		if (!node) {
			goto done;
		}
		for (unsigned long i=0; i<node->value_count; i+=2) {
			unsigned long nfsdb_index = node->value_list[i];
			unsigned long open_index = node->value_list[i+1];
			const struct nfsdb_entry* entry = &self->nfsdb->nfsdb[nfsdb_index];
			libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(
					self->nfsdb,entry,open_index,nfsdb_index);
			PyList_Append(mL,(PyObject*)openfile);
		}
	}
	else {
		PyErr_SetString(libetrace_nfsdbError, "Invalid module path type (not a string)");
		Py_DecRef(py_direct);
		return 0;
	}

done:
	Py_DecRef(py_direct);
	return mL;
}

void libetrace_nfsdb_entry_dealloc(libetrace_nfsdb_entry_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_entry_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_entry_object* self;

    self = (libetrace_nfsdb_entry_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->nfsdb = (const struct nfsdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
    	unsigned long index = PyLong_AsLong(PyTuple_GetItem(args,1));
    	if (index>=self->nfsdb->nfsdb_count) {
    		PyErr_SetString(libetrace_nfsdbError, "nfsdb index out of bounds");
    		return 0;
    	}
    	self->nfsdb_index = index;
    	self->entry = &self->nfsdb->nfsdb[index];
    }

    return (PyObject *)self;
}

PyObject* libetrace_nfsdb_entry_repr(PyObject* self) {

	static char repr[1024];

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	int written = snprintf(repr,1024,"<nfsdbEntry object at %lx : ",(uintptr_t)self);
	written+=snprintf(repr+written,1024-written,"index:%lu|pid:%lu;exeidx:%lu>",__self->nfsdb_index,
			__self->entry->eid.pid,__self->entry->eid.exeidx);

	return PyUnicode_FromString(repr);
}

PyObject* libetrace_nfsdb_entry_json(libetrace_nfsdb_entry_object *self, PyObject *args) {

	PyObject* json = PyDict_New();

	FTDB_SET_ENTRY_ULONG(json,p,self->entry->eid.pid);
	FTDB_SET_ENTRY_ULONG(json,x,self->entry->eid.exeidx);
	if (self->entry->etime!=ULONG_MAX) {
		FTDB_SET_ENTRY_ULONG(json,e,self->entry->etime);
	}
	PyObject* parentEntry = PyDict_New();
	FTDB_SET_ENTRY_ULONG(parentEntry,p,self->entry->parent_eid.pid);
	FTDB_SET_ENTRY_ULONG(parentEntry,x,self->entry->parent_eid.exeidx);
	FTDB_SET_ENTRY_PYOBJECT(json,r,parentEntry);
	PyObject* childEntries = PyList_New(0);
	for (unsigned long u=0; u<self->entry->child_ids_count; ++u) {
		PyObject* childEntry = PyDict_New();
		FTDB_SET_ENTRY_ULONG(childEntry,p,self->entry->child_ids[u].pid);
		FTDB_SET_ENTRY_ULONG(childEntry,f,self->entry->child_ids[u].flags);
		PYLIST_ADD_PYOBJECT(childEntries,childEntry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json,c,childEntries);
	FTDB_SET_ENTRY_STRING(json,b,self->nfsdb->string_table[self->entry->binary]);
	FTDB_SET_ENTRY_STRING(json,w,self->nfsdb->string_table[self->entry->cwd]);
	PyObject* argvEntries = PyList_New(0);
	for (unsigned long u=0; u<self->entry->argv_count; ++u) {
		PYLIST_ADD_STRING(argvEntries,self->nfsdb->string_table[self->entry->argv[u]]);
	}
	FTDB_SET_ENTRY_PYOBJECT(json,v,argvEntries);
	PyObject* openEntries = PyList_New(0);
	for (unsigned long u=0; u<self->entry->open_files_count; ++u) {
		PyObject* openEntry = PyDict_New();
		FTDB_SET_ENTRY_STRING(openEntry,p,self->nfsdb->string_table[self->entry->open_files[u].path]);
		FTDB_SET_ENTRY_ULONG(openEntry,m,self->entry->open_files[u].mode);
		FTDB_SET_ENTRY_ULONG(openEntry,s,self->entry->open_files[u].size);
		PYLIST_ADD_PYOBJECT(openEntries,openEntry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json,o,openEntries);

	if (self->entry->pcp) {
		char* pcp_string = base64_encode(self->entry->pcp,self->entry->pcp_count);
		FTDB_SET_ENTRY_STRING(json,n,pcp_string);
		free(pcp_string);
	}

	if (self->entry->wrapper_pid!=ULONG_MAX) {
		FTDB_SET_ENTRY_ULONG(json,m,self->entry->wrapper_pid);
	}

	PyObject* pipeEntries = PyList_New(0);
	for (unsigned long u=0; u<self->entry->pipe_eids_count; ++u) {
		PyObject* pipeEntry = PyDict_New();
		FTDB_SET_ENTRY_ULONG(pipeEntry,p,self->entry->pipe_eids[u].pid);
		FTDB_SET_ENTRY_ULONG(pipeEntry,x,self->entry->pipe_eids[u].exeidx);
		PYLIST_ADD_PYOBJECT(pipeEntries,pipeEntry);
	}
	FTDB_SET_ENTRY_PYOBJECT(json,i,pipeEntries);

	if (self->entry->compilation_info) {
		PyObject* ci = PyDict_New();
		/* Compiled files */
		PyObject* compiled_list = PyList_New(0);
		for (unsigned long u=0; u<self->entry->compilation_info->compiled_count; ++u) {
			PYLIST_ADD_STRING(compiled_list,self->nfsdb->string_table[self->entry->compilation_info->compiled_list[u]]);
		}
		FTDB_SET_ENTRY_PYOBJECT(ci,f,compiled_list);
		/* List of include paths */
		PyObject* ipath_list = PyList_New(0);
		for (unsigned long u=0; u<self->entry->compilation_info->include_paths_count; ++u) {
			PYLIST_ADD_STRING(ipath_list,self->nfsdb->string_table[self->entry->compilation_info->include_paths[u]]);
		}
		FTDB_SET_ENTRY_PYOBJECT(ci,i,ipath_list);
		/* Preprocessor definitions */
		PyObject* defs = PyList_New(0);
		for (unsigned long u=0; u<self->entry->compilation_info->pp_defs_count; ++u) {
			PyObject* def = PyDict_New();
			FTDB_SET_ENTRY_STRING(def,n,self->nfsdb->string_table[self->entry->compilation_info->pp_defs[u].name]);
			FTDB_SET_ENTRY_STRING(def,v,self->nfsdb->string_table[self->entry->compilation_info->pp_defs[u].value]);
			PYLIST_ADD_PYOBJECT(defs,def);
		}
		FTDB_SET_ENTRY_PYOBJECT(ci,d,defs);
		/* Preprocessor undefs */
		PyObject* udefs = PyList_New(0);
		for (unsigned long u=0; u<self->entry->compilation_info->pp_udefs_count; ++u) {
			PyObject* udef = PyDict_New();
			FTDB_SET_ENTRY_STRING(udef,n,self->nfsdb->string_table[self->entry->compilation_info->pp_udefs[u].name]);
			FTDB_SET_ENTRY_STRING(udef,v,self->nfsdb->string_table[self->entry->compilation_info->pp_udefs[u].value]);
			PYLIST_ADD_PYOBJECT(udefs,udef);
		}
		FTDB_SET_ENTRY_PYOBJECT(ci,u,udefs);
		/* Header list */
		PyObject* hdr_list = PyList_New(0);
		for (unsigned long u=0; u<self->entry->compilation_info->header_list_count; ++u) {
			PYLIST_ADD_STRING(hdr_list,self->nfsdb->string_table[self->entry->compilation_info->header_list[u]]);
		}
		FTDB_SET_ENTRY_PYOBJECT(ci,h,hdr_list);
		/* Compilation type */
		FTDB_SET_ENTRY_INT(ci,s,self->entry->compilation_info->compilation_type);
		/* List of object files */
		PyObject* obj_list = PyList_New(0);
		for (unsigned long u=0; u<self->entry->compilation_info->object_list_count; ++u) {
			PYLIST_ADD_STRING(obj_list,self->nfsdb->string_table[self->entry->compilation_info->object_list[u]]);
		}
		FTDB_SET_ENTRY_PYOBJECT(ci,o,obj_list);
		FTDB_SET_ENTRY_PYOBJECT(json,d,ci);
	}

	if (self->entry->linked_file) {
		FTDB_SET_ENTRY_STRING(json,l,self->nfsdb->string_table[*self->entry->linked_file]);
	}

	return json;
}

PyObject* libetrace_nfsdb_entry_has_pcp(libetrace_nfsdb_entry_object *self, PyObject *args) {

	if (self->entry->pcp) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_pcp_index(libetrace_nfsdb_entry_object *self, PyObject *args) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->pcp) {
		Py_RETURN_NONE;
	}

	PyObject* pcpv = PyList_New(0);

	unsigned start_index = 0;
	for (unsigned long i=0; i<__self->entry->pcp_count; ++i) {
		unsigned char v = __self->entry->pcp[i];
		for (int j=7; j>=0; --j) {
			if (v&(0x01<<j)) {
				PYLIST_ADD_ULONG(pcpv,start_index+(7-j));
			}
		}
		start_index+=8;
	}

	return pcpv;
}

PyObject* libetrace_nfsdb_entry_is_wrapped(libetrace_nfsdb_entry_object *self, PyObject *args) {

	if (self->entry->wrapper_pid!=ULONG_MAX) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_has_compilations(libetrace_nfsdb_entry_object *self, PyObject *args) {

	if (self->entry->compilation_info) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_is_linking(libetrace_nfsdb_entry_object *self, PyObject *args) {

	if (self->entry->linked_file) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_get_eid(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,__self->entry->eid.pid);
	PYTUPLE_SET_ULONG(args,1,__self->entry->eid.exeidx);
	PyObject *eid = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryEidType, args);
	Py_DECREF(args);

	return eid;
}

PyObject* libetrace_nfsdb_entry_get_etime(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	return PyLong_FromLong(__self->entry->etime);
}

PyObject* libetrace_nfsdb_entry_get_parent_eid(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,__self->entry->parent_eid.pid);
	PYTUPLE_SET_ULONG(args,1,__self->entry->parent_eid.exeidx);
	PyObject *parent_eid = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryEidType, args);
	Py_DECREF(args);

	return parent_eid;
}

PyObject* libetrace_nfsdb_entry_get_childs(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* cL = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->child_ids_count; ++i) {
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,__self->entry->child_ids[i].pid);
		PYTUPLE_SET_ULONG(args,1,__self->entry->child_ids[i].flags);
		PyObject *cid = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryCidType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(cL,cid);
	}

	return cL;
}

PyObject* libetrace_nfsdb_entry_get_binary(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	return PyUnicode_FromString(__self->nfsdb->string_table[__self->entry->binary]);

}

PyObject* libetrace_nfsdb_entry_get_cwd(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	return PyUnicode_FromString(__self->nfsdb->string_table[__self->entry->cwd]);
}

PyObject* libetrace_nfsdb_entry_get_bpath(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	return PyUnicode_FromString(__self->nfsdb->string_table[__self->entry->bpath]);
}

PyObject* libetrace_nfsdb_entry_get_argv(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* argv = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->argv_count; ++i) {
		PYLIST_ADD_STRING(argv,__self->nfsdb->string_table[__self->entry->argv[i]]);
	}

	return argv;
}

PyObject* libetrace_nfsdb_entry_get_openfiles(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* opens = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->open_files_count; ++i) {
		PyObject* args = PyTuple_New(7);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
		PYTUPLE_SET_ULONG(args,1,i);
		PYTUPLE_SET_ULONG(args,2,__self->entry->open_files[i].path);
		if (__self->entry->open_files[i].original_path) {
			PYTUPLE_SET_ULONG(args,3,*(__self->entry->open_files[i].original_path));
		}
		else {
			PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
		}
		PYTUPLE_SET_ULONG(args,4,__self->entry->open_files[i].mode);
		PYTUPLE_SET_ULONG(args,5,__self->entry->open_files[i].size);
		PYTUPLE_SET_ULONG(args,6,__self->nfsdb_index);
		PyObject *openfile = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(opens,openfile);
	}

	return opens;
}

PyObject* libetrace_nfsdb_entry_get_openpaths(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* openpaths = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->open_files_count; ++i) {
		PyObject* py_openpath = PyUnicode_FromString(__self->nfsdb->string_table[__self->entry->open_files[i].path]);
		PYLIST_ADD_PYOBJECT(openpaths,py_openpath);
	}

	return openpaths;
}

static void fill_openfile_list_with_children(const struct nfsdb* nfsdb, PyObject* opens, const struct nfsdb_entry* entry) {

	for (unsigned long i=0; i<entry->open_files_count; ++i) {
		PyObject* args = PyTuple_New(7);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)nfsdb);
		PYTUPLE_SET_ULONG(args,1,i);
		PYTUPLE_SET_ULONG(args,2,entry->open_files[i].path);
		if (entry->open_files[i].original_path) {
			PYTUPLE_SET_ULONG(args,3,*(entry->open_files[i].original_path));
		}
		else {
			PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
		}
		PYTUPLE_SET_ULONG(args,4,entry->open_files[i].mode);
		PYTUPLE_SET_ULONG(args,5,entry->open_files[i].size);
		PYTUPLE_SET_ULONG(args,6,entry->nfsdb_index);
		PyObject *openfile = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(opens,openfile);
	}

	for (unsigned long i=0; i<entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->child_ids[i].pid);
		for (unsigned long j=0; j<node->entry_count; ++j) {
			struct nfsdb_entry* child_entry = node->entry_list[j];
			fill_openfile_list_with_children(nfsdb,opens,child_entry);
		}
	}
}

static void fill_wr_openfile_list_with_children(const struct nfsdb* nfsdb, PyObject* opens, const struct nfsdb_entry* entry) {

	for (unsigned long i=0; i<entry->open_files_count; ++i) {
		struct openfile* openfile = &entry->open_files[i];
		if ((openfile->mode&0x03)>0) {
			PyObject* args = PyTuple_New(7);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)nfsdb);
			PYTUPLE_SET_ULONG(args,1,i);
			PYTUPLE_SET_ULONG(args,2,entry->open_files[i].path);
			if (entry->open_files[i].original_path) {
				PYTUPLE_SET_ULONG(args,3,*(entry->open_files[i].original_path));
			}
			else {
				PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
			}
			PYTUPLE_SET_ULONG(args,4,entry->open_files[i].mode);
			PYTUPLE_SET_ULONG(args,5,entry->open_files[i].size);
			PYTUPLE_SET_ULONG(args,6,entry->nfsdb_index);
			PyObject *py_openfile = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
			Py_DECREF(args);
			PYLIST_ADD_PYOBJECT(opens,py_openfile);
		}
	}

	for (unsigned long i=0; i<entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->child_ids[i].pid);
		for (unsigned long j=0; j<node->entry_count; ++j) {
			struct nfsdb_entry* child_entry = node->entry_list[j];
			fill_wr_openfile_list_with_children(nfsdb,opens,child_entry);
		}
	}
}

static PyObject* libetrace_nfsdb_raw_entry_get_openfiles_with_children(const struct nfsdb* nfsdb, const struct nfsdb_entry* entry) {

	PyObject* opens = PyList_New(0);
	fill_openfile_list_with_children(nfsdb,opens,entry);
	return opens;
}

PyObject* libetrace_nfsdb_entry_get_openfiles_with_children(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	PyObject* opens = PyList_New(0);
	fill_openfile_list_with_children(__self->nfsdb,opens,__self->entry);
	return opens;
}

static void fill_openpath_list_with_children(const struct nfsdb* nfsdb, PyObject* openpaths, const struct nfsdb_entry* entry) {

	for (unsigned long i=0; i<entry->open_files_count; ++i) {
		PyObject* py_openpath = PyUnicode_FromString(nfsdb->string_table[entry->open_files[i].path]);
		PYSET_ADD_PYOBJECT(openpaths,py_openpath);
	}

	for (unsigned long i=0; i<entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->child_ids[i].pid);
		for (unsigned long j=0; j<node->entry_count; ++j) {
			struct nfsdb_entry* child_entry = node->entry_list[j];
			fill_openpath_list_with_children(nfsdb,openpaths,child_entry);
		}
	}
}

PyObject* libetrace_nfsdb_entry_get_openpaths_with_children(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	PyObject* openpaths = PySet_New(0);
	fill_openpath_list_with_children(__self->nfsdb,openpaths,__self->entry);
	return openpaths;
}

PyObject* libetrace_nfsdb_entry_filtered_openfiles(PyObject* self, PyObject *args) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (PyTuple_Size(args)<1) {
		PyErr_SetString(libetrace_nfsdbError, "No filter specification for openfiles");
		Py_RETURN_NONE;
	}

	unsigned long filter = PyLong_AsLong(PyTuple_GetItem(args,0));

	PyObject* filtered_opens = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->open_files_count; ++i) {
		if ((__self->entry->open_files[i].mode&filter)==filter) {
			PyObject* args = PyTuple_New(7);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
			PYTUPLE_SET_ULONG(args,1,i);
			PYTUPLE_SET_ULONG(args,2,__self->entry->open_files[i].path);
			if (__self->entry->open_files[i].original_path) {
				PYTUPLE_SET_ULONG(args,3,*(__self->entry->open_files[i].original_path));
			}
			else {
				PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
			}
			PYTUPLE_SET_ULONG(args,4,__self->entry->open_files[i].mode);
			PYTUPLE_SET_ULONG(args,5,__self->entry->open_files[i].size);
			PYTUPLE_SET_ULONG(args,6,(uintptr_t)self);
			PyObject *openfile = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
			Py_DECREF(args);
			PYLIST_ADD_PYOBJECT(filtered_opens,openfile);
		}
	}

	return filtered_opens;
}

PyObject* libetrace_nfsdb_entry_accessed_openfiles(PyObject* self, PyObject *args) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (PyTuple_Size(args)<1) {
		PyErr_SetString(libetrace_nfsdbError, "No filter specification for openfiles");
		Py_RETURN_NONE;
	}

	unsigned long filter = PyLong_AsLong(PyTuple_GetItem(args,0));

	PyObject* filtered_opens = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->open_files_count; ++i) {
		if ((__self->entry->open_files[i].mode&0x03)==filter) {
			PyObject* args = PyTuple_New(7);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
			PYTUPLE_SET_ULONG(args,1,i);
			PYTUPLE_SET_ULONG(args,2,__self->entry->open_files[i].path);
			if (__self->entry->open_files[i].original_path) {
				PYTUPLE_SET_ULONG(args,3,*(__self->entry->open_files[i].original_path));
			}
			else {
				PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
			}
			PYTUPLE_SET_ULONG(args,4,__self->entry->open_files[i].mode);
			PYTUPLE_SET_ULONG(args,5,__self->entry->open_files[i].size);
			PYTUPLE_SET_ULONG(args,6,(uintptr_t)self);
			PyObject *openfile = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
			Py_DECREF(args);
			PYLIST_ADD_PYOBJECT(filtered_opens,openfile);
		}
	}

	return filtered_opens;
}


PyObject* libetrace_nfsdb_entry_get_pcp(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->pcp) {
		Py_RETURN_NONE;
	}

	return PyBytes_FromStringAndSize((const char *)__self->entry->pcp,__self->entry->pcp_count);
}

PyObject* libetrace_nfsdb_entry_get_wrapper_pid(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (__self->entry->wrapper_pid==ULONG_MAX) {
		Py_RETURN_NONE;
	}

	return PyLong_FromLong(__self->entry->wrapper_pid);
}

PyObject* libetrace_nfsdb_entry_get_pipe_eids(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* pL = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->pipe_eids_count; ++i) {
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,__self->entry->pipe_eids[i].pid);
		PYTUPLE_SET_ULONG(args,1,__self->entry->pipe_eids[i].exeidx);
		PyObject *eid = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryEidType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(pL,eid);
	}

	return pL;
}

PyObject* libetrace_nfsdb_entry_get_linked_path(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->linked_file) {
		Py_RETURN_NONE;
	}

	return PyUnicode_FromString(__self->nfsdb->string_table[*__self->entry->linked_file]);
}

PyObject* libetrace_nfsdb_entry_get_linked_ptr(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->linked_file) {
		Py_RETURN_NONE;
	}

	const struct nfsdb_entry_file_index* lindex = &__self->entry->linked_index;
	PyObject* ptr = PyTuple_New(2);
	PyTuple_SetItem(ptr,0,PyLong_FromUnsignedLong(lindex->nfsdb_index));
	PyTuple_SetItem(ptr,1,PyLong_FromUnsignedLong(lindex->open_index));
	return ptr;
}

PyObject* libetrace_nfsdb_entry_get_linked_file(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->linked_file) {
		Py_RETURN_NONE;
	}

	const struct nfsdb_entry_file_index* lindex = &__self->entry->linked_index;
	libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
			&__self->nfsdb->nfsdb[lindex->nfsdb_index], lindex->open_index, lindex->nfsdb_index);

	return (PyObject*)openfile;
}

PyObject* libetrace_nfsdb_entry_get_linked_type(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->linked_file) {
		Py_RETURN_NONE;
	}

	return PyLong_FromLong(__self->entry->linked_type);
}

PyObject* libetrace_nfsdb_entry_get_compilation_info(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (!__self->entry->compilation_info) {
		Py_RETURN_NONE;
	}

	PyObject* args = PyTuple_New(3);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,(uintptr_t)__self->entry->compilation_info);
	PYTUPLE_SET_ULONG(args,2,__self->nfsdb_index);
	PyObject *ci = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryCompilationInfoType, args);
	Py_DECREF(args);

	return ci;
}

Py_hash_t libetrace_nfsdb_entry_hash(PyObject *o) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)o;
	return __self->nfsdb_index;
}

PyObject* libetrace_nfsdb_entry_richcompare(PyObject *self, PyObject *other, int op) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	libetrace_nfsdb_entry_object* __other = (libetrace_nfsdb_entry_object*)other;
	Py_RETURN_RICHCOMPARE(__self->nfsdb_index, __other->nfsdb_index , op);
}

void libetrace_nfsdb_iter_dealloc(libetrace_nfsdb_iter_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_iter_object* self;

    self = (libetrace_nfsdb_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->nfsdb = ((libetrace_nfsdb_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->nfsdb;
    	self->start = PyLong_AsLong(PyTuple_GetItem(args,1));
    	self->step = PyLong_AsLong(PyTuple_GetItem(args,2));
    	self->end = PyLong_AsLong(PyTuple_GetItem(args,3));
    }

    return (PyObject *)self;
}

Py_ssize_t libetrace_nfsdb_iter_sq_length(PyObject* self) {

	libetrace_nfsdb_iter_object* __self = (libetrace_nfsdb_iter_object*)self;
	return __self->nfsdb->nfsdb_count;
}

PyObject* libetrace_nfsdb_iter_next(PyObject *self) {

	libetrace_nfsdb_iter_object* __self = (libetrace_nfsdb_iter_object*)self;

	if (__self->start >= __self->end) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,__self->start);
	PyObject *entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
	Py_DecRef(args);
	__self->start+=__self->step;
	return entry;
}

void libetrace_nfsdb_filtered_iter_dealloc(libetrace_nfsdb_filtered_iter_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    free(self->filters);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_filtered_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_filtered_iter_object* self;

    self = (libetrace_nfsdb_filtered_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->nfsdb = ((libetrace_nfsdb_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->nfsdb;
    	self->start = PyLong_AsLong(PyTuple_GetItem(args,1));
    	self->step = PyLong_AsLong(PyTuple_GetItem(args,2));
    	self->end = PyLong_AsLong(PyTuple_GetItem(args,3));
    	PyObject* filterList = PyTuple_GetItem(args,4);
    	self->filters = 0;
    	self->filters_count = PyList_Size(filterList);
    	if (self->filters_count>0) {
    		self->filters = malloc(self->filters_count*sizeof(nfsdb_entry_filter_func));
    	}
    	for (Py_ssize_t i=0; i<PyList_Size(filterList); ++i) {
    		nfsdb_entry_filter_func fp = (nfsdb_entry_filter_func)PyLong_AsUnsignedLong(PyList_GetItem(filterList,i));
    		self->filters[i] = fp;
    	}
    	/* Set the iterator to the first entry that matches the filter */
    	while (self->start < self->end) {
    		const struct nfsdb_entry* entry = &self->nfsdb->nfsdb[self->start];
    		int match = 1;
			for (Py_ssize_t i=0; i<self->filters_count; ++i) {
				if (!((self->filters[i])(entry))) {
					match = 0;
					break;
				}
			}
			if (!match)
				self->start+=self->step;
			else
				break;
    	}
    }

    return (PyObject *)self;
}

Py_ssize_t libetrace_nfsdb_filtered_iter_sq_length(PyObject* self) {

	libetrace_nfsdb_filtered_iter_object* __self = (libetrace_nfsdb_filtered_iter_object*)self;

	Py_ssize_t entry_count = 0;
	for (unsigned long u=0; u<__self->nfsdb->nfsdb_count; ++u) {
		const struct nfsdb_entry* entry = &__self->nfsdb->nfsdb[u];
		int match = 1;
		for (Py_ssize_t i=0; i<__self->filters_count; ++i) {
			if (!((__self->filters[i])(entry))) {
				match = 0;
				break;
			}
		}
		if (match) entry_count++;
	}

	return entry_count;
}

PyObject* libetrace_nfsdb_filtered_iter_next(PyObject *self) {

	libetrace_nfsdb_filtered_iter_object* __self = (libetrace_nfsdb_filtered_iter_object*)self;

	if (__self->start >= __self->end) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,__self->start);
	PyObject *entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
	Py_DecRef(args);

	__self->start+=__self->step;
	while (__self->start < __self->end) {
		const struct nfsdb_entry* entry = &__self->nfsdb->nfsdb[__self->start];
		int match = 1;
		for (Py_ssize_t i=0; i<__self->filters_count; ++i) {
			if (!((__self->filters[i])(entry))) {
				match = 0;
				break;
			}
		}
		if (!match)
			__self->start+=__self->step;
		else
			break;
	}

	return entry;
}

void libetrace_nfsdb_opens_iter_dealloc(libetrace_nfsdb_opens_iter_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_opens_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_opens_iter_object* self;

    self = (libetrace_nfsdb_opens_iter_object*)subtype->tp_alloc(subtype, 0);
    if (self != 0) {
    	self->nfsdb = ((libetrace_nfsdb_object*)PyLong_AsLong(PyTuple_GetItem(args,0)))->nfsdb;
    	self->start = PyLong_AsLong(PyTuple_GetItem(args,1));
    	self->step = PyLong_AsLong(PyTuple_GetItem(args,2));
    	self->end = PyLong_AsLong(PyTuple_GetItem(args,3));
    	self->open_index = PyLong_AsLong(PyTuple_GetItem(args,4));

    	/* Set the iterator to the first openfile entry (if possible) */
    	while (self->start < self->end) {
			const struct nfsdb_entry* entry = &self->nfsdb->nfsdb[self->start];
			if (self->open_index>=entry->open_files_count) {
				self->open_index = 0;
				self->start++;
			}
			else break;
    	}
    }

    return (PyObject *)self;
}

Py_ssize_t libetrace_nfsdb_opens_iter_sq_length(PyObject* self) {

	libetrace_nfsdb_opens_iter_object* __self = (libetrace_nfsdb_opens_iter_object*)self;

	Py_ssize_t open_count = 0;
	for (unsigned long u=0; u<__self->nfsdb->nfsdb_count; ++u) {
		open_count+=__self->nfsdb->nfsdb[u].open_files_count;
	}

	return open_count;
}

PyObject* libetrace_nfsdb_opens_iter_next(PyObject *self) {

	libetrace_nfsdb_opens_iter_object* __self = (libetrace_nfsdb_opens_iter_object*)self;

	if (__self->start >= __self->end) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	const struct nfsdb_entry* entry = &__self->nfsdb->nfsdb[__self->start];
	libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(
			__self->nfsdb,entry,__self->open_index,__self->start);

	__self->open_index++;
	if (__self->open_index>=entry->open_files_count) {
		__self->open_index = 0;
		const struct nfsdb_entry* next_entry = 0;
		do {
			__self->start+=__self->step;
			if (__self->start >= __self->end) break;
			next_entry = &__self->nfsdb->nfsdb[__self->start];
		} while (next_entry->open_files_count<=0);
	}

	return (PyObject*)openfile;
}

void libetrace_nfsdb_entry_eid_dealloc(libetrace_nfsdb_entry_eid_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
	tp->tp_free(self);
}

PyObject* libetrace_nfsdb_entry_eid_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_entry_eid_object* self;

	if (PyTuple_Size(args)<1) {
		PyErr_SetString(libetrace_nfsdbError, "EidType constructor should have at least one argument (pid)");
		Py_RETURN_NONE;
	}

	self = (libetrace_nfsdb_entry_eid_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->pid = PyLong_AsLong(PyTuple_GetItem(args,0));
		if (PyTuple_Size(args)>1) {
			self->exeidx = PyLong_AsLong(PyTuple_GetItem(args,1));
		}
		else {
			self->exeidx = ULONG_MAX;
		}
	}

	return (PyObject *)self;
}

PyObject* libetrace_nfsdb_entry_eid_get_index(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_eid_object* __self = (libetrace_nfsdb_entry_eid_object*)self;

	if (__self->exeidx==ULONG_MAX) {
		PyErr_SetString(libetrace_nfsdbError, "No index specified in Eid object");
		Py_RETURN_NONE;
	}

	return PyLong_FromLong(__self->exeidx);
}

PyObject* libetrace_nfsdb_entry_eid_repr(PyObject* self) {

	static char repr[128];

	libetrace_nfsdb_entry_eid_object* __self = (libetrace_nfsdb_entry_eid_object*)self;

	int written = snprintf(repr,128,"<%lu",__self->pid);
	if (__self->exeidx<ULONG_MAX) {
		written+=snprintf(repr+written,128-written,":%lu>",__self->exeidx);
	}
	else {
		written+=snprintf(repr+written,128-written,">");
	}

	return PyUnicode_FromString(repr);
}

void libetrace_nfsdb_entry_cid_dealloc(libetrace_nfsdb_entry_cid_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_entry_cid_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_entry_cid_object* self;

	self = (libetrace_nfsdb_entry_cid_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->pid = PyLong_AsLong(PyTuple_GetItem(args,0));
		self->flags = PyLong_AsLong(PyTuple_GetItem(args,1));
	}

	return (PyObject *)self;
}

void libetrace_nfsdb_entry_openfile_dealloc(libetrace_nfsdb_entry_openfile_object* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_entry_openfile_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_entry_openfile_object* self;

	self = (libetrace_nfsdb_entry_openfile_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->nfsdb = (const struct nfsdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
		self->index = PyLong_AsUnsignedLong(PyTuple_GetItem(args,1));
		self->path = PyLong_AsUnsignedLong(PyTuple_GetItem(args,2));
		self->original_path = PyLong_AsUnsignedLong(PyTuple_GetItem(args,3));
		self->mode = PyLong_AsUnsignedLong(PyTuple_GetItem(args,4));
		self->size = PyLong_AsUnsignedLong(PyTuple_GetItem(args,5));
		self->parent = PyLong_AsUnsignedLong(PyTuple_GetItem(args,6));
	}

	return (PyObject *)self;
}

PyObject* libetrace_nfsdb_entry_openfile_get_path(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	return PyUnicode_FromString(__self->nfsdb->string_table[__self->path]);
}

PyObject* libetrace_nfsdb_entry_openfile_get_original_path(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	if (__self->original_path==ULONG_MAX) {
		return PyUnicode_FromString(__self->nfsdb->string_table[__self->path]);
	}

	return PyUnicode_FromString(__self->nfsdb->string_table[__self->original_path]);
}

PyObject* libetrace_nfsdb_entry_openfile_get_parent(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,__self->parent);
	PyObject *entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
	Py_DECREF(args);
	return entry;
}

PyObject* libetrace_nfsdb_entry_openfile_get_size(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	if ((__self->mode&0x40)!=0) {
		return PyLong_FromUnsignedLong(__self->size);
	}

	Py_RETURN_NONE;
}

PyObject* libetrace_nfsdb_entry_openfile_get_ptr(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	PyObject* ptr = PyTuple_New(2);
	PYTUPLE_SET_ULONG(ptr,0,__self->parent);
	PYTUPLE_SET_ULONG(ptr,1,__self->index);
	return ptr;
}

PyObject* libetrace_nfsdb_entry_openfile_path_modified(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	if (__self->original_path==ULONG_MAX) {
		Py_RETURN_FALSE;
	}

	Py_RETURN_TRUE;
}

PyObject* libetrace_nfsdb_entry_openfile_exists(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if ((self->mode&0x40)!=0) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_read(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if ((self->mode&0x01)==0) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_write(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if ((self->mode&0x03)>0) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_reg(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0x8)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_dir(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0x4)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_chr(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0x2)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_block(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0x6)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_symlink(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0xA)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_fifo(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0x1)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_socket(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	if (((self->mode&0x40)!=0)&&(((self->mode&0x3C)>>2)==0xC)) Py_RETURN_TRUE;

	Py_RETURN_FALSE;
}

Py_hash_t libetrace_nfsdb_entry_openfile_hash(PyObject *o) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)o;
	return _Py_HashBytes(&__self->parent,2*sizeof(unsigned long));
}

PyObject* libetrace_nfsdb_entry_openfile_richcompare(PyObject *self, PyObject *other, int op) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;
	libetrace_nfsdb_entry_openfile_object* __other = (libetrace_nfsdb_entry_openfile_object*)other;

    int cmp = memcmp(&__self->parent,&__other->parent,2*sizeof(unsigned long));

	switch (op) {
		case Py_EQ: if (cmp==0) Py_RETURN_TRUE; Py_RETURN_FALSE;
		case Py_NE: if (cmp!=0) Py_RETURN_TRUE; Py_RETURN_FALSE;
		case Py_LT: if (cmp<0) Py_RETURN_TRUE; Py_RETURN_FALSE;
		case Py_GT: if (cmp>0) Py_RETURN_TRUE; Py_RETURN_FALSE;
		case Py_LE: if (cmp<=0) Py_RETURN_TRUE; Py_RETURN_FALSE;
		case Py_GE: if (cmp>=0) Py_RETURN_TRUE; Py_RETURN_FALSE;
		default:
			Py_UNREACHABLE();
    }
}

libetrace_nfsdb_entry_openfile_object* libetrace_nfsdb_create_openfile_entry(const struct nfsdb* nfsdb, const struct nfsdb_entry* entry,
		unsigned long open_index, unsigned long nfsdb_index) {

	PyObject* args = PyTuple_New(7);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)nfsdb);
	PYTUPLE_SET_ULONG(args,1,open_index);
	PYTUPLE_SET_ULONG(args,2,entry->open_files[open_index].path);
	if (entry->open_files[open_index].original_path) {
		PYTUPLE_SET_ULONG(args,3,*(entry->open_files[open_index].original_path));
	}
	else {
		PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
	}
	PYTUPLE_SET_ULONG(args,4,entry->open_files[open_index].mode);
	PYTUPLE_SET_ULONG(args,5,entry->open_files[open_index].size);
	PYTUPLE_SET_ULONG(args,6,nfsdb_index);
	PyObject* openfile_entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
	Py_DECREF(args);
	return (libetrace_nfsdb_entry_openfile_object*)openfile_entry;
}

void libetrace_nfsdb_entry_compilation_info_dealloc(libetrace_nfsdb_entry_compilation_info* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_entry_compilation_info_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_entry_compilation_info* self;

	self = (libetrace_nfsdb_entry_compilation_info*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->nfsdb = (const struct nfsdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
		self->ci = (struct compilation_info*)PyLong_AsLong(PyTuple_GetItem(args,1));
		self->parent = PyLong_AsUnsignedLong(PyTuple_GetItem(args,2));
	}

	return (PyObject *)self;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_compiled_file_paths(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* compiled = PyList_New(0);

	for (unsigned long i=0; i<__self->ci->compiled_count; ++i) {
		PYLIST_ADD_STRING(compiled,__self->nfsdb->string_table[__self->ci->compiled_list[i]]);
	}

	return compiled;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_compiled_files(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* compiled = PyList_New(0);
	for (unsigned long i=0; i<__self->ci->compiled_count; ++i) {
		struct nfsdb_entry_file_index* cindex = &__self->ci->compiled_index[i];
		libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
				&__self->nfsdb->nfsdb[cindex->nfsdb_index], cindex->open_index, cindex->nfsdb_index);
		PYLIST_ADD_PYOBJECT(compiled,(PyObject*)openfile);
	}

	return compiled;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_object_file_paths(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* objects = PyList_New(0);

	for (unsigned long i=0; i<__self->ci->object_list_count; ++i) {
		PYLIST_ADD_STRING(objects,__self->nfsdb->string_table[__self->ci->object_list[i]]);
	}

	return objects;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_object_files(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* objects = PyList_New(0);
	for (unsigned long i=0; i<__self->ci->object_list_count; ++i) {
		struct nfsdb_entry_file_index* cindex = &__self->ci->object_index[i];
		libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
				&__self->nfsdb->nfsdb[cindex->nfsdb_index], cindex->open_index, cindex->nfsdb_index);
		PYLIST_ADD_PYOBJECT(objects,(PyObject*)openfile);
	}

	return objects;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_header_paths(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* includes = PyList_New(0);

	for (unsigned long i=0; i<__self->ci->header_list_count; ++i) {
		PYLIST_ADD_STRING(includes,__self->nfsdb->string_table[__self->ci->header_list[i]]);
	}

	return includes;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_headers(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* objects = PyList_New(0);
	for (unsigned long i=0; i<__self->ci->header_list_count; ++i) {
		struct nfsdb_entry_file_index* cindex = &__self->ci->header_index[i];
		libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
				&__self->nfsdb->nfsdb[cindex->nfsdb_index], cindex->open_index, cindex->nfsdb_index);
		PYLIST_ADD_PYOBJECT(objects,(PyObject*)openfile);
	}

	return objects;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_include_paths(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* ipaths = PyList_New(0);

	for (unsigned long i=0; i<__self->ci->include_paths_count; ++i) {
		PYLIST_ADD_STRING(ipaths,__self->nfsdb->string_table[__self->ci->include_paths[i]]);
	}

	return ipaths;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_type(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	return PyLong_FromLong(__self->ci->compilation_type);
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_defines(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* defs = PyList_New(0);

	for (unsigned long i=0; i<__self->ci->pp_defs_count; ++i) {
		struct pp_def* def = &__self->ci->pp_defs[i];
		PyObject* T = PyTuple_New(2);
		PYTUPLE_SET_STR(T,0,__self->nfsdb->string_table[def->name]);
		PYTUPLE_SET_STR(T,1,__self->nfsdb->string_table[def->value]);
		PYLIST_ADD_PYOBJECT(defs,T);
	}

	return defs;
}

PyObject* libetrace_nfsdb_entry_compilation_info_get_undefs(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_compilation_info* __self = (libetrace_nfsdb_entry_compilation_info*)self;

	PyObject* udefs = PyList_New(0);

	for (unsigned long i=0; i<__self->ci->pp_udefs_count; ++i) {
		struct pp_def* def = &__self->ci->pp_udefs[i];
		PyObject* T = PyTuple_New(2);
		PYTUPLE_SET_STR(T,0,__self->nfsdb->string_table[def->name]);
		PYTUPLE_SET_STR(T,1,__self->nfsdb->string_table[def->value]);
		PYLIST_ADD_PYOBJECT(udefs,T);
	}

	return udefs;
}

PyObject* libetrace_nfsdb_entry_compilation_info_is_integrated(libetrace_nfsdb_entry_compilation_info *self, PyObject *args) {

	if (self->ci->integrated_compilation) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

void libetrace_nfsdb_filemap_dealloc(libetrace_nfsdb_filemap* self) {

    PyTypeObject *tp = Py_TYPE(self);
    tp->tp_free(self);
}

PyObject* libetrace_nfsdb_filemap_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_filemap* self;

	self = (libetrace_nfsdb_filemap*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->nfsdb = (const struct nfsdb*)PyLong_AsLong(PyTuple_GetItem(args,0));
	}

	return (PyObject *)self;
}

static void libetrace_nfsdb_fill_entry_list(libetrace_nfsdb_filemap* self, PyObject* fill_entries,
		struct nfsdb_entry** entry_list, unsigned long* entry_index, unsigned long entry_count) {

	for (unsigned long i=0; i<entry_count; ++i) {
		struct nfsdb_entry* entry = entry_list[i];
		unsigned long index = entry_index[i];
		PyObject* args = PyTuple_New(7);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->nfsdb);
		PYTUPLE_SET_ULONG(args,1,index);
		PYTUPLE_SET_ULONG(args,2,entry->open_files[index].path);
		if (entry->open_files[index].original_path) {
			PYTUPLE_SET_ULONG(args,3,*(entry->open_files[index].original_path));
		}
		else {
			PYTUPLE_SET_ULONG(args,3,ULONG_MAX);
		}
		PYTUPLE_SET_ULONG(args,4,entry->open_files[index].mode);
		PYTUPLE_SET_ULONG(args,5,entry->open_files[index].size);
		PYTUPLE_SET_ULONG(args,6,entry->nfsdb_index);
		PyObject *openfile_entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(fill_entries,openfile_entry);
	}
}

PyObject* libetrace_nfsdb_filemap_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libetrace_nfsdb_filemap* __self = (libetrace_nfsdb_filemap*)self;

	if (PyUnicode_Check(slice)) {
		const char* fpath = PyString_get_c_str(slice);
		struct stringRefMap_node* srefnode = stringRefMap_search(&__self->nfsdb->revstringmap, fpath);
		PYASSTR_DECREF(fpath);
		if (!srefnode) {
			return PyList_New(0);
		}
		unsigned long hfpath = srefnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&__self->nfsdb->filemap,hfpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at file path handle [%lu]",hfpath);
		PyObject* rL = PyList_New(0);
		libetrace_nfsdb_fill_entry_list(__self,rL,node->rd_entry_list,node->rd_entry_index,node->rd_entry_count);
		libetrace_nfsdb_fill_entry_list(__self,rL,node->rw_entry_list,node->rw_entry_index,node->rw_entry_count);
		libetrace_nfsdb_fill_entry_list(__self,rL,node->wr_entry_list,node->wr_entry_index,node->wr_entry_count);
		return rL;
	}
	else if (PyTuple_Check(slice)) {
		ASSERT_WITH_NFSDB_ERROR(PyTuple_Size(slice)==2,"Invalid tuple argument");
		const char* fpath = PyString_get_c_str(PyTuple_GetItem(slice,0));
		unsigned long filter_mode = PyLong_AsLong(PyTuple_GetItem(slice,1));
		ASSERT_WITH_NFSDB_FORMAT_ERROR(filter_mode<=2,"Invalid file access mode: [%lu]",filter_mode);
		struct stringRefMap_node* srefnode = stringRefMap_search(&__self->nfsdb->revstringmap, fpath);
		PYASSTR_DECREF(fpath);
		if (!srefnode) {
			return PyList_New(0);
		}
		unsigned long hfpath = srefnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&__self->nfsdb->filemap,hfpath);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at file path handle [%lu]",hfpath);
		PyObject* rL = PyList_New(0);
		if (filter_mode==0) {
			libetrace_nfsdb_fill_entry_list(__self,rL,node->rd_entry_list,node->rd_entry_index,node->rd_entry_count);
		}
		else if (filter_mode==1) {
			libetrace_nfsdb_fill_entry_list(__self,rL,node->wr_entry_list,node->wr_entry_index,node->wr_entry_count);
		}
		else {
			libetrace_nfsdb_fill_entry_list(__self,rL,node->rw_entry_list,node->rw_entry_index,node->rw_entry_count);
		}
		return rL;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid filemap subscript type (not a string or slice)");
	return 0;
}

PyMODINIT_FUNC
PyInit_libetrace(void)
{
    PyObject* m;

    m = PyModule_Create(&libetrace_module);
    if (m==0) return 0;

    if (PyType_Ready(&libetrace_nfsdbType) < 0)
        return 0;
    Py_XINCREF(&libetrace_nfsdbType);

    if (PyType_Ready(&libetrace_nfsdbIterType) < 0)
        return 0;
    Py_XINCREF(&libetrace_nfsdbIterType);

    if (PyType_Ready(&libetrace_nfsdbFilteredIterType) < 0)
        return 0;
    Py_XINCREF(&libetrace_nfsdbFilteredIterType);

    if (PyType_Ready(&libetrace_nfsdbOpensIterType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbOpensIterType);

    if (PyType_Ready(&libetrace_nfsdbEntryType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbEntryType);

	if (PyType_Ready(&libetrace_nfsdbEntryEidType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbEntryEidType);

	if (PyType_Ready(&libetrace_nfsdbEntryCidType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbEntryCidType);

	if (PyType_Ready(&libetrace_nfsdbEntryOpenfileType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbEntryOpenfileType);

	if (PyType_Ready(&libetrace_nfsdbEntryCompilationInfoType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbEntryCompilationInfoType);

	if (PyType_Ready(&libetrace_nfsdbFileMapType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbFileMapType);

    COMPILER_READY(gcc);
    COMPILER_READY(clang);
    COMPILER_READY(armcc);

    if (PyModule_AddObject(m, "nfsdb", (PyObject *)&libetrace_nfsdbType)<0) {
    	Py_DECREF(m);
    	return 0;
    }

    if (PyModule_AddObject(m, "eid", (PyObject *)&libetrace_nfsdbEntryEidType)<0) {
		Py_DECREF(m);
		return 0;
	}

    if (PyModule_AddObject(m, "nfsdbEntryOpenfile", (PyObject *)&libetrace_nfsdbEntryOpenfileType)<0) {
    	Py_DECREF(m);
    	return 0;
    }

    if (PyModule_AddObject(m, "nfsdbEntry", (PyObject *)&libetrace_nfsdbEntryType)<0) {
		Py_DECREF(m);
		return 0;
    }

    COMPILER_REGISTER(gcc);
    COMPILER_REGISTER(clang);
    COMPILER_REGISTER(armcc);

    libetrace_nfsdbError = PyErr_NewException("libetrace.error",0,0);
    Py_XINCREF(libetrace_nfsdbError);
    if (PyModule_AddObject(m, "error", libetrace_nfsdbError) < 0) {
    	Py_XDECREF(libetrace_nfsdbError);
		Py_CLEAR(libetrace_nfsdbError);
		Py_DECREF(m);
    }

    return m;
}
