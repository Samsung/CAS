#include "pyetrace.h"
#include "compiler.h"

#include <fcntl.h>
#include <stdbool.h>
#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include "uflat.h"

DEFINE_COMPILER(gcc);
DEFINE_COMPILER(clang);
DEFINE_COMPILER(armcc);

volatile int interrupt;
void intHandler(int v) {
	interrupt = 1;
}

nfsdb_entry_openfile_filter_func_array_t nfsdb_entry_openfile_filter_func_array[] = {
		{"contains_path",libetrace_nfsdb_entry_openfile_filter_contains_path},
		{"matches_wc",libetrace_nfsdb_entry_openfile_filter_matches_wc},
		{"matches_re",libetrace_nfsdb_entry_openfile_filter_matches_re},
		{"is_class",libetrace_nfsdb_entry_openfile_filter_is_class},
		{"file_exists",libetrace_nfsdb_entry_openfile_filter_file_exists},
		{"file_not_exists",libetrace_nfsdb_entry_openfile_filter_file_not_exists},
		{"dir_exists",libetrace_nfsdb_entry_openfile_filter_dir_exists},
		{"has_access",libetrace_nfsdb_entry_openfile_filter_has_access},
		{"at_source_root",libetrace_nfsdb_entry_openfile_filter_at_source_root},
		{"not_at_source_root",libetrace_nfsdb_entry_openfile_filter_not_at_source_root},
		{"source_type",libetrace_nfsdb_entry_openfile_filter_source_type},
};

nfsdb_entry_command_filter_func_array_t nfsdb_entry_command_filter_func_array[] = {
		{"cwd_contains_path",libetrace_nfsdb_entry_command_filter_cwd_contains_path},
		{"cwd_matches_wc",libetrace_nfsdb_entry_command_filter_cwd_matches_wc},
		{"cwd_matches_re",libetrace_nfsdb_entry_command_filter_cwd_matches_re},
		{"cmd_has_string",libetrace_nfsdb_entry_command_filter_cmd_has_string},
		{"cmd_matches_wc",libetrace_nfsdb_entry_command_filter_cmd_matches_wc},
		{"cmd_matches_re",libetrace_nfsdb_entry_command_filter_cmd_matches_re},
		{"bin_contains_path",libetrace_nfsdb_entry_command_filter_bin_contains_path},
		{"bin_matches_wc",libetrace_nfsdb_entry_command_filter_bin_matches_wc},
		{"bin_matches_re",libetrace_nfsdb_entry_command_filter_bin_matches_re},
		{"has_ppid",libetrace_nfsdb_entry_command_filter_has_ppid},
		{"is_class",libetrace_nfsdb_entry_command_filter_is_class},
		{"bin_at_source_root",libetrace_nfsdb_entry_command_filter_bin_at_source_root},
		{"bin_not_at_source_root",libetrace_nfsdb_entry_command_filter_bin_not_at_source_root},
		{"cwd_at_source_root",libetrace_nfsdb_entry_command_filter_cwd_at_source_root},
		{"cwd_not_at_source_root",libetrace_nfsdb_entry_command_filter_cwd_not_at_source_root},
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
		PyObject* py_space = PyUnicode_FromString(" ");
		for (Py_ssize_t v=0; v<PyList_Size(eL); ++v) {
			PyObject* e = PyList_GetItem(eL,v);
			PyObject* cmdv = PyDict_GetItem(e,vkey);
			PyObject* cmds = PyUnicode_Join(py_space,cmdv);
			const char* cmdstr = PyString_get_c_str(cmds);
			cmdi++;
			if (isatty(fileno(stdin))){
				if ((cmdi%1000)==0) {
					DBG(1,"\rPrecomputing exclude command patterns...%lu%%",(cmdi*100)/cmd_count);
				}
			}
			size_t bsize = (excl_commands_size-1)/8+1;
			unsigned char* b = calloc(bsize,1);
			assert(b!=0 && "Out of memory for allocating precompute pattern map");
			size_t k;
			for (k=0; k<excl_commands_size; ++k) {
				unsigned byte_index = k/8;
				unsigned bit_index = k%8;
				if (pattern_match_single(cmdstr,excl_commands[k])) {
					b[byte_index]|=0x01<<(7-bit_index);
				}
			}
			PYASSTR_DECREF(cmdstr);
			Py_DecRef(cmds);
			PyObject* pidext = PyTuple_New(2);
			PyTuple_SetItem(pidext, 0,Py_BuildValue("l",pid));
			PyTuple_SetItem(pidext, 1,Py_BuildValue("l",v));
			PyObject* bytes = PyBytes_FromStringAndSize((const char*)b,bsize);
			PyDict_SetItem(pcp_map, pidext, bytes);
			Py_DecRef(pidext);
			Py_DecRef(bytes);
			free(b);
		}
		Py_DecRef(py_space);
		if (interrupt) {
			goto interrupted;
		}
	}
    DBG(1,"\n");
	Py_DecRef(keys);
    Py_DecRef(vkey);
    for (size_t u=0; u<excl_commands_size; ++u) {
    	PYASSTR_DECREF(excl_commands[u]);
    }
	free(excl_commands);
	return pcp_map;
interrupted:
	Py_DecRef(keys);
    Py_DecRef(vkey);
    for (size_t u=0; u<excl_commands_size; ++u) {
    	PYASSTR_DECREF(excl_commands[u]);
    }
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

FUNCTION_DEFINE_FLATTEN_STRUCT(eid);
FUNCTION_DEFINE_FLATTEN_STRUCT(cid);
FUNCTION_DEFINE_FLATTEN_STRUCT(pp_def);
FUNCTION_DEFINE_FLATTEN_STRUCT(nfsdb_entry_file_index);

FUNCTION_DECLARE_FLATTEN_STRUCT(nfsdb_entry);

FUNCTION_DEFINE_FLATTEN_STRUCT(openfile,
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,original_path,1);
	AGGREGATE_FLATTEN_STRUCT_ARRAY(nfsdb_entry,opaque_entry,1);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(compilation_info,
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,compiled_list,ATTR(compiled_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(nfsdb_entry_file_index,compiled_index,ATTR(compiled_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,include_paths,ATTR(include_paths_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(pp_def,pp_defs,ATTR(pp_defs_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(pp_def,pp_udefs,ATTR(pp_udefs_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,header_list,ATTR(header_list_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(nfsdb_entry_file_index,header_index,ATTR(header_list_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,object_list,ATTR(object_list_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(nfsdb_entry_file_index,object_index,ATTR(object_list_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(nfsdb_entry,
	AGGREGATE_FLATTEN_STRUCT_ARRAY(cid,child_ids,ATTR(child_ids_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,argv,ATTR(argv_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(openfile,open_files,ATTR(open_files_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned char,pcp,ATTR(pcp_count));
	AGGREGATE_FLATTEN_STRUCT_ARRAY(eid,pipe_eids,ATTR(pipe_eids_count));
	AGGREGATE_FLATTEN_STRUCT(compilation_info,compilation_info);
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,linked_file,1);
);

FUNCTION_DECLARE_FLATTEN_STRUCT(nfsdb_entryMap_node);

FUNCTION_DEFINE_FLATTEN_STRUCT(nfsdb_entryMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_EMBEDDED_POINTER(nfsdb_entryMap_node,node.__rb_parent_color,
			ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_entryMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_entryMap_node,node.rb_left);
	AGGREGATE_FLATTEN_TYPE_ARRAY(struct nfsdb_entry*,entry_list,ATTR(entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(entry_list),ATTR(entry_count),
		FLATTEN_STRUCT(nfsdb_entry,entry);
	);
);

FUNCTION_DECLARE_FLATTEN_STRUCT(ulongMap_node);


FUNCTION_DEFINE_FLATTEN_STRUCT(ulongMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_EMBEDDED_POINTER(ulongMap_node,node.__rb_parent_color,
			ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,node.rb_left);
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,value_list,ATTR(value_count));
);

FUNCTION_DECLARE_FLATTEN_STRUCT(stringRefMap_node);

FUNCTION_DEFINE_FLATTEN_STRUCT(stringRefMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_EMBEDDED_POINTER(stringRefMap_node,node.__rb_parent_color,
			ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
	AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,node.rb_left);
	AGGREGATE_FLATTEN_STRING(key);
);

FUNCTION_DECLARE_FLATTEN_STRUCT(nfsdb_fileMap_node);


FUNCTION_DEFINE_FLATTEN_STRUCT(nfsdb_fileMap_node,
	STRUCT_ALIGN(4);
	AGGREGATE_FLATTEN_STRUCT_EMBEDDED_POINTER(nfsdb_fileMap_node,node.__rb_parent_color,
			ptr_clear_2lsb_bits,flatten_ptr_restore_2lsb_bits);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_fileMap_node,node.rb_right);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_fileMap_node,node.rb_left);
	AGGREGATE_FLATTEN_TYPE_ARRAY(struct nfsdb_entry*,rd_entry_list,ATTR(rd_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(rd_entry_list),ATTR(rd_entry_count),
		FLATTEN_STRUCT(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,rd_entry_index,ATTR(rd_entry_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(struct nfsdb_entry*,wr_entry_list,ATTR(wr_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(wr_entry_list),ATTR(wr_entry_count),
		FLATTEN_STRUCT(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,wr_entry_index,ATTR(wr_entry_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(struct nfsdb_entry*,rw_entry_list,ATTR(rw_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(rw_entry_list),ATTR(rw_entry_count),
		FLATTEN_STRUCT(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,rw_entry_index,ATTR(rw_entry_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(struct nfsdb_entry*,ga_entry_list,ATTR(ga_entry_count));
	FOREACH_POINTER(struct nfsdb_entry*,entry,ATTR(ga_entry_list),ATTR(ga_entry_count),
		FLATTEN_STRUCT(nfsdb_entry,entry);
	);
	AGGREGATE_FLATTEN_TYPE_ARRAY(unsigned long,ga_entry_index,ATTR(ga_entry_count));
);

FUNCTION_DEFINE_FLATTEN_STRUCT(nfsdb,
	AGGREGATE_FLATTEN_STRUCT_ARRAY(nfsdb_entry,nfsdb,ATTR(nfsdb_count));
	AGGREGATE_FLATTEN_STRING(source_root);
	AGGREGATE_FLATTEN_STRING(dbversion);
	AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,string_table,ATTR(string_count));
	FOREACH_POINTER(const char*,s,ATTR(string_table),ATTR(string_count),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_TYPE_ARRAY(uint32_t,string_size_table,ATTR(string_count));
	AGGREGATE_FLATTEN_TYPE_ARRAY(const char*,pcp_pattern_list,ATTR(pcp_pattern_list_size));
	FOREACH_POINTER(const char*,s,ATTR(pcp_pattern_list),ATTR(pcp_pattern_list_size),
		FLATTEN_STRING(s);
	);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_entryMap_node,procmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_entryMap_node,bmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,forkmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,revforkmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,pipemap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,wrmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,rdmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(stringRefMap_node,revstringmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_fileMap_node,filemap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(nfsdb_entryMap_node,linkedmap.rb_node);
);

FUNCTION_DEFINE_FLATTEN_STRUCT(nfsdb_deps,
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,depmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,ddepmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,revdepmap.rb_node);
	AGGREGATE_FLATTEN_STRUCT(ulongMap_node,revddepmap.rb_node);
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
		if (node) {
			for (unsigned long j=0; j<node->entry_count; ++j) {
				struct nfsdb_entry* child_entry = node->entry_list[j];
				if (scan_openfile_list_with_children(nfsdb,child_entry,cb,root_entry,arg)) return 1;
			}
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
		if (node) {
			for (unsigned long j=0; j<node->entry_count; ++j) {
				struct nfsdb_entry* child_entry = node->entry_list[j];
				if (scan_wr_openfile_list_with_children(nfsdb,child_entry,cb,root_entry,arg)) return 1;
			}
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
		entry->linked_index.__used = 1;
		openfile->opaque_entry = entry;
		return 1;
	}

	return 0;
}

void libetrace_nfsdb_entry_precompute_linked_file(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		if (!scan_wr_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_linked_file,entry,0)) {
		}
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
		entry->compilation_info->compiled_index[*compiled_index].__used = 1;
		openfile->opaque_entry = entry;
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
		entry->compilation_info->header_index[*compiled_index].__used = 1;
		openfile->opaque_entry = entry;
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
		entry->compilation_info->object_index[*compiled_index].__used = 1;
		openfile->opaque_entry = entry;
		return 1;
	}

	return 0;
}

void libetrace_nfsdb_entry_precompute_compilation_info_files(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		for (unsigned long u=0; u<entry->compilation_info->compiled_count; ++u) {
			if (!scan_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_compiled_file,entry,&u)) {
			}
		}
	}
}

void libetrace_nfsdb_entry_precompute_compilation_info_headers(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		for (unsigned long u=0; u<entry->compilation_info->header_list_count; ++u) {
			if (!scan_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_header_file,entry,&u)) {
			}
		}
	}
}

void libetrace_nfsdb_entry_precompute_compilation_info_objects(struct nfsdb* nfsdb, struct nfsdb_entry* entry) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&nfsdb->procmap,entry->eid.pid);

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = node->entry_list[i];
		for (unsigned long u=0; u<entry->compilation_info->object_list_count; ++u) {
			if (!scan_wr_openfile_list_with_children(nfsdb,nfsdb_entry,scan_openfile_precompute_object_file,entry,&u)) {
			}
		}
	}
}

PyObject * libetrace_create_nfsdb(PyObject *self, PyObject *args, PyObject* kwargs) {

	PyObject* stringMap = PyDict_New();
	PyObject* nfsdbJSON = PyTuple_GetItem(args,0);
	PyObject* source_root = PyTuple_GetItem(args,1);
	PyObject* dbversion = PyTuple_GetItem(args,2);
	PyObject* pcp_patterns = PyTuple_GetItem(args,3);
	PyObject* shared_argvs = PyTuple_GetItem(args,4);
	PyObject* dbfn = PyTuple_GetItem(args,5);
	int show_stats = 0, verbose_mode = 0, debug_mode = 0;
	if (PyTuple_Size(args)>6) {
		PyObject* show_stats_arg = PyTuple_GetItem(args,6);
		if (show_stats_arg==Py_True) {
			show_stats = 1;
		}
	}

	PyObject* py_debug = PyUnicode_FromString("verbose");
	PyObject* py_quiet = PyUnicode_FromString("debug");
	if (kwargs) {
		if (PyDict_Contains(kwargs, py_debug))
			verbose_mode = !!PyLong_AsLong(PyDict_GetItem(kwargs, py_debug));
		if (PyDict_Contains(kwargs, py_quiet))
			debug_mode = !!PyLong_AsLong(PyDict_GetItem(kwargs, py_quiet));
	}
	Py_DecRef(py_debug);
	Py_DecRef(py_quiet);

	PyObject* osModuleString = PyUnicode_FromString((char*)"os.path");
	PyObject* osModule = PyImport_Import(osModuleString);
	PyObject* pathJoinFunction = PyObject_GetAttrString(osModule,(char*)"join");

	struct nfsdb nfsdb = {0};
	nfsdb.db_magic = NFSDB_MAGIC_NUMBER;
	nfsdb.db_version = LIBETRACE_VERSION;

	nfsdb.source_root = PyString_get_c_str(source_root);
	nfsdb.source_root_size = strlen(nfsdb.source_root);
	nfsdb.dbversion = PyString_get_c_str(dbversion);
	if (PyList_Size(nfsdbJSON)<=0) goto flatten_start;
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
	nfsdb.shared_argv_list_size = PyList_Size(shared_argvs);
	nfsdb.shared_argv_list = malloc(nfsdb.shared_argv_list_size*sizeof(unsigned long));
	for (Py_ssize_t i=0; i<nfsdb.shared_argv_list_size; ++i) {
		PyObject* p = PyList_GetItem(shared_argvs,i);
		nfsdb.shared_argv_list[i] = string_table_add(&nfsdb, p, stringMap);
	}
	nfsdb.nfsdb_count = PyList_Size(nfsdbJSON);
	nfsdb.nfsdb = calloc(nfsdb.nfsdb_count,sizeof(struct nfsdb_entry));

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
		/* return code */
		if (PyDict_Contains(item, PyUnicode_FromString("!"))) {
			new_entry->return_code = PyLong_AsLong(PyDict_GetItem(item, PyUnicode_FromString("!")));
		}
		/* Open files */
		PyObject* openEntryList = PyDict_GetItem(item, PyUnicode_FromString("o"));
		new_entry->open_files_count = PyList_Size(openEntryList);
		new_entry->open_files = calloc(new_entry->open_files_count,sizeof(struct openfile));
		for (ssize_t u=0; u<PyList_Size(openEntryList); ++u) {
			PyObject* openEntry = PyList_GetItem(openEntryList,u);
			PyObject* openPath = PyDict_GetItem(openEntry, PyUnicode_FromString("p"));
			PyObject* originalPath = 0;
			if (PyDict_Contains(openEntry, PyUnicode_FromString("o"))) {
				originalPath = PyDict_GetItem(openEntry, PyUnicode_FromString("o"));
			}
			new_entry->open_files[u].path = string_table_add(&nfsdb, openPath, stringMap);
			new_entry->open_files[u].mode = PyLong_AsUnsignedLong(PyDict_GetItem(openEntry, PyUnicode_FromString("m")));
			if (PyDict_Contains(openEntry, PyUnicode_FromString("s"))) {
				new_entry->open_files[u].size = PyLong_AsUnsignedLong(PyDict_GetItem(openEntry, PyUnicode_FromString("s")));
			}
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
			new_entry->compilation_info->compiled_index = calloc(1,new_entry->compilation_info->compiled_count*sizeof(struct nfsdb_entry_file_index));
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
			new_entry->compilation_info->header_index = calloc(1,new_entry->compilation_info->header_list_count*sizeof(struct nfsdb_entry_file_index));
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
			new_entry->compilation_info->object_index = calloc(1,new_entry->compilation_info->object_list_count*sizeof(struct nfsdb_entry_file_index));
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
		/* do we have -shared arguments in the entry? */
		for (unsigned long u=0; u<new_entry->argv_count; ++u) {
			for (unsigned long j=0; j<nfsdb.shared_argv_list_size; ++j) {
				if (new_entry->argv[u]==nfsdb.shared_argv_list[j]) {
					new_entry->has_shared_argv = 1;
					break;
				}
			}
			if (new_entry->has_shared_argv) break;
		}
		if (PyErr_Occurred()) {
			printf("Exception while processing nfsdb entry at index %ld\n",i);
			PyErr_PrintEx(0);
			PyErr_Clear();
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

	/* If there is an opaque entry in the global access list place it at the beginning of the list */
	struct rb_node * p = rb_first(&nfsdb.filemap);
	while(p) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		unsigned long vu;
		for (vu=0; vu<data->ga_entry_count; ++vu) {
			struct openfile* openfile = &data->ga_entry_list[vu]->open_files[data->ga_entry_index[vu]];
			if (openfile->opaque_entry) {
				break;
			}
		}
		if (vu<data->ga_entry_count) {
			struct nfsdb_entry* first_entry = data->ga_entry_list[0];
			unsigned long first_index = data->ga_entry_index[0];
			data->ga_entry_list[0] = data->ga_entry_list[vu];
			data->ga_entry_index[0] = data->ga_entry_index[vu];
			data->ga_entry_list[vu] = first_entry;
			data->ga_entry_index[vu] = first_index;
		}
		p = rb_next(p);
	}

flatten_start: ;
	const char* dbfn_s =  PyString_get_c_str(dbfn);
	struct uflat* uflat = uflat_init(dbfn_s);
	PYASSTR_DECREF(dbfn_s);
	if(uflat == NULL) {
		printf("uflat_init(): failed\n");
		goto uflat_error_exit;
	}

	int rv = uflat_set_option(uflat, UFLAT_OPT_OUTPUT_SIZE, 50ULL * 1024 * 1024 * 1024);
	if(rv) {
		printf("uflat_set_option(OUTPUT_SIZE): %d\n", rv);
		goto uflat_error_exit;
	}

	rv = uflat_set_option(uflat, UFLAT_OPT_SKIP_MEM_FRAGMENTS, 1);
	if(rv) {
		printf("uflat_set_option(SKIP_MEM_FRAGMENTS)\n");
		goto uflat_error_exit;
	}

	if(verbose_mode)
		uflat_set_option(uflat, UFLAT_OPT_VERBOSE, 1);
	if(debug_mode)
		uflat_set_option(uflat, UFLAT_OPT_DEBUG, 1);

	FOR_ROOT_POINTER(&nfsdb,
		FLATTEN_STRUCT(nfsdb, &nfsdb);
	);

	int err = uflat_write(uflat);
	if (err != 0) {
		printf("flatten_write(): %d\n", err);
		goto uflat_error_exit;
	}

	uflat_fini(uflat);
	destroy_nfsdb(&nfsdb);

	Py_DECREF(pathJoinFunction);
	Py_DECREF(osModule);
	Py_DECREF(osModuleString);

	Py_RETURN_TRUE;

uflat_error_exit:
	if(uflat != NULL)
		uflat_fini(uflat);
	destroy_nfsdb(&nfsdb);

	Py_DECREF(pathJoinFunction);
	Py_DECREF(osModule);
	Py_DECREF(osModuleString);
	Py_RETURN_FALSE;
}

static void destroy_nfsdb_deps(struct nfsdb_deps* nfsdb_deps) {

	// TODO
}

PyObject* libetrace_nfsdb_create_deps_cache(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	PyObject* depmap = PyTuple_GetItem(args,0);
	PyObject* ddepmap = PyTuple_GetItem(args,1);
	PyObject* dbfn = PyTuple_GetItem(args,2);
	int show_stats = 0, verbose_mode = 0, debug_mode = 0;
	if (PyTuple_Size(args)>3) {
		PyObject* show_stats_arg = PyTuple_GetItem(args,3);
		if (show_stats_arg==Py_True) {
			show_stats = 1;
		}
	}

	PyObject* py_debug = PyUnicode_FromString("verbose");
	PyObject* py_quiet = PyUnicode_FromString("debug");
	if (kwargs) {
		if (PyDict_Contains(kwargs, py_debug))
			verbose_mode = !!PyLong_AsLong(PyDict_GetItem(kwargs, py_debug));
		if (PyDict_Contains(kwargs, py_quiet))
			debug_mode = !!PyLong_AsLong(PyDict_GetItem(kwargs, py_quiet));
	}
	Py_DecRef(py_debug);
	Py_DecRef(py_quiet);

	struct nfsdb_deps nfsdb_deps = {0};
	nfsdb_deps.db_magic = NFSDB_DEPS_MAGIC_NUMBER;
	nfsdb_deps.db_version = LIBETRACE_VERSION;

	/* Make a cache string table object */
	PyObject* stringTable = PyDict_New();
	for (unsigned long u=0; u<self->nfsdb->string_count; ++u) {
		PyObject* s = PyUnicode_FromString(self->nfsdb->string_table[u]);
		PyObject* index = PyLong_FromUnsignedLong(u);
		PyDict_SetItem(stringTable, s, index);
		Py_DecRef(s);
		Py_DecRef(index);
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
			values[2*j+0] = PyLong_AsUnsignedLong(PyTuple_GetItem(dtuple,0));
			values[2*j+1] = PyLong_AsUnsignedLong(PyTuple_GetItem(dtuple,1));
		}
		ulongMap_insert(&nfsdb_deps.depmap, PyLong_AsUnsignedLong(mpath_handle), values, 2*PyList_Size(deps), 2*PyList_Size(deps));
	}

	for (Py_ssize_t i=0; i<PyList_Size(depmap_keys); ++i) {
		PyObject* mpath = PyList_GetItem(depmap_keys,i);
		assert(PyDict_Contains(stringTable, mpath));
		PyObject* mpath_handle = PyDict_GetItem(stringTable, mpath);
		PyObject* deps = PyDict_GetItem(depmap,mpath);
		for (Py_ssize_t j=0; j<PyList_Size(deps); ++j) {
			PyObject* dtuple = PyList_GetItem(deps,j);
			unsigned long nfsdb_index = PyLong_AsUnsignedLong(PyTuple_GetItem(dtuple,0));
			unsigned long open_index = PyLong_AsUnsignedLong(PyTuple_GetItem(dtuple,1));
			struct openfile* openfile = &self->nfsdb->nfsdb[nfsdb_index].open_files[open_index];
			struct ulongMap_node* node = ulongMap_search(&nfsdb_deps.revdepmap, openfile->path);
			if (node) {
				node->value_list[node->value_count++] = PyLong_AsUnsignedLong(mpath_handle);
				if (node->value_count>=node->value_alloc) {
					/* We've reached the capacity, expand the array */
					node->value_list = realloc(node->value_list,((node->value_alloc*3)/2)*sizeof(unsigned long));
					node->value_alloc = (node->value_alloc*3)/2;
				}
			}
			else {
				unsigned long* values = malloc(16*sizeof(unsigned long));
				values[0] = PyLong_AsUnsignedLong(mpath_handle);
				ulongMap_insert(&nfsdb_deps.revdepmap, openfile->path, values, 1, 16);
			}
		}
	}

	if (show_stats) {
		printf("depmap entries: %ld\n",PyList_Size(depmap_keys));
		printf("depmap values: %ld\n",depmap_vals);
		printf("reverse depmap entries: %ld\n",ulongPairMap_count(&nfsdb_deps.revdepmap));
		printf("reverse depmap values: %ld\n",ulongPairMap_entry_count(&nfsdb_deps.revdepmap));
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
			values[2*j+0] = PyLong_AsUnsignedLong(PyTuple_GetItem(ddtuple,0));
			values[2*j+1] = PyLong_AsUnsignedLong(PyTuple_GetItem(ddtuple,1));
		}
		ulongMap_insert(&nfsdb_deps.ddepmap, PyLong_AsUnsignedLong(mpath_handle), values, 2*PyList_Size(ddeps), 2*PyList_Size(ddeps));
	}

	for (Py_ssize_t i=0; i<PyList_Size(ddepmap_keys); ++i) {
		PyObject* mpath = PyList_GetItem(ddepmap_keys,i);
		assert(PyDict_Contains(stringTable, mpath));
		PyObject* mpath_handle = PyDict_GetItem(stringTable, mpath);
		PyObject* ddeps = PyDict_GetItem(ddepmap,mpath);
		for (Py_ssize_t j=0; j<PyList_Size(ddeps); ++j) {
			PyObject* dtuple = PyList_GetItem(ddeps,j);
			unsigned long nfsdb_index = PyLong_AsUnsignedLong(PyTuple_GetItem(dtuple,0));
			unsigned long open_index = PyLong_AsUnsignedLong(PyTuple_GetItem(dtuple,1));
			struct openfile* openfile = &self->nfsdb->nfsdb[nfsdb_index].open_files[open_index];
			struct ulongMap_node* node = ulongMap_search(&nfsdb_deps.revddepmap, openfile->path);
			if (node) {
				node->value_list[node->value_count++] = PyLong_AsUnsignedLong(mpath_handle);
				if (node->value_count>=node->value_alloc) {
					/* We've reached the capacity, expand the array */
					node->value_list = realloc(node->value_list,((node->value_alloc*3)/2)*sizeof(unsigned long));
					node->value_alloc = (node->value_alloc*3)/2;
				}
			}
			else {
				unsigned long* values = malloc(16*sizeof(unsigned long));
				values[0] = PyLong_AsUnsignedLong(mpath_handle);
				ulongMap_insert(&nfsdb_deps.revddepmap, openfile->path, values, 1, 16);
			}
		}
	}

	if (show_stats) {
		printf("ddepmap entries: %ld\n",PyList_Size(ddepmap_keys));
		printf("ddepmap values: %ld\n",ddepmap_vals);
		printf("reverse rdepmap entries: %ld\n",ulongPairMap_count(&nfsdb_deps.revddepmap));
		printf("reverse rdepmap values: %ld\n",ulongPairMap_entry_count(&nfsdb_deps.revddepmap));
	}
	Py_DecRef(ddepmap_keys);

	const char* dbfn_s =  PyString_get_c_str(dbfn);
	struct uflat* uflat = uflat_init(dbfn_s);
	PYASSTR_DECREF(dbfn_s);
	if(uflat == NULL) {
		printf("uflat_init(): failed\n");
		Py_RETURN_FALSE;
	}

	int rv = uflat_set_option(uflat, UFLAT_OPT_OUTPUT_SIZE, 50ULL * 1024 * 1024 * 1024);
	if(rv) {
		printf("uflat_set_option(OUTPUT_SIZE): %d\n", rv);
		goto uflat_error_exit;
	}

	rv = uflat_set_option(uflat, UFLAT_OPT_SKIP_MEM_FRAGMENTS, 1);
	if(rv) {
		printf("uflat_set_option(SKIP_MEM_FRAGMENTS)\n");
		goto uflat_error_exit;
	}

	if(verbose_mode)
		uflat_set_option(uflat, UFLAT_OPT_VERBOSE, 1);
	if(debug_mode)
		uflat_set_option(uflat, UFLAT_OPT_DEBUG, 1);
	
	FOR_ROOT_POINTER(&nfsdb_deps,
		FLATTEN_STRUCT(nfsdb_deps,&nfsdb_deps);
	);

	int err = uflat_write(uflat);
	if (err != 0) {
		printf("flatten_write(): %d\n", err);
		goto uflat_error_exit;
	}

	uflat_fini(uflat);
	destroy_nfsdb_deps(&nfsdb_deps);

	Py_DecRef(stringTable);
	Py_RETURN_TRUE;

uflat_error_exit:
	if(uflat != NULL)
		uflat_fini(uflat);
	destroy_nfsdb_deps(&nfsdb_deps);
	Py_DecRef(stringTable);
	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_precompute_command_patterns(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

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

    if (exclude_commands && (PyList_Size(exclude_commands)>0)) {
		excl_commands = malloc(PyList_Size(exclude_commands)*sizeof(const char*));
		excl_commands_size = PyList_Size(exclude_commands);
		int u;
		for (u=0; u<PyList_Size(exclude_commands); ++u) {
			excl_commands[u] = PyString_get_c_str(PyList_GetItem(exclude_commands,u));
			DBG(debug,"        exclude command pattern: %s\n",excl_commands[u]);
		}
	}

    DBG(1,"--- Number of commands: %lu, patterns: %lu\n",self->nfsdb->nfsdb_count,excl_commands_size);
    for (size_t i=0; i<excl_commands_size; ++i) {
    	DBG(1,"--- PATTERN[%zu]:  %s\n",i,excl_commands[i]);
    }

    if (excl_commands_size<=0) {
    	// Nothing to do
    	Py_RETURN_NONE;
    }

    PyObject* pcp_map = PyDict_New();

    DBG(1,"Precomputing exclude command patterns...");

    for (unsigned long cmdi=0; cmdi<self->nfsdb->nfsdb_count; ++cmdi) {
    	struct nfsdb_entry* entry = &self->nfsdb->nfsdb[cmdi];
    	const char* cmdstr = libetrace_nfsdb_string_handle_join(self->nfsdb,entry->argv,entry->argv_count," ");
    	if (isatty(fileno(stdin))){
			if ((cmdi%1000)==0) {
				DBG(1,"\rPrecomputing exclude command patterns...%lu%%",(cmdi*100)/self->nfsdb->nfsdb_count);
			}
		}
		size_t bsize = (excl_commands_size-1)/8+1;
		unsigned char* b = calloc(bsize,1);
		assert(b!=0 && "Out of memory for allocating precompute pattern map");
		size_t k;
		for (k=0; k<excl_commands_size; ++k) {
			unsigned byte_index = k/8;
			unsigned bit_index = k%8;
			if (pattern_match_single(cmdstr,excl_commands[k])) {
				b[byte_index]|=0x01<<(7-bit_index);
			}
		}
		PyObject* pidext = PyTuple_New(2);
		PyTuple_SetItem(pidext, 0,Py_BuildValue("l",entry->eid.pid));
		PyTuple_SetItem(pidext, 1,Py_BuildValue("l",entry->eid.exeidx));
		PyObject* bytes = PyBytes_FromStringAndSize((const char*)b,bsize);
		PyDict_SetItem(pcp_map, pidext, bytes);
		Py_DecRef(pidext);
		Py_DecRef(bytes);
		free(b);
		free((void*)cmdstr);
		if (interrupt) {
			goto interrupted;
		}
    }
    DBG(1,"\n");
    for (size_t u=0; u<excl_commands_size; ++u) {
    	PYASSTR_DECREF(excl_commands[u]);
    }
	free(excl_commands);
	return pcp_map;
interrupted:
    for (size_t u=0; u<excl_commands_size; ++u) {
    	PYASSTR_DECREF(excl_commands[u]);
    }
	free(excl_commands);
	Py_DecRef(pcp_map);
	Py_RETURN_NONE;
}

PyObject * libetrace_parse_nfsdb(PyObject *self, PyObject *args) {
	int ret;
	int argc = 1;
	char *envp[] = { NULL };
	const char **argv = NULL;
	Py_ssize_t nargs = PyTuple_Size(args);
	pid_t pid;

	if (nargs >= 1)
		argc += nargs;

	argv = malloc((argc + 1) * sizeof(char*));
	argv[0] = "etrace_parser";

	for (Py_ssize_t i = 1; i <= nargs; ++i) {
		PyObject* arg = PyTuple_GetItem(args, i - 1);
		if (!PyUnicode_Check(arg)) {
			free(argv);
			Py_RETURN_NONE;
		}

		argv[i] = PyString_get_c_str(arg);
	}

	argv[argc] = NULL;

	ret = posix_spawnp(&pid, "etrace_parser", NULL, NULL, (char *const*) argv, envp);
	if (ret)
		goto err;
	waitpid(pid, &ret, 0);
	if (!WIFEXITED(ret))
		goto err;

	for (Py_ssize_t i = 1; i <= nargs; ++i)
		PYASSTR_DECREF(argv[i]);

	free(argv);

	return PyLong_FromLong(WEXITSTATUS(ret));
err:
	for (Py_ssize_t i = 1; i <= nargs; ++i)
		PYASSTR_DECREF(argv[i]);

	free(argv);
	Py_RETURN_NONE;
}

void libetrace_nfsdb_dealloc(libetrace_nfsdb_object* self) {

	PyTypeObject *tp = Py_TYPE(self);
	if (self->init_done) {
		unflatten_deinit(self->unflatten);
		unflatten_deinit(self->unflatten_deps);
	}
	Py_DecRef(self->libetrace_nfsdb_entry_openfile_filterMap);
	Py_DecRef(self->libetrace_nfsdb_entry_command_filterMap);
	tp->tp_free(self);
}

PyObject* libetrace_nfsdb_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_object* self;

	self = (libetrace_nfsdb_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		/* Use the 'load' function to initialize the cache */
		self->init_done = 0;
		self->nfsdb = 0;
		self->libetrace_nfsdb_entry_openfile_filterMap = PyDict_New();
		for (unsigned long i=0; i<LIBETRACE_NFSDB_ENTRY_OPENFILE_FILTER_FUNC_ARRAY_SIZE; ++i) {
			PyDict_SetItem(self->libetrace_nfsdb_entry_openfile_filterMap,
					PyUnicode_FromString(nfsdb_entry_openfile_filter_func_array[i].nfsdb_entry_openfile_filter_funcname),
					PyLong_FromUnsignedLong((unsigned long)nfsdb_entry_openfile_filter_func_array[i].fp));
		}
		self->libetrace_nfsdb_entry_command_filterMap = PyDict_New();
		for (unsigned long i=0; i<LIBETRACE_NFSDB_ENTRY_COMMAND_FILTER_FUNC_ARRAY_SIZE; ++i) {
			PyDict_SetItem(self->libetrace_nfsdb_entry_command_filterMap,
					PyUnicode_FromString(nfsdb_entry_command_filter_func_array[i].nfsdb_entry_command_filter_funcname),
					PyLong_FromUnsignedLong((unsigned long)nfsdb_entry_command_filter_func_array[i].fp));
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

static void libetrace_nfsdb_entry_list_append(libetrace_nfsdb_object* self,
		struct nfsdb_entryMap_node* node, PyObject* entryList) {

	for (unsigned long i=0; i<node->entry_count; ++i) {
		struct nfsdb_entry* entry = node->entry_list[i];
		PyObject* item = libetrace_nfsdb_sq_item((PyObject*)self,entry->nfsdb_index);
		PYLIST_ADD_PYOBJECT(entryList,item);
	}
}

static int libetrace_nfsdb_entry_from_tuple(libetrace_nfsdb_object* self, PyObject* entryList, PyObject* slice_tuple) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	ASSERT_WITH_NFSDB_ERROR(PyTuple_Size(slice_tuple)>=1,"Invalid subscript argument (empty tuple)");
	PyObject* pypid = PyTuple_GetItem(slice_tuple,0);
	ASSERT_WITH_NFSDB_ERROR(PyLong_Check(pypid),"Invalid subscript argument (not (long))");
	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,PyLong_AsLong(pypid));
	ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Invalid pid key [%ld] at nfsdb entry",PyLong_AsLong(pypid));
	if (PyTuple_Size(slice_tuple)<2) {
		/* Return the list of all executions for a given pid */
		libetrace_nfsdb_entry_list_append(self,node,entryList);
	}
	else {
		/* Return specific execution given pid:exeidx value */
		PyObject* pyexeidx = PyTuple_GetItem(slice_tuple,1);
		ASSERT_WITH_NFSDB_ERROR(PyLong_Check(pyexeidx),"Invalid subscript argument (not (long))");
		ASSERT_WITH_NFSDB_FORMAT_ERROR(PyLong_AsLong(pyexeidx)<node->entry_count,
				"nfsdb entry index [%ld] out of range",PyLong_AsLong(pyexeidx));
		struct nfsdb_entry* entry = node->entry_list[PyLong_AsLong(pyexeidx)];
		PYLIST_ADD_PYOBJECT(entryList,libetrace_nfsdb_sq_item((PyObject*)self,entry->nfsdb_index));
	}

	return 1;
}

static int libetrace_nfsdb_entry_from_string(libetrace_nfsdb_object* self, PyObject* entryList, PyObject* slice_string) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	const char* bpath = PyString_get_c_str(slice_string);
	struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, bpath);
	if (!srefnode) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid binary path key [%s] at nfsdb entry",bpath);
		PyErr_SetString(libetrace_nfsdbError, errmsg);
		PYASSTR_DECREF(bpath);
		return 0;
	}
	PYASSTR_DECREF(bpath);
	unsigned long hbpath = srefnode->value;
	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->bmap,hbpath);
	ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at binary path handle [%lu]",hbpath);
	libetrace_nfsdb_entry_list_append(self,node,entryList);
	return 1;
}

static int libetrace_nfsdb_entry_from_Eid(libetrace_nfsdb_object* self, PyObject* entryList,
		libetrace_nfsdb_entry_eid_object* eid) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,eid->pid);
	ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Invalid pid key [%ld] at nfsdb entry",eid->pid);
	if (eid->exeidx==ULONG_MAX) {
		/* Return the list of all executions for a given pid */
		libetrace_nfsdb_entry_list_append(self,node,entryList);
	}
	else {
		/* Return specific execution given pid:exeidx value */
		ASSERT_WITH_NFSDB_FORMAT_ERROR(eid->exeidx<node->entry_count,
				"nfsdb entry index [%ld] out of range",eid->exeidx);
		struct nfsdb_entry* entry = node->entry_list[eid->exeidx];
		PYLIST_ADD_PYOBJECT(entryList,libetrace_nfsdb_sq_item((PyObject*)self,entry->nfsdb_index));
	}

	return 1;
}

static int libetrace_nfsdb_entry_from_slice(libetrace_nfsdb_object* self, PyObject* entryList, PyObject* real_slice) {

	Py_ssize_t start,end,step;
	if (PySlice_Unpack(real_slice,&start,&end,&step)) {
		PyErr_SetString(libetrace_nfsdbError, "Cannot decode slice argument");
		return 0;
	}

	for (Py_ssize_t i=start; i<((end<self->nfsdb->nfsdb_count)?(end):(self->nfsdb->nfsdb_count)); i+=step) {
		PyObject* entry = libetrace_nfsdb_sq_item((PyObject*)self,i);
		PYLIST_ADD_PYOBJECT(entryList,entry);
	}

	return 1;
}

PyObject* libetrace_nfsdb_mp_subscript(PyObject* self, PyObject* slice) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	PyObject* rL = PyList_New(0);

	if (PyLong_Check(slice)) {
		return libetrace_nfsdb_sq_item(self,PyLong_AsLong(slice));
	}
	else if (PyTuple_Check(slice)) {
		if (libetrace_nfsdb_entry_from_tuple(__self,rL,slice)) {
			return rL;
		}
	}
	else if (PyUnicode_Check(slice)) {
		if (libetrace_nfsdb_entry_from_string(__self,rL,slice)) {
			return rL;
		}
	}
	else if (PyObject_IsInstance(slice, (PyObject *)&libetrace_nfsdbEntryEidType)) {
		if (libetrace_nfsdb_entry_from_Eid(__self,rL,(libetrace_nfsdb_entry_eid_object*)slice)) {
			return rL;
		}
	}
	else if (PyList_Check(slice)) {
		Py_ssize_t i;
		for (i=0; i<PyList_Size(slice); ++i) {
			PyObject* item = PyList_GetItem(slice,i);
			if (PyLong_Check(item)) {
				PyObject* entry = libetrace_nfsdb_sq_item(self,PyLong_AsLong(item));
				PYLIST_ADD_PYOBJECT(rL,entry);
			}
			else if (PyTuple_Check(item)) {
				if (!libetrace_nfsdb_entry_from_tuple(__self,rL,item)) {
					break;
				}
			}
			else if (PyUnicode_Check(item)) {
				if (!libetrace_nfsdb_entry_from_string(__self,rL,item)) {
					break;
				}
			}
			else if (PyObject_IsInstance(item, (PyObject *)&libetrace_nfsdbEntryEidType)) {
				if (!libetrace_nfsdb_entry_from_Eid(__self,rL,(libetrace_nfsdb_entry_eid_object*)item)) {
					break;
				}
			}
			else {
				PyErr_SetString(libetrace_nfsdbError, "Invalid subscript argument");
				break;
			}
		}
		if (i>=PyList_Size(slice)) return rL; /* The loop finished normally */
	}
	else if (!PySlice_Check(slice)) {
		PyErr_SetString(libetrace_nfsdbError, "Invalid subscript argument");
	}
	else {
		/* Now we have a real slice with numbers */
		if (libetrace_nfsdb_entry_from_slice(__self,rL,slice)) {
			return rL;
		}
	}

	Py_DecRef(rL);
	return 0;
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

	if (libetrace_nfsdb_entry_has_compilations_internal(entry)) {
		return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_filter_has_linked_file(const struct nfsdb_entry* entry) {

	if (libetrace_nfsdb_entry_is_linking_internal(entry)) {
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

PyObject* libetrace_nfsdb_opens_list(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* opens = PyList_New(0);
	for (unsigned long i=0; i<self->nfsdb->nfsdb_count; ++i) {
		struct nfsdb_entry* nfsdb_entry = &self->nfsdb->nfsdb[i];
		for (unsigned long j=0; j<nfsdb_entry->open_files_count; ++j) {
			libetrace_nfsdb_entry_openfile_object* py_openfile = libetrace_nfsdb_create_openfile_entry(
						self->nfsdb,nfsdb_entry,j,nfsdb_entry->nfsdb_index);
			PyList_Append(opens,(PyObject*)py_openfile);
			Py_DecRef((PyObject*)py_openfile);
		}
	}

	return opens;
}

PyObject* libetrace_nfsdb_opens_paths(libetrace_nfsdb_object *self, PyObject *args) {

    PyObject* paths = PyList_New(0);
	struct rb_node * p = rb_first(&self->nfsdb->filemap);
    while(p) {
        struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
        if (data->access_type!=FILE_ACCESS_TYPE_EXEC) {
	        PyObject* s = PyUnicode_FromString(self->nfsdb->string_table[data->key]);
	        PyList_Append(paths,s);
	        Py_DecRef(s);
	    }
        p = rb_next(p);
    }
    return paths;
}

static void destroy_file_filters(struct file_filter* fflt, size_t andsz) {

	for (size_t j=0; j<andsz; ++j) {
		if (fflt[j].filter_set[0].func) {
			if (fflt[j].filter_set[0].func==libetrace_nfsdb_entry_openfile_filter_matches_re) {
				Py_DecRef((PyObject*)fflt[j].filter_set[0].arg);
			}
			else {
				PYASSTR_DECREF(fflt[j].filter_set[0].arg);
			}
		}
		if (fflt[j].filter_set[6].func) {
			struct rb_node * p = rb_first(fflt[j].filter_set[6].arg);
		    while(p) {
		        struct stringRefMap_node* data = (struct stringRefMap_node*)p;
		        PYASSTR_DECREF(data->key);
		        p = rb_next(p);
		    }
			stringRefMap_destroy((struct rb_root*)fflt[j].filter_set[6].arg);
			free((void*)fflt[j].filter_set[6].arg);
		}
	}
}

static void destroy_opens_paths_filters(Py_ssize_t filter_count, struct file_filter** cflts, size_t* cflts_size) {

	for (Py_ssize_t i=0; i<filter_count; ++i) {
		struct file_filter* andflts = cflts[i];
		size_t andsz = cflts_size[i];
		destroy_file_filters(andflts,andsz);
	}
}

static void destroy_single_command_filters(struct command_filter* fflt, size_t andsz) {

	for (size_t j=0; j<andsz; ++j) {
		if (fflt[j].filter_set[0].func) {
			if ((fflt[j].filter_set[0].func==libetrace_nfsdb_entry_command_filter_cwd_matches_re)||
						(fflt[j].filter_set[0].func==libetrace_nfsdb_entry_command_filter_cmd_matches_re)||
						(fflt[j].filter_set[0].func==libetrace_nfsdb_entry_command_filter_bin_matches_re)) {
				Py_DecRef((PyObject*)fflt[j].filter_set[0].arg);
			}
			else {
				PYASSTR_DECREF(fflt[j].filter_set[0].arg);
			}
		}
		if (fflt[j].filter_set[4].func) {
			struct rb_node * p = rb_first(fflt[j].filter_set[4].arg);
		    while(p) {
		        struct stringRefMap_node* data = (struct stringRefMap_node*)p;
		        PYASSTR_DECREF(data->key);
		        p = rb_next(p);
		    }
			stringRefMap_destroy((struct rb_root*)fflt[j].filter_set[4].arg);
			free((void*)fflt[j].filter_set[4].arg);
		}

		if (fflt[j].filter_set[5].func) {
			ulong_entryMap_destroy((struct rb_root*)fflt[j].filter_set[5].arg);
			free((void*)fflt[j].filter_set[5].arg);
		}
	}
}

static void destroy_command_filters(Py_ssize_t filter_count, struct command_filter** cflts, size_t* cflts_size) {

	for (Py_ssize_t i=0; i<filter_count; ++i) {
		struct command_filter* andflts = cflts[i];
		size_t andsz = cflts_size[i];
		destroy_single_command_filters(andflts,andsz);
	}
}

static void free_string_list(struct rb_root* string_list) {

	struct rb_node * p = rb_first(string_list);
	while(p) {
		struct stringRefMap_node* data = (struct stringRefMap_node*)p;
		PYASSTR_DECREF(data->key);
		p = rb_next(p);
	}
	stringRefMap_destroy(string_list);
}

static int parse_opens_fast_filter(libetrace_nfsdb_object *self, PyObject* kwargs, struct file_filter* fast_filter, int pure_path) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	int have_fast_filter = 0;

	if (!kwargs) {
		return 0;
	}

	if (KWARGS_HAVE(kwargs,"path") && KWARGS_GET(kwargs,"path") != Py_None) {
		PyObject* py_path_list = KWARGS_GET(kwargs,"path");
		ASSERT_WITH_NFSDB_ERROR(PyList_Check(py_path_list),"Invalid type of 'path' argument (expected 'list')");
		struct rb_root* path_list = calloc(sizeof(struct rb_root),1);
		for (Py_ssize_t i=0; i<PyList_Size(py_path_list); ++i) {
			PyObject* py_path = PyList_GetItem(py_path_list,i);
			if (!PyUnicode_Check(py_path)) {
				free_string_list(path_list);
				free(path_list);
				PyErr_SetString(libetrace_nfsdbError, "Invalid type of 'path' element argument (expected 'str')");
				return 0;
			}
			const char* path = PyString_get_c_str(py_path);
			stringRefMap_insert(path_list, path, 0);
		}
		fast_filter->filter_set[6].func = libetrace_nfsdb_entry_openfile_filter_in_path_list;
		fast_filter->filter_set[6].arg = path_list;
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"has_path") && KWARGS_GET(kwargs,"has_path") != Py_None) {
		PyObject* py_path = KWARGS_GET(kwargs,"has_path");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(py_path),"Invalid type of 'has_path' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_openfile_filter_contains_path;
		fast_filter->filter_set[0].arg = PyString_get_c_str(py_path);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"wc") && KWARGS_GET(kwargs,"wc") != Py_None) {
		PyObject* pattern = KWARGS_GET(kwargs,"wc");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(pattern),"Invalid type of 'wc' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_openfile_filter_matches_wc;
		fast_filter->filter_set[0].arg = PyString_get_c_str(pattern);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"re") && KWARGS_GET(kwargs,"re") != Py_None) {
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_openfile_filter_matches_re;
		PyObject* pattern = KWARGS_GET(kwargs,"re");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(pattern),"Invalid type of 're' argument (expected 'str')");
		PyObject* osModuleString = PyUnicode_FromString((char*)"re");
		PyObject* osModule = PyImport_Import(osModuleString);
		PyObject* compileFunction = PyObject_GetAttrString(osModule,(char*)"compile");
		PyObject* args = PyTuple_New(1);
		PyTuple_SetItem(args,0,pattern);
		Py_XINCREF(pattern);
		PyObject* reObject = PyObject_CallObject(compileFunction, args);
		Py_DecRef(args);
		fast_filter->filter_set[0].arg = reObject;
		Py_DECREF(compileFunction);
		Py_DECREF(osModule);
		Py_DECREF(osModuleString);
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"compiled") && KWARGS_GET(kwargs,"compiled") != Py_None) {
		PyObject* is_compiled = KWARGS_GET(kwargs,"compiled");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_compiled),"Invalid type of 'compiled' argument (expected 'bool')");
		if (PyObject_IsTrue(is_compiled)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_COMPILED;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"linked") && KWARGS_GET(kwargs,"linked") != Py_None) {
		PyObject* is_linked = KWARGS_GET(kwargs,"linked");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_linked),"Invalid type of 'linked' argument (expected 'bool')");
		if (PyObject_IsTrue(is_linked)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_LINKED;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"linked_static") && KWARGS_GET(kwargs,"linked_static") != Py_None) {
		PyObject* is_linked_static = KWARGS_GET(kwargs,"linked_static");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_linked_static),"Invalid type of 'linked_static' argument (expected 'bool')");
		if (PyObject_IsTrue(is_linked_static)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_LINKED_STATIC;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"linked_shared") && KWARGS_GET(kwargs,"linked_shared") != Py_None) {
		PyObject* is_linked_shared = KWARGS_GET(kwargs,"linked_shared");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_linked_shared),"Invalid type of 'linked_shared' argument (expected 'bool')");
		if (PyObject_IsTrue(is_linked_shared)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_LINKED_SHARED;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"linked_exe") && KWARGS_GET(kwargs,"linked_exe") != Py_None) {
		PyObject* is_linked_exe = KWARGS_GET(kwargs,"linked_exe");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_linked_exe),"Invalid type of 'linked_exe' argument (expected 'bool')");
		if (PyObject_IsTrue(is_linked_exe)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_LINKED_EXE;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"plain") && KWARGS_GET(kwargs,"plain") != Py_None) {
		PyObject* is_plain = KWARGS_GET(kwargs,"plain");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_plain),"Invalid type of 'plain' argument (expected 'bool')");
		if (PyObject_IsTrue(is_plain)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_PLAIN;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"compiler") && KWARGS_GET(kwargs,"compiler") != Py_None) {
		PyObject* is_compiler = KWARGS_GET(kwargs,"compiler");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_compiler),"Invalid type of 'compiler' argument (expected 'bool')");
		if (PyObject_IsTrue(is_compiler)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_COMPILER;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"linker") && KWARGS_GET(kwargs,"linker") != Py_None) {
		PyObject* is_linker = KWARGS_GET(kwargs,"linker");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_linker),"Invalid type of 'linker' argument (expected 'bool')");
		if (PyObject_IsTrue(is_linker)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_LINKER;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"binary") && KWARGS_GET(kwargs,"binary") != Py_None) {
		PyObject* is_binary = KWARGS_GET(kwargs,"binary");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_binary),"Invalid type of 'binary' argument (expected 'bool')");
		if (PyObject_IsTrue(is_binary)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_BINARY;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"symlink") && KWARGS_GET(kwargs,"symlink") != Py_None) {
		PyObject* is_symlink = KWARGS_GET(kwargs,"symlink");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_symlink),"Invalid type of 'symlink' argument (expected 'bool')");
		if (PyObject_IsTrue(is_symlink)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_SYMLINK;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"no_symlink") && KWARGS_GET(kwargs,"no_symlink") != Py_None) {
		PyObject* is_no_symlink = KWARGS_GET(kwargs,"no_symlink");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_no_symlink),"Invalid type of 'no_symlink' argument (expected 'bool')");
		if (PyObject_IsTrue(is_no_symlink)) {
			fast_filter->filter_set[1].func = libetrace_nfsdb_entry_openfile_filter_is_class;
			fast_filter->filter_set[1].arg = (const void*)FILE_CLASS_NOSYMLINK;
			have_fast_filter=1;
		}
	}

	if (KWARGS_HAVE(kwargs,"file_exists") && KWARGS_GET(kwargs,"file_exists") != Py_None) {
		PyObject* is_file_exists = KWARGS_GET(kwargs,"file_exists");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_file_exists),"Invalid type of 'file_exists' argument (expected 'bool')");
		if (PyObject_IsTrue(is_file_exists)) {
			fast_filter->filter_set[2].func = libetrace_nfsdb_entry_openfile_filter_file_exists;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"file_not_exists") && KWARGS_GET(kwargs,"file_not_exists") != Py_None) {
		PyObject* is_file_not_exists = KWARGS_GET(kwargs,"file_not_exists");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_file_not_exists),"Invalid type of 'file_not_exists' argument (expected 'bool')");
		if (PyObject_IsTrue(is_file_not_exists)) {
			fast_filter->filter_set[2].func = libetrace_nfsdb_entry_openfile_filter_file_not_exists;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"dir_exists") && KWARGS_GET(kwargs,"dir_exists") != Py_None) {
		PyObject* is_dir_exists = KWARGS_GET(kwargs,"dir_exists");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_dir_exists),"Invalid type of 'dir_exists' argument (expected 'bool')");
		if (PyObject_IsTrue(is_dir_exists)) {
			fast_filter->filter_set[2].func = libetrace_nfsdb_entry_openfile_filter_dir_exists;
			have_fast_filter=1;
		}
	}

	if (KWARGS_HAVE(kwargs,"has_access") && KWARGS_GET(kwargs,"has_access") != Py_None) {
		PyObject* py_access_arg = KWARGS_GET(kwargs,"has_access");
		ASSERT_WITH_NFSDB_ERROR(PyLong_Check(py_access_arg),"Invalid type of 'has_access' argument (expected 'int')");
		unsigned long access_arg = PyLong_AsUnsignedLong(py_access_arg);
		if (pure_path) {
			ASSERT_WITH_NFSDB_FORMAT_ERROR(((access_arg>=GLOBAL_ACCESS_READ) && (access_arg<=GLOBAL_ACCESS_RW)),
				"Invalid 'access' argument value %lu (expected value between %d [GLOBAL_ACCESS_READ] and %d [GLOBAL_ACCESS_RW])",
				access_arg,GLOBAL_ACCESS_READ,GLOBAL_ACCESS_RW);
		}
		else {
			ASSERT_WITH_NFSDB_FORMAT_ERROR(((access_arg>=ACCESS_READ) && (access_arg<=GLOBAL_ACCESS_RW)),
				"Invalid access value %lu (expected value between %d [ACCESS_READ] and %d [GLOBAL_ACCESS_RW])",
				access_arg,ACCESS_READ,GLOBAL_ACCESS_RW);
		}
		fast_filter->filter_set[3].func = libetrace_nfsdb_entry_openfile_filter_has_access;
		fast_filter->filter_set[3].arg = (const void*)access_arg;
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"negate") && KWARGS_GET(kwargs,"negate") != Py_None) {
		PyObject* is_negate = KWARGS_GET(kwargs,"negate");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_negate),"Invalid type of 'negate' argument (expected 'bool')");
		fast_filter->negate = PyObject_IsTrue(is_negate);
	}

	if (KWARGS_HAVE(kwargs,"at_source_root") && KWARGS_GET(kwargs,"at_source_root") != Py_None) {
		PyObject* is_at_source_root = KWARGS_GET(kwargs,"at_source_root");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_at_source_root),"Invalid type of 'at_source_root' argument (expected 'bool')");
		if (PyObject_IsTrue(is_at_source_root)) {
			fast_filter->filter_set[4].func = libetrace_nfsdb_entry_openfile_filter_at_source_root;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"not_at_source_root") && KWARGS_GET(kwargs,"not_at_source_root") != Py_None) {
		PyObject* is_not_at_source_root = KWARGS_GET(kwargs,"not_at_source_root");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(is_not_at_source_root),"Invalid type of 'not_at_source_root' argument (expected 'bool')");
		if (PyObject_IsTrue(is_not_at_source_root)) {
			fast_filter->filter_set[4].func = libetrace_nfsdb_entry_openfile_filter_not_at_source_root;
			have_fast_filter=1;
		}
	}

	if (KWARGS_HAVE(kwargs,"source_type") && KWARGS_GET(kwargs,"source_type") != Py_None) {
		PyObject* py_srctype_arg = KWARGS_GET(kwargs,"source_type");
		ASSERT_WITH_NFSDB_ERROR(PyLong_Check(py_srctype_arg),"Invalid type of 'source_type' argument (expected 'int')");
		unsigned long srctype = PyLong_AsUnsignedLong(py_srctype_arg);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(((srctype==FILE_SOURCE_TYPE_C) || (srctype<=FILE_SOURCE_TYPE_CPP) || (srctype<=FILE_SOURCE_TYPE_OTHER)),
				"Invalid 'source_type' argument value %lu (expected %d [FILE_SOURCE_TYPE_C], %d [FILE_SOURCE_TYPE_CPP] or %d [FILE_SOURCE_TYPE_OTHER])",
				srctype,FILE_SOURCE_TYPE_C,FILE_SOURCE_TYPE_CPP,FILE_SOURCE_TYPE_OTHER);
		fast_filter->filter_set[5].func = libetrace_nfsdb_entry_openfile_filter_source_type;
		fast_filter->filter_set[5].arg = (const void*)srctype;
		have_fast_filter=1;
	}

	return have_fast_filter;
}

static int parse_opens_filters(libetrace_nfsdb_object *self, PyObject* filters, struct file_filter** cflts, size_t* cflts_size, int pure_path) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	for (Py_ssize_t i=0; i<PyList_Size(filters); ++i) {
		/* [(PATH,CLASS,EXISTS,ACCESS,NEGATE,SRCROOT,SRCTYPE),(...),...] */
		PyObject* andSection = PyList_GetItem(filters,i);
		struct file_filter* andflts = calloc(sizeof(struct file_filter),PyList_Size(andSection));
		cflts[i] = andflts;
		cflts_size[i] = PyList_Size(andSection);
		for (Py_ssize_t j=0; j<PyList_Size(andSection); ++j) {
			/* (PATH,CLASS,EXISTS,ACCESS,NEGATE,SRCROOT,SRCTYPE) */
			struct file_filter* fflt = &andflts[j];
			PyObject* filter = PyList_GetItem(andSection,j);
			PyObject* path_filter = PyTuple_GetItem(filter,0);
			if ((path_filter != Py_None)&&(PyTuple_Size(path_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(path_filter,0);
				PyObject* py_path_arg = PyTuple_GetItem(path_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyUnicode_Check(py_path_arg),"Invalid type of filter 'path' argument (expected 'str')");
				CHECK_ASSIGN_FILE_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[0].func = __func;
					if (__func==libetrace_nfsdb_entry_openfile_filter_matches_re) {
						PyObject* osModuleString = PyUnicode_FromString((char*)"re");
						PyObject* osModule = PyImport_Import(osModuleString);
						PyObject* compileFunction = PyObject_GetAttrString(osModule,(char*)"compile");
						PyObject* args = PyTuple_New(1);
						PyTuple_SetItem(args,0,py_path_arg);
						Py_XINCREF(py_path_arg);
						PyObject* reObject = PyObject_CallObject(compileFunction, args);
						Py_DecRef(args);
						fflt->filter_set[0].arg = reObject;
						Py_DECREF(compileFunction);
						Py_DECREF(osModule);
						Py_DECREF(osModuleString);
					}
					else {
						fflt->filter_set[0].arg = PyString_get_c_str(py_path_arg);
					}
				);
			}
			if (PyTuple_Size(filter)<2) continue;
			PyObject* class_filter = PyTuple_GetItem(filter,1);
			if ((class_filter != Py_None)&&(PyTuple_Size(class_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(class_filter,0);
				PyObject* py_class_arg = PyTuple_GetItem(class_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyLong_Check(py_class_arg),"Invalid type of filter 'class' argument (expected 'int')");
				unsigned long class_arg = PyLong_AsUnsignedLong(py_class_arg);
				ASSERT_CLEANUP_WITH_NFSDB_FORMAT_ERROR((class_arg&0xf800)==0,
				"Invalid 'class' argument bits in %lu (expected maximum bit value 0x%x)",
				class_arg,FILE_CLASS_NOSYMLINK);
				CHECK_ASSIGN_FILE_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[1].func = __func;
					fflt->filter_set[1].arg = (const void*)class_arg;
				);
			}
			if (PyTuple_Size(filter)<3) continue;
			PyObject* exists_filter = PyTuple_GetItem(filter,2);
			if ((exists_filter != Py_None)&&(PyTuple_Size(exists_filter)>=1)) {
				PyObject* fpkey = PyTuple_GetItem(exists_filter,0);
				CHECK_ASSIGN_FILE_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[2].func = __func;
				);
			}
			if (PyTuple_Size(filter)<4) continue;
			PyObject* access_filter = PyTuple_GetItem(filter,3);
			if ((access_filter != Py_None)&&(PyTuple_Size(access_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(access_filter,0);
				PyObject* py_access_arg = PyTuple_GetItem(access_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyLong_Check(py_access_arg),"Invalid type of filter 'access' argument (expected 'int')");
				unsigned long access_arg = PyLong_AsUnsignedLong(py_access_arg);
				if (pure_path) {
					ASSERT_CLEANUP_WITH_NFSDB_FORMAT_ERROR(((access_arg>=GLOBAL_ACCESS_READ) && (access_arg<=GLOBAL_ACCESS_RW)),
						"Invalid 'access' argument value %lu (expected value between %d [GLOBAL_ACCESS_READ] and %d [GLOBAL_ACCESS_RW])",
						access_arg,GLOBAL_ACCESS_READ,GLOBAL_ACCESS_RW);
				}
				else {
					ASSERT_CLEANUP_WITH_NFSDB_FORMAT_ERROR(((access_arg>=ACCESS_READ) && (access_arg<=GLOBAL_ACCESS_RW)),
						"Invalid access value %lu (expected value between %d [ACCESS_READ] and %d [GLOBAL_ACCESS_RW])",
						access_arg,ACCESS_READ,GLOBAL_ACCESS_RW);
				}
				CHECK_ASSIGN_FILE_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[3].func = __func;
					fflt->filter_set[3].arg = (const void*)access_arg;
				);
			}
			if (PyTuple_Size(filter)<5) continue;
			PyObject* negate_filter = PyTuple_GetItem(filter,4);
			if (negate_filter != Py_None) {
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyBool_Check(negate_filter),"Invalid type of filter 'negate' argument (expected 'bool')");
				fflt->negate = PyObject_IsTrue(negate_filter);
			}
			if (PyTuple_Size(filter)<6) continue;
			PyObject* srcroot_filter = PyTuple_GetItem(filter,5);
			if ((srcroot_filter != Py_None)&&(PyTuple_Size(srcroot_filter)>=1)) {
				PyObject* fpkey = PyTuple_GetItem(srcroot_filter,0);
				CHECK_ASSIGN_FILE_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[4].func = __func;
				);
			}
			if (PyTuple_Size(filter)<7) continue;
			PyObject* srctype_filter = PyTuple_GetItem(filter,6);
			if ((srctype_filter != Py_None)&&(PyTuple_Size(srctype_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(srctype_filter,0);
				PyObject* py_srctype_arg = PyTuple_GetItem(srctype_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyLong_Check(py_srctype_arg),"Invalid type of filter 'srctype' argument (expected 'int')");
				unsigned long srctype = PyLong_AsUnsignedLong(py_srctype_arg);
				ASSERT_CLEANUP_WITH_NFSDB_FORMAT_ERROR(((srctype==FILE_SOURCE_TYPE_C) || (srctype<=FILE_SOURCE_TYPE_CPP) || (srctype<=FILE_SOURCE_TYPE_OTHER)),
					"Invalid 'source_type' argument value %lu (expected %d [FILE_SOURCE_TYPE_C], %d [FILE_SOURCE_TYPE_CPP] or %d [FILE_SOURCE_TYPE_OTHER])",
					srctype,FILE_SOURCE_TYPE_C,FILE_SOURCE_TYPE_CPP,FILE_SOURCE_TYPE_OTHER);
				CHECK_ASSIGN_FILE_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[5].func = __func;
					fflt->filter_set[5].arg = (const void*)srctype;
				);
			}
		}
	}

	return 1;

cleanup_and_err:
	destroy_opens_paths_filters(PyList_Size(filters),cflts,cflts_size);
	return 0;
}

static int parse_commands_fast_filter(libetrace_nfsdb_object *self, PyObject* kwargs, struct command_filter* fast_filter) {

	int have_fast_filter = 0;

	if (!kwargs) {
		return 0;
	}

	if (KWARGS_HAVE(kwargs,"bins") && KWARGS_GET(kwargs,"bins") != Py_None) {
		PyObject* py_bins_list = KWARGS_GET(kwargs,"bins");
		ASSERT_WITH_NFSDB_ERROR(PyList_Check(py_bins_list),"Invalid type of 'bins' argument (expected 'list')");
		struct rb_root* bins_list = calloc(sizeof(struct rb_root),1);
		for (Py_ssize_t i=0; i<PyList_Size(py_bins_list); ++i) {
			PyObject* py_bpath = PyList_GetItem(py_bins_list,i);
			if (!PyUnicode_Check(py_bpath)) {
				free_string_list(bins_list);
				free(bins_list);
				PyErr_SetString(libetrace_nfsdbError, "Invalid type of 'bins' element argument (expected 'str')");
				return 0;
			}
			const char* bpath = PyString_get_c_str(py_bpath);
			stringRefMap_insert(bins_list, bpath, 0);
		}
		fast_filter->filter_set[4].func = libetrace_nfsdb_entry_command_filter_in_binaries_list;
		fast_filter->filter_set[4].arg = bins_list;
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"pids") && KWARGS_GET(kwargs,"pids") != Py_None) {
		PyObject* py_pids_list = KWARGS_GET(kwargs,"pids");
		ASSERT_WITH_NFSDB_ERROR(PyList_Check(py_pids_list),"Invalid type of 'pids' argument (expected 'list')");
		struct rb_root* pids_list = calloc(sizeof(struct rb_root),1);
		for (Py_ssize_t i=0; i<PyList_Size(py_pids_list); ++i) {
			PyObject* py_pid = PyList_GetItem(py_pids_list,i);
			if (!PyLong_Check(py_pid)) {
				ulong_entryMap_destroy(pids_list);
				free(pids_list);
				PyErr_SetString(libetrace_nfsdbError, "Invalid type of 'pids' element argument (expected 'int')");
				return 0;
			}
			unsigned long pid = PyLong_AsUnsignedLong(py_pid);
			ulong_entryMap_insert(pids_list, pid, 0);
		}
		fast_filter->filter_set[5].func = libetrace_nfsdb_entry_command_filter_in_pids_list;
		fast_filter->filter_set[5].arg = pids_list;
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"cwd_has_str") && KWARGS_GET(kwargs,"cwd_has_str") != Py_None) {
		PyObject* path = KWARGS_GET(kwargs,"cwd_has_str");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(path),"Invalid type of 'cwd_has_str' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_cwd_contains_path;
		fast_filter->filter_set[0].arg = PyString_get_c_str(path);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"cwd_wc") && KWARGS_GET(kwargs,"cwd_wc") != Py_None) {
		PyObject* wc = KWARGS_GET(kwargs,"cwd_wc");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(wc),"Invalid type of 'cwd_wc' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_cwd_matches_wc;
		fast_filter->filter_set[0].arg = PyString_get_c_str(wc);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"cwd_re") && KWARGS_GET(kwargs,"cwd_re") != Py_None) {
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_cwd_matches_re;
		PyObject* pattern = KWARGS_GET(kwargs,"cwd_re");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(pattern),"Invalid type of 'cwd_re' argument (expected 'str')");
		PyObject* osModuleString = PyUnicode_FromString((char*)"re");
		PyObject* osModule = PyImport_Import(osModuleString);
		PyObject* compileFunction = PyObject_GetAttrString(osModule,(char*)"compile");
		PyObject* args = PyTuple_New(1);
		PyTuple_SetItem(args,0,pattern);
		Py_XINCREF(pattern);
		PyObject* reObject = PyObject_CallObject(compileFunction, args);
		Py_DecRef(args);
		fast_filter->filter_set[0].arg = reObject;
		Py_DECREF(compileFunction);
		Py_DECREF(osModule);
		Py_DECREF(osModuleString);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"cmd_has_str") && KWARGS_GET(kwargs,"cmd_has_str") != Py_None) {
		PyObject* path = KWARGS_GET(kwargs,"cmd_has_str");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(path),"Invalid type of 'cmd_has_str' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_cmd_has_string;
		fast_filter->filter_set[0].arg = PyString_get_c_str(path);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"cmd_wc") && KWARGS_GET(kwargs,"cmd_wc") != Py_None) {
		PyObject* wc = KWARGS_GET(kwargs,"cmd_wc");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(wc),"Invalid type of 'cmd_wc' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_cmd_matches_wc;
		fast_filter->filter_set[0].arg = PyString_get_c_str(wc);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"cmd_re") && KWARGS_GET(kwargs,"cmd_re") != Py_None) {
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_cmd_matches_re;
		PyObject* pattern = KWARGS_GET(kwargs,"cmd_re");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(pattern),"Invalid type of 'cmd_re' argument (expected 'str')");
		PyObject* osModuleString = PyUnicode_FromString((char*)"re");
		PyObject* osModule = PyImport_Import(osModuleString);
		PyObject* compileFunction = PyObject_GetAttrString(osModule,(char*)"compile");
		PyObject* args = PyTuple_New(1);
		PyTuple_SetItem(args,0,pattern);
		Py_XINCREF(pattern);
		PyObject* reObject = PyObject_CallObject(compileFunction, args);
		Py_DecRef(args);
		fast_filter->filter_set[0].arg = reObject;
		Py_DECREF(compileFunction);
		Py_DECREF(osModule);
		Py_DECREF(osModuleString);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"bin_has_str") && KWARGS_GET(kwargs,"bin_has_str") != Py_None) {
		PyObject* path = KWARGS_GET(kwargs,"bin_has_str");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(path),"Invalid type of 'bin_has_str' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_bin_contains_path;
		fast_filter->filter_set[0].arg = PyString_get_c_str(path);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"bin_wc") && KWARGS_GET(kwargs,"bin_wc") != Py_None) {
		PyObject* wc = KWARGS_GET(kwargs,"bin_wc");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(wc),"Invalid type of 'bin_wc' argument (expected 'str')");
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_bin_matches_wc;
		fast_filter->filter_set[0].arg = PyString_get_c_str(wc);
		have_fast_filter=1;
	}
	else if (KWARGS_HAVE(kwargs,"bin_re") && KWARGS_GET(kwargs,"bin_re") != Py_None) {
		fast_filter->filter_set[0].func = libetrace_nfsdb_entry_command_filter_bin_matches_re;
		PyObject* pattern = KWARGS_GET(kwargs,"bin_re");
		ASSERT_WITH_NFSDB_ERROR(PyUnicode_Check(pattern),"Invalid type of 'bin_re' argument (expected 'str')");
		PyObject* osModuleString = PyUnicode_FromString((char*)"re");
		PyObject* osModule = PyImport_Import(osModuleString);
		PyObject* compileFunction = PyObject_GetAttrString(osModule,(char*)"compile");
		PyObject* args = PyTuple_New(1);
		PyTuple_SetItem(args,0,pattern);
		Py_XINCREF(pattern);
		PyObject* reObject = PyObject_CallObject(compileFunction, args);
		Py_DecRef(args);
		fast_filter->filter_set[0].arg = reObject;
		Py_DECREF(compileFunction);
		Py_DECREF(osModule);
		Py_DECREF(osModuleString);
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"has_ppid") && KWARGS_GET(kwargs,"has_ppid") != Py_None) {
		PyObject* py_ppid_arg = KWARGS_GET(kwargs,"has_ppid");
		ASSERT_WITH_NFSDB_ERROR(PyLong_Check(py_ppid_arg),"Invalid type of 'has_ppid' argument (expected 'int')");
		fast_filter->filter_set[1].func = libetrace_nfsdb_entry_command_filter_has_ppid;
		fast_filter->filter_set[1].arg = (const void*)PyLong_AsUnsignedLong(py_ppid_arg);
		have_fast_filter=1;
	}

	if (KWARGS_HAVE(kwargs,"has_command") && KWARGS_GET(kwargs,"has_command") != Py_None) {
		PyObject* has_command = KWARGS_GET(kwargs,"has_command");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(has_command),"Invalid type of 'has_command' argument (expected 'bool')");
		if (PyObject_IsTrue(has_command)) {
			fast_filter->filter_set[2].func = libetrace_nfsdb_entry_command_filter_is_class;
			fast_filter->filter_set[2].arg = (const void*)FILE_CLASS_COMMAND;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"has_comp_info") && KWARGS_GET(kwargs,"has_comp_info") != Py_None) {
		PyObject* has_comp_info = KWARGS_GET(kwargs,"has_comp_info");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(has_comp_info),"Invalid type of 'has_comp_info' argument (expected 'bool')");
		if (PyObject_IsTrue(has_comp_info)) {
			fast_filter->filter_set[2].func = libetrace_nfsdb_entry_command_filter_is_class;
			fast_filter->filter_set[2].arg = (const void*)FILE_CLASS_COMPILING;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"has_linked_file") && KWARGS_GET(kwargs,"has_linked_file") != Py_None) {
		PyObject* has_linked_file = KWARGS_GET(kwargs,"has_linked_file");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(has_linked_file),"Invalid type of 'has_linked_file' argument (expected 'bool')");
		if (PyObject_IsTrue(has_linked_file)) {
			fast_filter->filter_set[2].func = libetrace_nfsdb_entry_command_filter_is_class;
			fast_filter->filter_set[2].arg = (const void*)FILE_CLASS_LINKING;
			have_fast_filter=1;
		}
	}

	if (KWARGS_HAVE(kwargs,"negate") && KWARGS_GET(kwargs,"negate") != Py_None) {
		PyObject* py_negate = KWARGS_GET(kwargs,"negate");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(py_negate),"Invalid type of 'negate' argument (expected 'bool')");
		fast_filter->negate = PyObject_IsTrue(py_negate);
	}

	if (KWARGS_HAVE(kwargs,"bin_at_source_root") && KWARGS_GET(kwargs,"bin_at_source_root") != Py_None) {
		PyObject* py_bin_at_source_root = KWARGS_GET(kwargs,"bin_at_source_root");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(py_bin_at_source_root),"Invalid type of 'bin_at_source_root' argument (expected 'bool')");
		if (PyObject_IsTrue(py_bin_at_source_root)) {
			fast_filter->filter_set[3].func = libetrace_nfsdb_entry_command_filter_bin_at_source_root;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"bin_not_at_source_root") && KWARGS_GET(kwargs,"bin_not_at_source_root") != Py_None) {
		PyObject* py_bin_not_at_source_root = KWARGS_GET(kwargs,"bin_not_at_source_root");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(py_bin_not_at_source_root),"Invalid type of 'bin_not_at_source_root' argument (expected 'bool')");
		if (PyObject_IsTrue(py_bin_not_at_source_root)) {
			fast_filter->filter_set[3].func = libetrace_nfsdb_entry_command_filter_bin_not_at_source_root;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"cwd_at_source_root") && KWARGS_GET(kwargs,"cwd_at_source_root") != Py_None) {
		PyObject* py_cwd_at_source_root = KWARGS_GET(kwargs,"cwd_at_source_root");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(py_cwd_at_source_root),"Invalid type of 'cwd_at_source_root' argument (expected 'bool')");
		if (PyObject_IsTrue(py_cwd_at_source_root)) {
			fast_filter->filter_set[3].func = libetrace_nfsdb_entry_command_filter_cwd_at_source_root;
			have_fast_filter=1;
		}
	}
	else if (KWARGS_HAVE(kwargs,"cwd_not_at_source_root") && KWARGS_GET(kwargs,"cwd_not_at_source_root") != Py_None) {
		PyObject* py_cwd_not_at_source_root = KWARGS_GET(kwargs,"cwd_not_at_source_root");
		ASSERT_WITH_NFSDB_ERROR(PyBool_Check(py_cwd_not_at_source_root),"Invalid type of 'cwd_not_at_source_root' argument (expected 'bool')");
		if (PyObject_IsTrue(py_cwd_not_at_source_root)) {
			fast_filter->filter_set[3].func = libetrace_nfsdb_entry_command_filter_cwd_not_at_source_root;
			have_fast_filter=1;
		}
	}

	return have_fast_filter;
}

static int parse_command_filters(libetrace_nfsdb_object *self, PyObject* filters, struct command_filter** cflts, size_t* cflts_size) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	for (Py_ssize_t i=0; i<PyList_Size(filters); ++i) {
		/* [ [(STR,PPID,CLASS,NEGATE,SRCROOT),(...),...], ... ] */
		PyObject* andSection = PyList_GetItem(filters,i);
		struct command_filter* andflts = calloc(sizeof(struct command_filter),PyList_Size(andSection));
		cflts[i] = andflts;
		cflts_size[i] = PyList_Size(andSection);
		for (Py_ssize_t j=0; j<PyList_Size(andSection); ++j) {
			/* (STR,PPID,CLASS,NEGATE,SRCROOT) */
			struct command_filter* fflt = &andflts[j];
			PyObject* filter = PyList_GetItem(andSection,j);
			PyObject* str_filter = PyTuple_GetItem(filter,0);
			if ((str_filter != Py_None)&&(PyTuple_Size(str_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(str_filter,0);
				PyObject* py_str_arg = PyTuple_GetItem(str_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyUnicode_Check(py_str_arg),"Invalid type of filter 'str' argument (expected 'str')");
				CHECK_ASSIGN_COMMAND_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[0].func = __func;
					if ((__func==libetrace_nfsdb_entry_command_filter_cwd_matches_re)||
							(__func==libetrace_nfsdb_entry_command_filter_cmd_matches_re)||
							(__func==libetrace_nfsdb_entry_command_filter_bin_matches_re)) {
						PyObject* osModuleString = PyUnicode_FromString((char*)"re");
						PyObject* osModule = PyImport_Import(osModuleString);
						PyObject* compileFunction = PyObject_GetAttrString(osModule,(char*)"compile");
						PyObject* args = PyTuple_New(1);
						PyTuple_SetItem(args,0,py_str_arg);
						Py_XINCREF(py_str_arg);
						PyObject* reObject = PyObject_CallObject(compileFunction, args);
						Py_DecRef(args);
						fflt->filter_set[0].arg = reObject;
						Py_DECREF(compileFunction);
						Py_DECREF(osModule);
						Py_DECREF(osModuleString);
					}
					else {
						fflt->filter_set[0].arg = PyString_get_c_str(py_str_arg);
					}
				);
			}
			if (PyTuple_Size(filter)<2) continue;
			PyObject* ppid_filter = PyTuple_GetItem(filter,1);
			if ((ppid_filter != Py_None)&&(PyTuple_Size(ppid_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(ppid_filter,0);
				PyObject* py_ppid_arg = PyTuple_GetItem(ppid_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyLong_Check(py_ppid_arg),"Invalid type of filter 'ppid' argument (expected 'int')");
				CHECK_ASSIGN_COMMAND_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[1].func = __func;
					fflt->filter_set[1].arg = (const void*)PyLong_AsUnsignedLong(py_ppid_arg);
				);
			}
			if (PyTuple_Size(filter)<3) continue;
			PyObject* class_filter = PyTuple_GetItem(filter,2);
			if ((class_filter != Py_None)&&(PyTuple_Size(class_filter)>=2)) {
				PyObject* fpkey = PyTuple_GetItem(class_filter,0);
				PyObject* py_class_arg = PyTuple_GetItem(class_filter,1);
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyLong_Check(py_class_arg),"Invalid type of filter 'class' argument (expected 'int')");
				unsigned long class_arg = PyLong_AsUnsignedLong(py_class_arg);
				ASSERT_CLEANUP_WITH_NFSDB_FORMAT_ERROR((class_arg&0xfff8)==0,
				"Invalid 'class' argument bits in %lu (expected maximum bit value 0x%x)",
				class_arg,FILE_CLASS_LINKING);
				CHECK_ASSIGN_COMMAND_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[2].func = __func;
					fflt->filter_set[2].arg = (const void*)class_arg;
				);
			}
			if (PyTuple_Size(filter)<4) continue;
			PyObject* negate_filter = PyTuple_GetItem(filter,3);
			if (negate_filter != Py_None) {
				ASSERT_CLEANUP_WITH_NFSDB_ERROR(PyBool_Check(negate_filter),"Invalid type of filter 'negate' argument (expected 'bool')");
				fflt->negate = PyObject_IsTrue(negate_filter);
			}
			if (PyTuple_Size(filter)<5) continue;
			PyObject* srcroot_filter = PyTuple_GetItem(filter,4);
			if ((srcroot_filter != Py_None)&&(PyTuple_Size(srcroot_filter)>=1)) {
				PyObject* fpkey = PyTuple_GetItem(srcroot_filter,0);
				CHECK_ASSIGN_COMMAND_FILTER_FUNC_WITH_FTDB_FORMAT_ERROR(fpkey,
					fflt->filter_set[3].func = __func;
				);
			}
		}
	}

	return 1;

cleanup_and_err:
	destroy_command_filters(PyList_Size(filters),cflts,cflts_size);
	return 0;
}

static int libetrace_nfsdb_filtered_opens_paths_filter_once(
	libetrace_nfsdb_object *self,
	struct nfsdb_fileMap_node* data,
	struct file_filter** cflts,
	size_t* cflts_size,
	unsigned long filter_count)
{

	int filter_result = 0;
	for (unsigned long i=0; i<filter_count; ++i) {
		struct file_filter* andflts = cflts[i];
		size_t andsz = cflts_size[i];
		int and_result = 1;
		for (size_t j=0; j<andsz; ++j) {
			struct file_filter* fflt = &andflts[j];
			int flt_result = 1;
			for (int n=0; n<FILE_FILTER_SIZE; ++n) {
				if (fflt->filter_set[n].func) {
					if (!fflt->filter_set[n].func(self,data,fflt->filter_set[n].arg,ULONG_MAX)) {
						flt_result=0;
						break;
					}
				}
			}
			if (fflt->negate) {
				flt_result = 1-flt_result;
			}
			if (flt_result==0) {
				and_result = 0;
				break;
			}
		} /* for (a*b*c) */
		if (and_result==1) {
			filter_result = 1;
			break;
		}
	}

	return filter_result;
}

static int libetrace_nfsdb_filtered_opens_filter_once(
	libetrace_nfsdb_object *self,
	struct nfsdb_fileMap_node* data,
	unsigned long index,
	struct file_filter** cflts,
	size_t* cflts_size,
	unsigned long filter_count)
{

	int filter_result = 0;
	for (unsigned long i=0; i<filter_count; ++i) {
		struct file_filter* andflts = cflts[i];
		size_t andsz = cflts_size[i];
		int and_result = 1;
		for (size_t j=0; j<andsz; ++j) {
			struct file_filter* fflt = &andflts[j];
			int flt_result = 1;
			for (int n=0; n<FILE_FILTER_SIZE; ++n) {
				if (fflt->filter_set[n].func) {
					if (!fflt->filter_set[n].func(self,data,fflt->filter_set[n].arg,index)) {
						flt_result=0;
						break;
					}
				}
			}
			if (fflt->negate) {
				flt_result = 1-flt_result;
			}
			if (flt_result==0) {
				and_result = 0;
				break;
			}
		} /* for (a*b*c) */
		if (and_result==1) {
			filter_result = 1;
			break;
		}
	}

	return filter_result;
}

static int libetrace_nfsdb_filtered_commands_filter_once(
	libetrace_nfsdb_object *self,
	struct nfsdb_entry* data,
	struct command_filter** cflts,
	size_t* cflts_size,
	unsigned long filter_count)
{

	int filter_result = 0;
	for (unsigned long i=0; i<filter_count; ++i) {
		struct command_filter* andflts = cflts[i];
		size_t andsz = cflts_size[i];
		int and_result = 1;
		for (size_t j=0; j<andsz; ++j) {
			struct command_filter* fflt = &andflts[j];
			int flt_result = 1;
			for (int n=0; n<COMMAND_FILTER_SIZE; ++n) {
				if (fflt->filter_set[n].func) {
					if (!fflt->filter_set[n].func(self,data,fflt->filter_set[n].arg)) {
						flt_result=0;
						break;
					}
				}
			}
			if (fflt->negate) {
				flt_result = 1-flt_result;
			}
			if (flt_result==0) {
				and_result = 0;
				break;
			}
		} /* for (a*b*c) */
		if (and_result==1) {
			filter_result = 1;
			break;
		}
	}

	return filter_result;
}

PyObject* libetrace_nfsdb_filtered_opens_paths(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	struct file_filter** cflts = 0;
	size_t* cflts_size = 0;
	struct file_filter* fflts[1] = {};
	size_t fflts_size[1] = {1};
	struct file_filter* fast_filter = 0;
	PyObject* paths = PyList_New(0);
	unsigned long filter_count = 0;

	if (PyTuple_Size(args)>0 || KWARGS_HAVE(kwargs,"file_filter")) {
		PyObject* filters = KWARGS_HAVE(kwargs,"file_filter") ?
				KWARGS_GET(kwargs,"file_filter") :
				PyTuple_GetItem(args,0);

		if (filters != Py_None) {
			filter_count = PyList_Size(filters);
			if (filter_count>0) {
				cflts = calloc(sizeof(struct file_filter*),filter_count);
				cflts_size = malloc(filter_count*sizeof(size_t));
				if (!parse_opens_filters(self,filters,cflts,cflts_size,1)) {
					free(cflts);
					free(cflts_size);
					Py_DecRef(paths);
					return 0;
				}
			}
		}
	}

	fast_filter = calloc(sizeof(struct file_filter),1);
	if (!parse_opens_fast_filter(self,kwargs,fast_filter,1)) {
		free(fast_filter);
		fast_filter = 0;
		if (PyErr_Occurred()) {
			Py_DecRef(paths);
			paths = 0;
			goto cleanup_and_exit;
		}
	}
	else {
		fflts[0] = fast_filter;
	}

	/* Make filtering */
	struct rb_node * p = rb_first(&self->nfsdb->filemap);
    while(p) {
        struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		if ((fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(self,data,fflts,fflts_size,1) : true) &&
			((cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(self,data,cflts,cflts_size,filter_count) : true)) {
			/* filter returned true */
			PyObject* s = PyUnicode_FromString(self->nfsdb->string_table[data->key]);
	        PyList_Append(paths,s);
	        Py_DecRef(s);
		}
        p = rb_next(p);
    }

cleanup_and_exit:
    if (cflts_size > 0) {
		destroy_opens_paths_filters(filter_count, cflts, cflts_size);
	}
	if (fast_filter) {
		destroy_opens_paths_filters(1, fflts, fflts_size);
	}

	free(cflts);
	free(cflts_size);
	free(fast_filter);

	return paths;
}

PyObject* libetrace_nfsdb_filtered_opens_paths_iter(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	Py_ssize_t patch_size = 1;
	if (PyTuple_Size(args)>1) {
		patch_size = PyLong_AsLong(PyTuple_GetItem(args,1));
	}
	else {
		if (KWARGS_HAVE(kwargs,"patch_size")) {
			patch_size = PyLong_AsLong(KWARGS_GET(kwargs,"patch_size"));
		}
	}

	PyObject* iter_args = PyTuple_New(4);
	PYTUPLE_SET_ULONG(iter_args,0,(uintptr_t)self);
	PyTuple_SetItem(iter_args,1,args);
	Py_XINCREF(args);
	PYTUPLE_SET_ULONG(iter_args,2,patch_size); /* step */
	PyTuple_SetItem(iter_args,3,kwargs);
	Py_XINCREF(kwargs);

	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbFilteredOpensPathsIterType, iter_args);
	Py_DecRef(iter_args);
	return iter;
}

static int pure_paths_single_filter(struct file_filter* flt) {

	/* Check if we have at least one filter that is not pure path filter */
	if ((flt->filter_set[1].func) && ((enum file_class)flt->filter_set[1].arg<=FILE_CLASS_PLAIN)) { /* class */
		return 0;
	}
	if ((flt->filter_set[3].func) && ((enum file_access)flt->filter_set[3].arg<=ACCESS_RW)) { /* access */
		return 0;
	}
	if (flt->filter_set[5].func) { /* srctype */
		return 0;
	}

	return 1;
}

static int pure_paths_filter_only(struct file_filter** cflts, size_t* cflts_size, unsigned long filter_count, struct file_filter* fast_filter) {

	for (unsigned long i=0; i<filter_count; ++i) {
		struct file_filter* andflts = cflts[i];
		for (size_t j=0; j<cflts_size[i]; ++j) {
			struct file_filter* flt = &andflts[j];
			if (!pure_paths_single_filter(flt)) {
				return 0;
			}
		}
	}
	if (fast_filter && (!pure_paths_single_filter(fast_filter))) {
		return 0;
	}

	return 1;
}

/* Filters all opened files in the database through a set of given filters
 * The filter specification is as follows:
 * [ <and_filter_spec0>, <and_filter_spec1>, ... ]
 * and the filter calculates the following boolean formula:
 * R = <and_filter_spec0> + <and_filter_spec1> + ...
 * The <and_filter_spec> is defined as follows:
 * [ <filter_spec0>, <filter_spec1>, ... ]
 * and the calculated boolean formula is as follows:
 * A = <filter_spec0> * <filter_spec1> * ...
 * Finally the <filter_spec> is defined as follows:
 * <filter_spec>: (<path_spec>,<class_spec>,<exists_spec>,<access_spec>,<negate_spec>,<srcroot_spec>,<srctype_spec>)
 * where:
 * <path_spec>*:
 *		('contains_path',<str>)
 * 		('matches_wc',<str>)
 * 		('matches_re',<str>)
 * <class_spec>: ('is_class',<int>)
 *  class specifiers:
 * 		LINKED: 			1>>0 [1]
 * 		LINKED_STATIC: 		1>>1 [2]
 * 		LINKED_SHARED: 		1>>2 [4]
 * 		LINKED_EXE: 		1>>3 [8]
 * 		COMPILED: 			1>>5 [16]
 * 		PLAIN: 				1>>6 [32]
 * 		COMPILER*: 			1>>7 [64]
 * 		LINKER*: 			1>>8 [128]
 * 		BINARY*: 			1>>9 [256]
 * 		SYMLINK*: 			1>>10 [512]
 * 		NOSYMLINK*:			1>>11 [1024]
 * <exists_spec>*:
 * 		('file_exists',)
 * 		('file_not_exists',)
 * 		('dir_exists',)
 * <access_spec>: ('has_access',<int>)
 *  access specifiers:
 * 		ACCESS_READ:			0
 *		ACCESS_WRITE:			1
 * 		ACCESS_RW:				2
 *		GLOBAL_ACCESS_READ*:	3
 *		GLOBAL_ACCESS_WRITE*:	4
 *		GLOBAL_ACCESS_RW*: 		5
 * <negate_spec>*: <bool>
 * <srcroot_spec>*:
 * 		('at_source_root',)
 * 		('not_at_source_root',)
 * <srctype_spec>: ('source_type',<int>)
 *  source type specifiers:
 * 		SOURCE_TYPE_C:		1
 * 		SOURCE_TYPE_CPP:	2
 * 		SOURCE_TYPE_OTHER:	4
 *
 * Filter specification can also be passed through keyword arguments. The limitation is that the keyword arguments
 *  only define a single filter specification <and_filter_spec>. The keyword specifiers are as follows:
 * <path_spec>:
 *		has_path=<str>
 * 		wc=<str>
 * 		re=<str>
 *		path=<list>
 * <class_spec>:
 * 		compiled=<bool>
 * 		linked=<bool>
 * 		linked_static=<bool>
 * 		linked_shared=<bool>
 * 		linked_exe=<bool>
 * 		plain=<bool>
 * 		compiler=<bool>
 * 		linker=<bool>
 * 		binary=<bool>
 * 		symlink=<bool>
 * 		no_symlink=<bool>
 * <exists_spec>:
 * 		file_exists=<bool>
 * 		file_not_exists=<bool>
 * 		dir_exists=<bool>
 * <access_spec>: has_access=<int>
 * <negate_spec>: negate=<bool>
 * <srcroot_spec>:
 * 		at_source_root=<bool>
 * 		not_at_source_root=<bool>
 * <srctype_spec>: source_type=<int>
 *
 * Please note that the <path_spec> specifier has additional keyword, i.e. 'path'. In this case the filter
 *  will match only paths provided through the 'path' parameter list.
 *
 * Filtering for files opened with a given access has two options. One option is the global access of files.
 *  When a file with the same path is opened for R and W at two different points in time, the global access
 *  will be defined as RW. In that the search for GLOBAL_ACCESS_RW will return both opened files described
 *  above. However the search for ACCESS_RW will not return this path (until there is specific opened file
 *  entry opened specifically with RW access flag).
 *
 * [*] Some of the specified filters can make filtering based on the openfile path only, others require
 *  filtering through all the openfile entries. Filtering only through the paths is much quicker therefore
 *  in case of a filter combination that can only goes through the paths there's a fast path of filtering
 *  involved. Filters that goes only through the paths while filtering are marked '*'
 */
PyObject* libetrace_nfsdb_filtered_opens(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	struct file_filter** cflts = 0;
	size_t* cflts_size = 0;
	struct file_filter* fflts[1] = {};
	size_t fflts_size[1] = {1};
	struct file_filter* fast_filter = 0;
	PyObject* opens = PyList_New(0);
	unsigned long filter_count = 0;
	int force_standard_filters = 0;

	if (KWARGS_HAVE(kwargs,"force_standard_filters") && (KWARGS_GET(kwargs,"force_standard_filters") != Py_None)) {
		/* Force standard filters for measurement purposes */
		force_standard_filters = PyObject_IsTrue(KWARGS_GET(kwargs,"force_standard_filters"));
	}

	if (PyTuple_Size(args)>0 || KWARGS_HAVE(kwargs,"file_filter")) {
		PyObject* filters = KWARGS_HAVE(kwargs,"file_filter") ?
				KWARGS_GET(kwargs,"file_filter") :
				PyTuple_GetItem(args,0);

		if (filters != Py_None) {
			filter_count = PyList_Size(filters);
			if (filter_count>0) {
				cflts = calloc(sizeof(struct file_filter*),filter_count);
				cflts_size = malloc(filter_count*sizeof(size_t));
				if (!parse_opens_filters(self,filters,cflts,cflts_size,0)) {
					free(cflts);
					free(cflts_size);
					Py_DecRef(opens);
					return 0;
				}
			}
		}
	}

	fast_filter = calloc(sizeof(struct file_filter),1);
	if (!parse_opens_fast_filter(self,kwargs,fast_filter,0)) {
		free(fast_filter);
		fast_filter = 0;
		if (PyErr_Occurred()) {
			Py_DecRef(opens);
			opens = 0;
			goto cleanup_and_exit;
		}
	}
	else {
		fflts[0] = fast_filter;
	}

	if (pure_paths_filter_only(cflts,cflts_size,filter_count,fast_filter) && (!force_standard_filters)) {
		/* Make filtering
		 * Faster filtering; we basically filter only based on paths and feed all the opens accordingly
		 */
		struct rb_node * p = rb_first(&self->nfsdb->filemap);
		while(p) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
			if ((fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(self,data,fflts,fflts_size,1) : true) &&
				((cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(self,data,cflts,cflts_size,filter_count) : true)) {
				/* filter returned true */
				for (unsigned long i=0; i<data->ga_entry_count; ++i) {
					struct nfsdb_entry* entry = data->ga_entry_list[i];
					unsigned long index = data->ga_entry_index[i];
					libetrace_nfsdb_entry_openfile_object* py_openfile = libetrace_nfsdb_create_openfile_entry(self->nfsdb,entry,index,entry->nfsdb_index);
					PyList_Append(opens,(PyObject*)py_openfile);
					Py_DecRef((PyObject*)py_openfile);
				}
			}
			p = rb_next(p);
		}
	}
	else {
		/* Make standard filtering */
		struct rb_node * p = rb_first(&self->nfsdb->filemap);
		while(p) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
			for (unsigned long i=0; i<data->ga_entry_count; ++i) {
				if ((fast_filter ? libetrace_nfsdb_filtered_opens_filter_once(self,data,i,fflts,fflts_size,1) : true) &&
					((cflts_size > 0) ? libetrace_nfsdb_filtered_opens_filter_once(self,data,i,cflts,cflts_size,filter_count) : true)) {
					/* filter returned true */
					struct nfsdb_entry* entry = data->ga_entry_list[i];
					unsigned long index = data->ga_entry_index[i];
					libetrace_nfsdb_entry_openfile_object* py_openfile = libetrace_nfsdb_create_openfile_entry(self->nfsdb,entry,index,entry->nfsdb_index);
					PyList_Append(opens,(PyObject*)py_openfile);
					Py_DecRef((PyObject*)py_openfile);
				}
			}
			p = rb_next(p);
		}
	}

cleanup_and_exit:
	if (cflts_size > 0) {
		destroy_opens_paths_filters(filter_count, cflts, cflts_size);
	}
	if (fast_filter) {
		destroy_opens_paths_filters(1, fflts, fflts_size);
	}

	free(cflts);
	free(cflts_size);
	free(fast_filter);
	return opens;
}

PyObject* libetrace_nfsdb_filtered_opens_iter(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	Py_ssize_t patch_size = 1;
	if (PyTuple_Size(args)>1) {
		patch_size = PyLong_AsLong(PyTuple_GetItem(args,1));
	}
	else {
		if (KWARGS_HAVE(kwargs,"patch_size")) {
			patch_size = PyLong_AsLong(KWARGS_GET(kwargs,"patch_size"));
		}
	}

	PyObject* iter_args = PyTuple_New(4);
	PYTUPLE_SET_ULONG(iter_args,0,(uintptr_t)self);
	PyTuple_SetItem(iter_args,1,args);
	Py_XINCREF(args);
	PYTUPLE_SET_ULONG(iter_args,2,patch_size); /* step */
	PyTuple_SetItem(iter_args,3,kwargs);
	Py_XINCREF(kwargs);

	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbFilteredOpensIterType, iter_args);
	Py_DecRef(iter_args);
	return iter;
}

/* Filters all executed commands in the database through a set of given filters
 * The filter specification is as follows:
 * [ <and_filter_spec0>, <and_filter_spec1>, ... ]
 * and the filter calculates the following boolean formula:
 * R = <and_filter_spec0> + <and_filter_spec1> + ...
 * The <and_filter_spec> is defined as follows:
 * [ <filter_spec0>, <filter_spec1>, ... ]
 * and the calculated boolean formula is as follows:
 * A = <filter_spec0> * <filter_spec1> * ...
 * Finally the <filter_spec> is defined as follows:
 * <filter_spec>: (<str_spec>,<ppid_spec>,<class_spec>,<negate_spec>,<srcroot_spec>)
 * where:
 * <str_spec>:
 *		('cwd_contains_path',<str>)
 * 		('cwd_matches_wc',<str>)
 * 		('cwd_matches_re',<str>)
 *		('cmd_has_string',<str>)
 * 		('cmd_matches_wc',<str>)
 * 		('cmd_matches_re',<str>)
 *		('bin_contains_path',<str>)
 * 		('bin_matches_wc',<str>)
 * 		('bin_matches_re',<str>)
 * <ppid_spec>: ('has_ppid',<int>)
 * <class_spec>: ('is_class',<int>)
 *  class specifiers:
 * 		COMMAND: 			1>>0 [1]
 * 		COMPILING: 			1>>1 [2]
 * 		LINKING: 			1>>2 [4]
 * <negate_spec>: <bool>
 * <srcroot_spec>:
 * 		('bin_at_source_root',)
 * 		('bin_not_at_source_root',)
 * 		('cwd_at_source_root',)
 * 		('cwd_not_at_source_root',)
 *
 * Filter specification can also be passed through keyword arguments. The limitation is that the keyword arguments
 *  only define a single filter specification <and_filter_spec>. The keyword specifiers are as follows:
 * <str_spec>:
 *		cwd_has_str=<str>
 * 		cwd_wc=<str>
 * 		cwd_re=<str>
 *		cmd_has_str=<str>
 * 		cmd_wc=<str>
 * 		cmd_re=<str>
 *		bin_has_str=<str>
 * 		bin_wc=<str>
 * 		bin_re=<str>
 *		bins=<list>
 * <ppid_spec>:
 *		has_ppid=<int>
 *		pids=<list>
 * <class_spec>:
 * 		has_command=<bool>
 * 		has_comp_info=<bool>
 * 		has_linked_file=<bool>
 * <negate_spec>: negate=<bool>
 * <srcroot_spec>:
 * 		bin_at_source_root=<bool>
 * 		bin_not_at_source_root=<bool>
 * 		cwd_at_source_root=<bool>
 * 		cwd_not_at_source_root=<bool>
 *
 * Please note that the <str_spec> specifier has additional keyword, i.e. 'bins'. In this case the filter
 *  will match only commands that execute binaries provided through the 'bins' parameter list.
 *
 * Please also note that the <ppid_spec> specifier has additional keyword, i.e. 'pids'. In this case the filter
 *  will match only commands that have pids provided through the 'pids' parameter list.
 */
PyObject* libetrace_nfsdb_filtered_execs(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	struct command_filter** cflts = 0;
	size_t* cflts_size = 0;
	struct command_filter* fflts[1] = {};
	size_t fflts_size[1] = {1};
	struct command_filter* fast_filter = 0;
	PyObject* execs = PyList_New(0);
	unsigned long filter_count = 0;

	if (PyTuple_Size(args)>0 || KWARGS_HAVE(kwargs,"exec_filter")) {
		PyObject* filters = KWARGS_HAVE(kwargs,"exec_filter") ?
				KWARGS_GET(kwargs,"exec_filter") :
				PyTuple_GetItem(args,0);

		if (filters != Py_None) {
			filter_count = PyList_Size(filters);
			if (filter_count) {
				cflts = calloc(sizeof(struct command_filter*),PyList_Size(filters));
				cflts_size = malloc(PyList_Size(filters)*sizeof(size_t));
				if (!parse_command_filters(self,filters,cflts,cflts_size)) {
					free(cflts);
					free(cflts_size);
					Py_DecRef(execs);
					return 0;
				}
			}
		}
	}

	fast_filter = calloc(sizeof(struct command_filter),1);
	if (!parse_commands_fast_filter(self,kwargs,fast_filter)) {
		free(fast_filter);
		fast_filter = 0;
		if (PyErr_Occurred()) {
			Py_DecRef(execs);
			execs = 0;
			goto cleanup_and_exit;
		}
	}
	else {
		fflts[0] = fast_filter;
	}

	/* Make filtering */
	for (unsigned long i=0; i<self->nfsdb->nfsdb_count; ++i) {
		struct nfsdb_entry* entry = &self->nfsdb->nfsdb[i];
		if ((fast_filter ? libetrace_nfsdb_filtered_commands_filter_once(self,entry,fflts,fflts_size,1): true) &&
			(cflts_size > 0 ? libetrace_nfsdb_filtered_commands_filter_once(self,entry,cflts,cflts_size,filter_count): true)) {
			/* filter returned true */
			PyObject* args = PyTuple_New(2);
			PYTUPLE_SET_ULONG(args,0,(uintptr_t)self->nfsdb);
			PYTUPLE_SET_ULONG(args,1,i);
			PyObject *entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
			Py_DECREF(args);
			PyList_Append(execs,entry);
			Py_DecRef(entry);
		}
	}

cleanup_and_exit:
	if (cflts_size > 0) {
		destroy_command_filters(filter_count,cflts,cflts_size);
	}
	if (fast_filter) {
		destroy_command_filters(1,fflts,fflts_size);
	}

	free(cflts);
	free(cflts_size);
	free(fast_filter);
	return execs;
}

PyObject* libetrace_nfsdb_filtered_execs_iter(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	Py_ssize_t patch_size = 1;
	if (PyTuple_Size(args)>1) {
		patch_size = PyLong_AsLong(PyTuple_GetItem(args,1));
	}
	else {
		if (KWARGS_HAVE(kwargs,"patch_size")) {
			patch_size = PyLong_AsLong(KWARGS_GET(kwargs,"patch_size"));
		}
	}

	PyObject* iter_args = PyTuple_New(4);
	PYTUPLE_SET_ULONG(iter_args,0,(uintptr_t)self);
	PyTuple_SetItem(iter_args,1,args);
	Py_XINCREF(args);
	PYTUPLE_SET_ULONG(iter_args,2,patch_size); /* step */
	PyTuple_SetItem(iter_args,3,kwargs);
	Py_XINCREF(kwargs);

	PyObject *iter = PyObject_CallObject((PyObject *) &libetrace_nfsdbFilteredCommandsIterType, iter_args);
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
		if (libetrace_nfsdb_entry_is_linking_internal(entry)) {
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
		if (node->access_type==FILE_ACCESS_TYPE_EXEC) {
			struct nfsdb_entryMap_node* binary_node = nfsdb_entryMap_search(&self->nfsdb->bmap,node->key);
			ASSERT_WITH_NFSDB_FORMAT_ERROR(binary_node,"Internal nfsdb error at binary path handle [%lu]",node->key);
			if (binary_node->custom_data) Py_RETURN_TRUE;
		}
		else {
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
		if (node->access_type!=FILE_ACCESS_TYPE_EXEC) {
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
		}
		Py_RETURN_FALSE;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid argument type (not a string)");
	return 0;
}

PyObject* libetrace_nfsdb_path_symlinked(libetrace_nfsdb_object *self, PyObject *args) {

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
		if (node->access_type!=FILE_ACCESS_TYPE_EXEC) {
			if (node->rd_entry_count>0) {
				struct nfsdb_entry* entry = node->rd_entry_list[0];
				unsigned long index = node->rd_entry_index[0];
				unsigned long mode = entry->open_files[index].mode;
				if (((mode&0x40)!=0)&&(((mode&0x3C)>>2)==0xA)) Py_RETURN_TRUE;
			}
			else if (node->rw_entry_count>0) {
				struct nfsdb_entry* entry = node->rw_entry_list[0];
				unsigned long index = node->rw_entry_index[0];
				unsigned long mode = entry->open_files[index].mode;
				if (((mode&0x40)!=0)&&(((mode&0x3C)>>2)==0xA)) Py_RETURN_TRUE;
			}
			else if (node->wr_entry_count>0) {
				struct nfsdb_entry* entry = node->wr_entry_list[0];
				unsigned long index = node->wr_entry_index[0];
				unsigned long mode = entry->open_files[index].mode;
				if (((mode&0x40)!=0)&&(((mode&0x3C)>>2)==0xA)) Py_RETURN_TRUE;
			}
		}
		Py_RETURN_FALSE;
	}

	PyErr_SetString(libetrace_nfsdbError, "Invalid argument type (not a string)");
	return 0;
}

PyObject* libetrace_nfsdb_symlink_paths(libetrace_nfsdb_object *self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	PyObject* path_symlinked = libetrace_nfsdb_path_symlinked(self,args);
	if (!path_symlinked) {
		return 0;
	}

	if (!PyObject_IsTrue(path_symlinked)) {
		PyErr_SetString(libetrace_nfsdbError, "The provided path has not been resolved from a symlink");
		return 0;
	}

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
		PyObject* pset = PySet_New(0);
		if (node->rd_entry_count>0) {
			struct nfsdb_entry* entry = node->rd_entry_list[0];
			unsigned long index = node->rd_entry_index[0];
			struct openfile* openfile = &entry->open_files[index];
			if (((openfile->mode&0x40)!=0)&&(((openfile->mode&0x3C)>>2)==0xA)) {
				if (openfile->original_path) {
					PySet_Add(pset,PyUnicode_FromString(self->nfsdb->string_table[*openfile->original_path]));
				}
			}
		}
		else if (node->rw_entry_count>0) {
			struct nfsdb_entry* entry = node->rw_entry_list[0];
			unsigned long index = node->rw_entry_index[0];
			struct openfile* openfile = &entry->open_files[index];
			if (((openfile->mode&0x40)!=0)&&(((openfile->mode&0x3C)>>2)==0xA)) {
				if (openfile->original_path) {
					PySet_Add(pset,PyUnicode_FromString(self->nfsdb->string_table[*openfile->original_path]));
				}
			}
		}
		else if (node->wr_entry_count>0) {
			struct nfsdb_entry* entry = node->wr_entry_list[0];
			unsigned long index = node->wr_entry_index[0];
			struct openfile* openfile = &entry->open_files[index];
			if (((openfile->mode&0x40)!=0)&&(((openfile->mode&0x3C)>>2)==0xA)) {
				if (openfile->original_path) {
					PySet_Add(pset,PyUnicode_FromString(self->nfsdb->string_table[*openfile->original_path]));
				}
			}
		}
		PyObject* argv = PyList_New(0);
		while(PySet_Size(pset)>0) {
			PyObject* p = PySet_Pop(pset);
			PyList_Append(argv,p);
		}
		Py_DecRef(pset);
		return argv;
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

    int debug = self->debug;
    int quiet = 0;
    bool err = true;

    if (kwargs) {
    	if (PyDict_Contains(kwargs,py_debug)) {
    		debug = PyLong_AsLong(PyDict_GetItem(kwargs,py_debug));
    	}
    	if (PyDict_Contains(kwargs,py_quiet)) {
			quiet = PyLong_AsLong(PyDict_GetItem(kwargs,py_quiet));
		}
    }

    DBG( debug, "--- libetrace_nfsdb_load(\"%s\")\n",cache_filename);

    if (self->init_done) {
    	PyErr_SetString(libetrace_nfsdbError, "nfsdb cache already initialized");
    	goto done;
	}

	FILE* in = fopen(cache_filename, "r+b");
	if (!in) {
		in = fopen(cache_filename, "rb");
		if(!in) {
			PyErr_Format(libetrace_nfsdbError, "Cannot open cache file - (%d) %s", errno, strerror(errno));
			goto done;
		}
	}

	int debug_level = 0;
	if(debug)
		debug_level = 2;
	else if (!quiet)
		debug_level = 1;

	self->unflatten = unflatten_init(debug_level);
	if(self->unflatten == NULL) {
		PyErr_SetString(libetrace_nfsdbError, "Failed to intialize unflatten library");
		goto done;
	}

	if (unflatten_load_continuous(self->unflatten, in, NULL)) {
		PyErr_SetString(libetrace_nfsdbError, "Failed to read cache file");
		unflatten_deinit(self->unflatten);
		fclose(in);
		goto done;
	}

	fclose(in);

    self->nfsdb = (const struct nfsdb*) unflatten_root_pointer_next(self->unflatten);

	/* Check whether it's correct file and in supported version */
	if(self->nfsdb->db_magic != NFSDB_MAGIC_NUMBER) {
		PyErr_Format(libetrace_nfsdbError, "Failed to parse cache file - invalid magic %p", self->nfsdb->db_magic);
		unflatten_deinit(self->unflatten);
		goto done;
	}
	if(self->nfsdb->db_version != LIBETRACE_VERSION) {
		PyErr_Format(libetrace_nfsdbError, "Failed to parse cache file - unsupported image version %p (required: %p)", 
						self->nfsdb->db_version, LIBETRACE_VERSION);
		unflatten_deinit(self->unflatten);
		goto done;
	}

	self->init_done = 1;
	err = false;

done:
	Py_DecRef(py_debug);
	Py_DecRef(py_quiet);
	PYASSTR_DECREF(cache_filename);
	if (err) 
		return NULL;	/* Indicate that an error has occurred */
	Py_RETURN_TRUE;
}

PyObject* libetrace_nfsdb_load_deps(libetrace_nfsdb_object* self, PyObject* args, PyObject* kwargs) {

	const char* cache_filename = ".nfsdb.deps.img";

	if (PyTuple_Size(args)>0) {
		cache_filename = PyBytes_AsString(PyUnicode_AsASCIIString(PyTuple_GetItem(args,0)));
	}

	PyObject* py_debug = PyUnicode_FromString("debug");
	PyObject* py_quiet = PyUnicode_FromString("quiet");

	int debug = self->debug;
	int quiet = 0;
	bool err = true;

	if (kwargs) {
		if (PyDict_Contains(kwargs,py_debug)) {
			debug = PyLong_AsLong(PyDict_GetItem(kwargs,py_debug));
		}
		if (PyDict_Contains(kwargs,py_quiet)) {
			quiet = PyLong_AsLong(PyDict_GetItem(kwargs,py_quiet));
		}
	}

	DBG( debug, "--- libetrace_nfsdb_load_deps(\"%s\")\n",cache_filename);

	if (self->nfsdb_deps) {
		PyErr_SetString(libetrace_nfsdbError, "nfsdb dependency cache already initialized");
		goto done;
	}

	FILE* in = fopen(cache_filename, "r+b");
	if (!in) {
		in = fopen(cache_filename, "rb");
		if(!in) {
			PyErr_Format(libetrace_nfsdbError, "Cannot open cache file - (%d) %s", errno, strerror(errno));
			goto done;
		}
	}

	int debug_level = 0;
	if(debug)
		debug_level = 2;
	else if (!quiet)
		debug_level = 1;

	self->unflatten_deps = unflatten_init(debug_level);
	if(self->unflatten_deps == NULL) {
		PyErr_SetString(libetrace_nfsdbError, "Failed to intialize unflatten library");
		goto done;
	}

	if (unflatten_load_continuous(self->unflatten_deps, in, NULL)) {
		PyErr_SetString(libetrace_nfsdbError, "Failed to read cache file");
		unflatten_deinit(self->unflatten_deps);
		fclose(in);
		goto done;
	}

	fclose(in);

	self->nfsdb_deps = (const struct nfsdb_deps*) unflatten_root_pointer_next(self->unflatten_deps);

	/* Check whether it's correct file and in supported version */
	if(self->nfsdb_deps->db_magic != NFSDB_DEPS_MAGIC_NUMBER) {
		PyErr_Format(libetrace_nfsdbError, "Failed to parse cache file - invalid magic %p", self->nfsdb_deps->db_magic);
		unflatten_deinit(self->unflatten);
		goto done;
	}
	if(self->nfsdb_deps->db_version != LIBETRACE_VERSION) {
		PyErr_Format(libetrace_nfsdbError, "Failed to parse cache file - unsupported image version %p (required: %p)", 
						self->nfsdb_deps->db_version, LIBETRACE_VERSION);
		unflatten_deinit(self->unflatten);
		goto done;
	}

	err = false;

done:
	Py_DecRef(py_debug);
	Py_DecRef(py_quiet);
	PYASSTR_DECREF(cache_filename);
	if (err) {
		self->nfsdb_deps = NULL;
		return NULL;
	}
	Py_RETURN_TRUE;
}

PyObject* libetrace_nfsdb_module_dependencies_count(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	PyObject* path_args = PyList_New(0);
	for (Py_ssize_t i=0; i<PyTuple_Size(args); ++i) {
		PyObject* arg = PyTuple_GetItem(args,i);
		if (PyUnicode_Check(arg)) {
			PyList_Append(path_args,arg);
		}
		else if (PyList_Check(arg)) {
			for (Py_ssize_t j=0; j<PyList_Size(arg); ++j) {
				PyObject* __arg = PyList_GetItem(arg,j);
				if (PyUnicode_Check(__arg)) {
					PyList_Append(path_args,__arg);
				}
				else {
					ASSERT_WITH_NFSDB_ERROR(0,"Invalid argument type: not (str) or (list)");
				}
			}
		}
		else {
			ASSERT_WITH_NFSDB_ERROR(0,"Invalid argument type: not (str) or (list)");
		}
	}

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

	PyObject* path_set = PySet_New(0);

	for (Py_ssize_t u=0; u<PyList_Size(path_args); ++u) {
		PyObject* parg = PyList_GetItem(path_args,u);
		const char* mpath = PyString_get_c_str(parg);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, mpath);
		PYASSTR_DECREF(mpath);
		if (!srefnode) {
			continue;
		}
		unsigned long hmpath = srefnode->value;
		struct ulongMap_node* node = ulongMap_search(dmap, hmpath);
		if (!node) {
			continue;
		}
		for (unsigned long i=0; i<node->value_count; i+=2) {
			unsigned long nfsdb_index = node->value_list[i];
			unsigned long open_index = node->value_list[i+1];
			PyObject* openfile_ptr = PyTuple_New(2);
			PYTUPLE_SET_ULONG(openfile_ptr,0,nfsdb_index);
			PYTUPLE_SET_ULONG(openfile_ptr,1,open_index);
			PYSET_ADD_PYOBJECT(path_set,openfile_ptr);
		}
	}

	Py_DecRef(py_direct);

	unsigned long dep_count = PySet_Size(path_set);
	Py_DecRef(path_set);

	return PyLong_FromUnsignedLong(dep_count);
}

PyObject* libetrace_nfsdb_module_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	PyObject* path_args = PyList_New(0);
	for (Py_ssize_t i=0; i<PyTuple_Size(args); ++i) {
		PyObject* arg = PyTuple_GetItem(args,i);
		if (PyUnicode_Check(arg)) {
			PyList_Append(path_args,arg);
		}
		else if (PyList_Check(arg)) {
			for (Py_ssize_t j=0; j<PyList_Size(arg); ++j) {
				PyObject* __arg = PyList_GetItem(arg,j);
				if (PyUnicode_Check(__arg)) {
					PyList_Append(path_args,__arg);
				}
				else {
					ASSERT_WITH_NFSDB_ERROR(0,"Invalid argument type: not (str) or (list)");
				}
			}
		}
		else {
			ASSERT_WITH_NFSDB_ERROR(0,"Invalid argument type: not (str) or (list)");
		}
	}

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

	PyObject* path_set = PySet_New(0);
	for (Py_ssize_t u=0; u<PyList_Size(path_args); ++u) {
		PyObject* parg = PyList_GetItem(path_args,u);
		const char* mpath = PyString_get_c_str(parg);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, mpath);
		PYASSTR_DECREF(mpath);
		if (!srefnode) {
			continue;
		}
		unsigned long hmpath = srefnode->value;
		struct ulongMap_node* node = ulongMap_search(dmap, hmpath);
		if (!node) {
			continue;
		}
		for (unsigned long i=0; i<node->value_count; i+=2) {
			unsigned long nfsdb_index = node->value_list[i];
			unsigned long open_index = node->value_list[i+1];
			PyObject* openfile_ptr = PyTuple_New(2);
			PYTUPLE_SET_ULONG(openfile_ptr,0,nfsdb_index);
			PYTUPLE_SET_ULONG(openfile_ptr,1,open_index);
			PYSET_ADD_PYOBJECT(path_set,openfile_ptr);
		}
	}

	while(PySet_Size(path_set)>0) {
		PyObject* openfile_ptr = PySet_Pop(path_set);
		unsigned long nfsdb_index = PyLong_AsUnsignedLong(PyTuple_GetItem(openfile_ptr,0));
		unsigned long open_index = PyLong_AsUnsignedLong(PyTuple_GetItem(openfile_ptr,1));
		Py_DecRef(openfile_ptr);
		const struct nfsdb_entry* entry = &self->nfsdb->nfsdb[nfsdb_index];
		libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(
				self->nfsdb,entry,open_index,nfsdb_index);
		PyList_Append(mL,(PyObject*)openfile);
	}

	Py_DecRef(py_direct);
	Py_DecRef(path_set);
	return mL;
}

PyObject* libetrace_nfsdb_reverse_module_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	PyObject* path_args = PyList_New(0);
	for (Py_ssize_t i=0; i<PyTuple_Size(args); ++i) {
		PyObject* arg = PyTuple_GetItem(args,i);
		if (PyUnicode_Check(arg)) {
			PyList_Append(path_args,arg);
		}
		else if (PyList_Check(arg)) {
			for (Py_ssize_t j=0; j<PyList_Size(arg); ++j) {
				PyObject* __arg = PyList_GetItem(arg,j);
				if (PyUnicode_Check(__arg)) {
					PyList_Append(path_args,__arg);
				}
				else {
					ASSERT_WITH_NFSDB_ERROR(0,"Invalid argument type: not (str) or (list)");
				}
			}
		}
		else {
			ASSERT_WITH_NFSDB_ERROR(0,"Invalid argument type: not (str) or (list)");
		}
	}

	PyObject* py_recursive = PyUnicode_FromString("recursive");
	int recursive = 0;

	if (kwargs) {
		if (PyDict_Contains(kwargs,py_recursive)) {
			recursive = PyLong_AsLong(PyDict_GetItem(kwargs,py_recursive));
		}
	}

	if (!self->nfsdb_deps) {
		PyErr_SetString(libetrace_nfsdbError, "nfsdb dependency cache not initialized");
		Py_DecRef(py_recursive);
		return 0;
	}

	const struct rb_root* rdmap = 0;
	if (recursive) {
		rdmap = &self->nfsdb_deps->revdepmap;
	}
	else {
		rdmap = &self->nfsdb_deps->revddepmap;
	}

	PyObject* path_set = PySet_New(0);
	for (Py_ssize_t u=0; u<PyList_Size(path_args); ++u) {
		PyObject* parg = PyList_GetItem(path_args,u);
		const char* mpath = PyString_get_c_str(parg);
		struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, mpath);
		PYASSTR_DECREF(mpath);
		if (!srefnode) {
			continue;
		}
		unsigned long hmpath = srefnode->value;
		struct ulongMap_node* node = ulongMap_search(rdmap, hmpath);
		if (!node) {
			continue;
		}
		for (unsigned long i=0; i<node->value_count; ++i) {
			unsigned long dep = node->value_list[i];
			PyObject* py_dep = PyUnicode_FromString(self->nfsdb->string_table[dep]);
			PYSET_ADD_PYOBJECT(path_set,py_dep);
		}
	}

	Py_DecRef(py_recursive);
	return path_set;
}

PyObject* libetrace_nfsdb_filemap_has_path(libetrace_nfsdb_object *self, PyObject *args) {

	PyObject* path = PyTuple_GetItem(args,0);
	const char* fpath = PyString_get_c_str(path);
	struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
	if (!srefnode) {
		Py_RETURN_FALSE;
	}
	unsigned long hfpath = srefnode->value;
	struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
	if (!node) {
		Py_RETURN_FALSE;
	}
	PYASSTR_DECREF(fpath);
	Py_RETURN_TRUE;
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
		if (self->entry->open_files[u].original_path) {
			FTDB_SET_ENTRY_STRING(openEntry,o,self->nfsdb->string_table[*self->entry->open_files[u].original_path]);
		}
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
		FTDB_SET_ENTRY_INT(ci,p,!self->entry->compilation_info->integrated_compilation);
		FTDB_SET_ENTRY_PYOBJECT(json,d,ci);
	}

	if (self->entry->linked_file) {
		FTDB_SET_ENTRY_STRING(json,l,self->nfsdb->string_table[*self->entry->linked_file]);
		FTDB_SET_ENTRY_INT(json,t,self->entry->linked_type);
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

int libetrace_nfsdb_entry_has_compilations_internal(const struct nfsdb_entry * entry) {

	int __used = 0;

	if (entry->compilation_info) {
		for (unsigned long i=0; i<entry->compilation_info->compiled_count; ++i) {
			if (entry->compilation_info->compiled_index[i].__used) {
				__used = 1;
				break;
			}
		}
	}

	return __used;
}

PyObject* libetrace_nfsdb_entry_has_compilations(libetrace_nfsdb_entry_object *self, PyObject *args) {

	if (libetrace_nfsdb_entry_has_compilations_internal(self->entry)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

int libetrace_nfsdb_entry_is_linking_internal(const struct nfsdb_entry * entry) {

	if ((entry->linked_file)&&(entry->linked_index.__used)) {
		return 1;
	}

	return 0;
}

PyObject* libetrace_nfsdb_entry_is_linking(libetrace_nfsdb_entry_object *self, PyObject *args) {

	if (libetrace_nfsdb_entry_is_linking_internal(self->entry)) {
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

PyObject* libetrace_nfsdb_entry_get_argvn(PyObject* self, PyObject *args) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	unsigned long index = PyLong_AsUnsignedLong(PyTuple_GetItem(args,0));
	ASSERT_WITH_NFSDB_FORMAT_ERROR(index<__self->entry->argv_count,"Invalid index value to 'argv' table [%lu]",index);
	return PyUnicode_FromString(__self->nfsdb->string_table[__self->entry->argv[index]]);
}

PyObject* libetrace_nfsdb_entry_get_ptr(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	return PyLong_FromUnsignedLong(__self->nfsdb_index);
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

PyObject* libetrace_nfsdb_entry_get_parent(PyObject* self, void* closure) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->procmap,__self->entry->parent_eid.pid);
	ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Invalid pid key [%ld] at nfsdb entry",__self->entry->parent_eid.pid);
	ASSERT_WITH_NFSDB_FORMAT_ERROR(__self->entry->parent_eid.exeidx<node->entry_count,
			"nfsdb entry index [%ld] out of range",__self->entry->parent_eid.exeidx);
	struct nfsdb_entry* entry = node->entry_list[__self->entry->parent_eid.exeidx];

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,entry->nfsdb_index);
	PyObject *entryObj = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
	Py_DECREF(args);
	return entryObj;
}

PyObject* libetrace_nfsdb_entry_get_child_cids(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* cL = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->procmap,__self->entry->child_ids[i].pid);
		if (!node) {
			printf("WARNING: Missing child pid entry [%ld] created from process [%ld:%ld]\n",__self->entry->child_ids[i].pid,__self->entry->eid.pid,__self->entry->eid.exeidx);
			continue;
		}
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,__self->entry->child_ids[i].pid);
		PYTUPLE_SET_ULONG(args,1,__self->entry->child_ids[i].flags);
		PyObject *cid = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryCidType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(cL,cid);
	}

	return cL;
}

PyObject* libetrace_nfsdb_entry_get_childs(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* cL = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->child_ids_count; ++i) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->procmap,__self->entry->child_ids[i].pid);
		if (!node) {
			printf("WARNING: Missing child pid entry [%ld] created from process [%ld:%ld]\n",__self->entry->child_ids[i].pid,__self->entry->eid.pid,__self->entry->eid.exeidx);
			continue;
		}
		struct nfsdb_entry* entry = node->entry_list[0];

		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
		PYTUPLE_SET_ULONG(args,1,entry->nfsdb_index);
		PyObject *child_entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
		Py_DECREF(args);

		PYLIST_ADD_PYOBJECT(cL,child_entry);
	}

	return cL;
}

PyObject* libetrace_nfsdb_entry_get_next_entry(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	unsigned long next_index = __self->entry->nfsdb_index+1;

	if ((next_index<__self->nfsdb->nfsdb_count) && (__self->nfsdb->nfsdb[next_index].eid.pid==__self->entry->eid.pid)) {
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
		PYTUPLE_SET_ULONG(args,1,next_index);
		PyObject *next_entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
		Py_DECREF(args);
		return next_entry;
	}

	Py_RETURN_NONE;
}

PyObject* libetrace_nfsdb_entry_get_prev_entry(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;

	if (__self->entry->nfsdb_index<=0) {
		Py_RETURN_NONE;
	}
	unsigned long prev_index = __self->entry->nfsdb_index-1;

	if (__self->nfsdb->nfsdb[prev_index].eid.pid==__self->entry->eid.pid) {
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
		PYTUPLE_SET_ULONG(args,1,prev_index);
		PyObject *prev_entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
		Py_DECREF(args);
		return prev_entry;
	}

	Py_RETURN_NONE;
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

const char* libetrace_nfsdb_string_handle_join(const struct nfsdb* nfsdb, unsigned long* argv, unsigned long argv_count, const char* sep) {

    size_t sep_size = strlen(sep);
    size_t arglen = 0;
    for (unsigned long i=0; i<argv_count; ++i) {
        arglen+=strlen(nfsdb->string_table[argv[i]]);
    }
    unsigned long alloc_size = (argv_count>0)?(arglen+sep_size*(argv_count-1)+1):1;
    char* rs = malloc(alloc_size);
    unsigned long pi = 0;
    if (argv_count>0) {
        for (unsigned long i=0; i<argv_count; ++i) {
        	const char* srgv = nfsdb->string_table[argv[i]];
            size_t argvlen = strlen(srgv);
            memcpy(&rs[pi],srgv,argvlen);
            pi+=argvlen;
            if (i+1<argv_count) {
                memcpy(&rs[pi],sep,sep_size);
                pi+=sep_size;
            }
        }
    }
    rs[pi] = 0;
    return rs;
}

PyObject* libetrace_nfsdb_entry_get_command(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	const char* cmdstr = libetrace_nfsdb_string_handle_join(__self->nfsdb,__self->entry->argv,__self->entry->argv_count," ");
	PyObject* cmd = PyUnicode_FromString(cmdstr);
	free((void*)cmdstr);
	return cmd;
}

PyObject* libetrace_nfsdb_entry_get_return_code(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	return PyLong_FromLong(__self->entry->return_code);
}

PyObject* libetrace_nfsdb_entry_get_openfiles(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_object* __self = (libetrace_nfsdb_entry_object*)self;
	PyObject* opens = PyList_New(0);

	for (unsigned long i=0; i<__self->entry->open_files_count; ++i) {

		//TODO: libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,__self->entry,i,__self->nfsdb_index);

		PyObject* args = PyTuple_New(8);
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
		if (__self->entry->open_files[i].opaque_entry) {
			PYTUPLE_SET_ULONG(args,7,__self->entry->open_files[i].opaque_entry->nfsdb_index);
		}
		else {
			PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
		}
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

		//TODO: libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(nfsdb,entry,i,entry->nfsdb_index);

		PyObject* args = PyTuple_New(8);
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
		if (entry->open_files[i].opaque_entry) {
			PYTUPLE_SET_ULONG(args,7,entry->open_files[i].opaque_entry->nfsdb_index);
		}
		else {
			PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
		}
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

			//TODO: libetrace_nfsdb_entry_openfile_object* py_openfile = libetrace_nfsdb_create_openfile_entry(nfsdb,entry,i,entry->nfsdb_index);

			PyObject* args = PyTuple_New(8);
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
			if (entry->open_files[i].opaque_entry) {
				PYTUPLE_SET_ULONG(args,7,entry->open_files[i].opaque_entry->nfsdb_index);
			}
			else {
				PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
			}
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

			//TODO: libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,__self->entry,i,__self->entry->nfsdb_index);

			PyObject* args = PyTuple_New(8);
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
			if (__self->entry->open_files[i].opaque_entry) {
				PYTUPLE_SET_ULONG(args,7,__self->entry->open_files[i].opaque_entry->nfsdb_index);
			}
			else {
				PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
			}
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

			//TODO: libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,__self->entry,i,__self->entry->nfsdb_index);

			PyObject* args = PyTuple_New(8);
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
			if (__self->entry->open_files[i].opaque_entry) {
				PYTUPLE_SET_ULONG(args,7,__self->entry->open_files[i].opaque_entry->nfsdb_index);
			}
			else {
				PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
			}
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

	if ((!__self->entry->linked_file)||(!__self->entry->linked_index.__used)) {
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

	if ((!__self->entry->linked_file)||(!__self->entry->linked_index.__used)) {
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
	Py_RETURN_RICHCOMPARE_internal(__self->nfsdb_index, __other->nfsdb_index , op);
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

void libetrace_nfsdb_filtered_opens_paths_iter_dealloc(libetrace_nfsdb_filtered_opens_paths_iter_object* self) {

	if (self->cflts_size > 0) {
		destroy_opens_paths_filters(self->filter_count, self->cflts, self->cflts_size);
	}
	if (self->fast_filter) {
		destroy_opens_paths_filters(1, self->fflts, self->fflts_size);
	}

	free(self->cflts);
	free(self->cflts_size);
	free(self->fast_filter);

	PyTypeObject *tp = Py_TYPE(self);
	tp->tp_free(self);
}

PyObject* libetrace_nfsdb_filtered_opens_paths_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_filtered_opens_paths_iter_object* self;

	self = (libetrace_nfsdb_filtered_opens_paths_iter_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->nfsdb = (libetrace_nfsdb_object*)PyLong_AsLong(PyTuple_GetItem(args,0));
		PyObject* iter_args = PyTuple_GetItem(args,1);
		self->patch_size = PyLong_AsLong(PyTuple_GetItem(args,2));
		PyObject* iter_kwargs = PyTuple_GetItem(args,3);
		self->cflts = 0;
		self->cflts_size = 0;
		self->fflts[0] = 0;
		self->fflts_size[0] = 1;
		self->fast_filter = 0;
		self->filter_count = 0;

		if (PyTuple_Size(iter_args)>0 || KWARGS_HAVE(iter_kwargs,"file_filter")) {
			PyObject* filters = KWARGS_HAVE(iter_kwargs,"file_filter") ?
					KWARGS_GET(iter_kwargs,"file_filter") :
					PyTuple_GetItem(iter_args,0);

			if (filters != Py_None) {
				self->filter_count = PyList_Size(filters);
				if (self->filter_count>0) {
					self->cflts = calloc(sizeof(struct file_filter*),self->filter_count);
					self->cflts_size = malloc(self->filter_count*sizeof(size_t));
					if (!parse_opens_filters(self->nfsdb,filters,self->cflts,self->cflts_size,1)) {
						free(self->cflts);
						free(self->cflts_size);
						return 0;
					}
				}
			}
		}

		self->fast_filter = calloc(sizeof(struct file_filter),1);
		if (!parse_opens_fast_filter(self->nfsdb,iter_kwargs,self->fast_filter,1)) {
			free(self->fast_filter);
			self->fast_filter = 0;
			if (PyErr_Occurred()) {
				free(self->cflts);
				free(self->cflts_size);
				return 0;
			}
		}
		else {
			self->fflts[0] = self->fast_filter;
		}


		/* Set the iterator to the first filtered opens path (if possible) */
		self->filemap_node = rb_first(&self->nfsdb->nfsdb->filemap);
		while(self->filemap_node) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)self->filemap_node;
			if ((self->fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(self->nfsdb,data,self->fflts,self->fflts_size,1) : true) &&
				((self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(self->nfsdb,data,self->cflts,self->cflts_size,self->filter_count) : true)) {
				/* filter returned true */
				break;
			}
			self->filemap_node = rb_next(self->filemap_node);
		}
	}

	return (PyObject *)self;
}

Py_ssize_t libetrace_nfsdb_filtered_opens_paths_iter_sq_length(PyObject* self) {

	libetrace_nfsdb_filtered_opens_paths_iter_object* __self = (libetrace_nfsdb_filtered_opens_paths_iter_object*)self;

	unsigned long entry_count = 0;
	struct rb_node * p = rb_first(&__self->nfsdb->nfsdb->filemap);
	while(p) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
		if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->fflts,__self->fflts_size,1) : true) &&
			((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
			/* filter returned true */
			entry_count++;
		}
		p = rb_next(p);
	}

	return entry_count;
}

PyObject* libetrace_nfsdb_filtered_opens_paths_iter_next(PyObject *self) {

	libetrace_nfsdb_filtered_opens_paths_iter_object* __self = (libetrace_nfsdb_filtered_opens_paths_iter_object*)self;

	if (!__self->filemap_node) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* paths = PyList_New(0);
	/* __self->filemap_node is pointing to the first filtered path */
	struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)__self->filemap_node;
	PyObject* s = PyUnicode_FromString(__self->nfsdb->nfsdb->string_table[data->key]);
    PyList_Append(paths,s);
    Py_DecRef(s);
    __self->filemap_node = rb_next(__self->filemap_node);

	/* Find more entries to fill the 'patch_size' paths */
	while(__self->filemap_node) {
		struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)__self->filemap_node;
		if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->fflts,__self->fflts_size,1) : true) &&
			((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
			/* filter returned true */
			if (__self->patch_size>PyList_Size(paths)) {
				PyObject* s = PyUnicode_FromString(__self->nfsdb->nfsdb->string_table[data->key]);
				PyList_Append(paths,s);
				Py_DecRef(s);
			}
			else {
				break;
			}
		}
		__self->filemap_node = rb_next(__self->filemap_node);
	}

	if (__self->patch_size>1) {
		return paths;
	}
	else {
		PyObject* path = PyList_GetItem(paths,0);
		Py_XINCREF(path);
		Py_DecRef(paths);
		return path;
	}
}

void libetrace_nfsdb_filtered_opens_iter_dealloc(libetrace_nfsdb_filtered_opens_iter_object* self) {

	if (self->cflts_size > 0) {
		destroy_opens_paths_filters(self->filter_count, self->cflts, self->cflts_size);
	}
	if (self->fast_filter) {
		destroy_opens_paths_filters(1, self->fflts, self->fflts_size);
	}

	free(self->cflts);
	free(self->cflts_size);
	free(self->fast_filter);

	PyTypeObject *tp = Py_TYPE(self);
	tp->tp_free(self);
}

PyObject* libetrace_nfsdb_filtered_opens_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_filtered_opens_iter_object* self;

	self = (libetrace_nfsdb_filtered_opens_iter_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->nfsdb = (libetrace_nfsdb_object*)PyLong_AsLong(PyTuple_GetItem(args,0));
		PyObject* iter_args = PyTuple_GetItem(args,1);
		self->patch_size = PyLong_AsLong(PyTuple_GetItem(args,2));
		PyObject* iter_kwargs = PyTuple_GetItem(args,3);
		self->cflts = 0;
		self->cflts_size = 0;
		self->fflts[0] = 0;
		self->fflts_size[0] = 1;
		self->fast_filter = 0;
		self->filter_count = 0;
		int force_standard_filters = 0;

		if (PyTuple_Size(iter_args)>0 || KWARGS_HAVE(iter_kwargs,"file_filter")) {
			PyObject* filters = KWARGS_HAVE(iter_kwargs,"file_filter") ?
					KWARGS_GET(iter_kwargs,"file_filter") :
					PyTuple_GetItem(iter_args,0);

			if (filters != Py_None) {
				self->filter_count = PyList_Size(filters);
				if (self->filter_count>0) {
					self->cflts = calloc(sizeof(struct file_filter*),self->filter_count);
					self->cflts_size = malloc(self->filter_count*sizeof(size_t));
					if (!parse_opens_filters(self->nfsdb,filters,self->cflts,self->cflts_size,0)) {
						free(self->cflts);
						free(self->cflts_size);
						return 0;
					}
				}
			}
		}

		if (KWARGS_HAVE(iter_kwargs,"force_standard_filters") && (KWARGS_GET(iter_kwargs,"force_standard_filters") != Py_None)) {
			/* Force standard filters for measurement purposes */
			force_standard_filters = PyObject_IsTrue(KWARGS_GET(iter_kwargs,"force_standard_filters"));
		}

		self->fast_filter = calloc(sizeof(struct file_filter),1);
		if (!parse_opens_fast_filter(self->nfsdb,iter_kwargs,self->fast_filter,0)) {
			free(self->fast_filter);
			self->fast_filter = 0;
			if (PyErr_Occurred()) {
				free(self->cflts);
				free(self->cflts_size);
				return 0;
			}
		}
		else {
			self->fflts[0] = self->fast_filter;
		}

		self->path_index = 0;
		/* Set the iterator to the first filtered open(if possible) */
		if (pure_paths_filter_only(self->cflts,self->cflts_size,self->filter_count,self->fast_filter) && (!force_standard_filters)) {
			self->filemap_node = rb_first(&self->nfsdb->nfsdb->filemap);
			while(self->filemap_node) {
				struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)self->filemap_node;
				if ((self->fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(self->nfsdb,data,self->fflts,self->fflts_size,1) : true) &&
					((self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(self->nfsdb,data,self->cflts,self->cflts_size,self->filter_count) : true)) {
					/* filter returned true */
					break;
				}
				self->filemap_node = rb_next(self->filemap_node);
			}
		}
		else {
			self->filemap_node = rb_first(&self->nfsdb->nfsdb->filemap);
			while(self->filemap_node) {
				struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)self->filemap_node;
				for (self->path_index=0; self->path_index<data->ga_entry_count; ++self->path_index) {
					if ((self->fast_filter ? libetrace_nfsdb_filtered_opens_filter_once(self->nfsdb,data,self->path_index,self->fflts,self->fflts_size,1) : true) &&
						((self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_filter_once(self->nfsdb,data,self->path_index,self->cflts,self->cflts_size,self->filter_count) : true)) {
						/* filter returned true */
						goto done;
					}
				}
				self->filemap_node = rb_next(self->filemap_node);
			}
		}
	}
done:
	return (PyObject *)self;
}

Py_ssize_t libetrace_nfsdb_filtered_opens_iter_sq_length(PyObject* self) {

	libetrace_nfsdb_filtered_opens_iter_object* __self = (libetrace_nfsdb_filtered_opens_iter_object*)self;

	unsigned long entry_count = 0;
	struct rb_node * p = rb_first(&__self->nfsdb->nfsdb->filemap);
	if (pure_paths_filter_only(__self->cflts,__self->cflts_size,__self->filter_count,__self->fast_filter)) {
		while(p) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
			if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->fflts,__self->fflts_size,1) : true) &&
				((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
				/* filter returned true */
				entry_count+=data->ga_entry_count;
			}
			p = rb_next(p);
		}
	}
	else {
		while(p) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)p;
			for (unsigned long i=0; i<data->ga_entry_count; ++i) {
				if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_filter_once(__self->nfsdb,data,i,__self->fflts,__self->fflts_size,1) : true) &&
					((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_filter_once(__self->nfsdb,data,i,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
					/* filter returned true */
					entry_count++;
				}
			}
			p = rb_next(p);
		}
	}

    return entry_count;
}

PyObject* libetrace_nfsdb_filtered_opens_iter_next(PyObject *self) {

	libetrace_nfsdb_filtered_opens_iter_object* __self = (libetrace_nfsdb_filtered_opens_iter_object*)self;

	if (!__self->filemap_node) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* opens = PyList_New(0);
	unsigned long patch_count = 0;
	/* __self->filemap_node is pointing to the first filtered path
	 * __self->path_index is pointing to the current nfsdbOpenEntry within the filtered path
	 */

	if (pure_paths_filter_only(__self->cflts,__self->cflts_size,__self->filter_count,__self->fast_filter)) {
		while(__self->filemap_node) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)__self->filemap_node;
			/* Find entries to fill the 'patch_size' paths */
			unsigned long i;
			for (i=__self->path_index; i<data->ga_entry_count; ++i) {
				struct nfsdb_entry* entry = data->ga_entry_list[i];
				unsigned long index = data->ga_entry_index[i];
				libetrace_nfsdb_entry_openfile_object* py_openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb->nfsdb,entry,index,entry->nfsdb_index);
				PyList_Append(opens,(PyObject*)py_openfile);
				Py_DecRef((PyObject*)py_openfile);
				__self->path_index++;
				patch_count++;
				if (patch_count>=__self->patch_size) {
					/* We're done */
					break;
				}
			}
			if (__self->path_index>=data->ga_entry_count) {
				__self->path_index=0;
			    __self->filemap_node = rb_next(__self->filemap_node);
			    while(__self->filemap_node) {
					struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)__self->filemap_node;
					if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->fflts,__self->fflts_size,1) : true) &&
						((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_paths_filter_once(__self->nfsdb,data,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
						/* filter returned true */
						break;
					}
					__self->filemap_node = rb_next(__self->filemap_node);
				}
			}
			if (patch_count>=__self->patch_size) {
				break;
			}
		}
	}
	else {
main_loop:
		while(__self->filemap_node) {
			struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)__self->filemap_node;
			while(__self->path_index<data->ga_entry_count) {
				struct nfsdb_entry* entry = data->ga_entry_list[__self->path_index];
				unsigned long index = data->ga_entry_index[__self->path_index];
				libetrace_nfsdb_entry_openfile_object* py_openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb->nfsdb,entry,index,entry->nfsdb_index);
				PyList_Append(opens,(PyObject*)py_openfile);
				Py_DecRef((PyObject*)py_openfile);
				patch_count++;
				__self->path_index++;
				/* Find entries to fill the 'patch_size' paths */
				for (; __self->path_index<data->ga_entry_count; ++__self->path_index) {
					if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_filter_once(__self->nfsdb,data,__self->path_index,__self->fflts,__self->fflts_size,1) : true) &&
						((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_filter_once(__self->nfsdb,data,__self->path_index,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
						/* filter returned true */
						if (patch_count>=__self->patch_size) {
							goto done;
						}
						goto main_loop;
					}
				}
			}
			__self->filemap_node = rb_next(__self->filemap_node);
			while(__self->filemap_node) {
				struct nfsdb_fileMap_node* data = (struct nfsdb_fileMap_node*)__self->filemap_node;
				for (__self->path_index=0; __self->path_index<data->ga_entry_count; ++__self->path_index) {
					if ((__self->fast_filter ? libetrace_nfsdb_filtered_opens_filter_once(__self->nfsdb,data,__self->path_index,__self->fflts,__self->fflts_size,1) : true) &&
						((__self->cflts_size > 0) ? libetrace_nfsdb_filtered_opens_filter_once(__self->nfsdb,data,__self->path_index,__self->cflts,__self->cflts_size,__self->filter_count) : true)) {
						/* filter returned true */
						if (patch_count>=__self->patch_size) {
							goto done;
						}
						goto main_loop;
					}
				}
				__self->filemap_node = rb_next(__self->filemap_node);
			}
		}
	}

done:
	if (__self->patch_size>1) {
		return opens;
	}
	else {
		PyObject* py_openfile = PyList_GetItem(opens,0);
		Py_XINCREF(py_openfile);
		Py_DecRef(opens);
		return py_openfile;
	}
}

void libetrace_nfsdb_filtered_commands_iter_dealloc(libetrace_nfsdb_filtered_commands_iter_object* self) {

	if (self->cflts_size > 0) {
		destroy_command_filters(self->filter_count,self->cflts,self->cflts_size);
	}
	if (self->fast_filter) {
		destroy_command_filters(1,self->fflts,self->fflts_size);
	}

	free(self->cflts);
	free(self->cflts_size);
	free(self->fast_filter);

	PyTypeObject *tp = Py_TYPE(self);
	tp->tp_free(self);
}

PyObject* libetrace_nfsdb_filtered_commands_iter_new(PyTypeObject *subtype, PyObject *args, PyObject *kwds) {

	libetrace_nfsdb_filtered_commands_iter_object* self;

	self = (libetrace_nfsdb_filtered_commands_iter_object*)subtype->tp_alloc(subtype, 0);
	if (self != 0) {
		self->nfsdb = (libetrace_nfsdb_object*)PyLong_AsLong(PyTuple_GetItem(args,0));
		PyObject* iter_args = PyTuple_GetItem(args,1);
		self->patch_size = PyLong_AsLong(PyTuple_GetItem(args,2));
		PyObject* iter_kwargs = PyTuple_GetItem(args,3);
		self->cflts = 0;
		self->cflts_size = 0;
		self->fflts[0] = 0;
		self->fflts_size[0] = 1;
		self->fast_filter = 0;
		self->filter_count = 0;

		if (PyTuple_Size(iter_args)>0 || KWARGS_HAVE(iter_kwargs,"exec_filter")) {
			PyObject* filters = KWARGS_HAVE(iter_kwargs,"exec_filter") ?
					KWARGS_GET(iter_kwargs,"exec_filter") :
					PyTuple_GetItem(iter_args,0);

			if (filters != Py_None) {
				self->filter_count = PyList_Size(filters);
				if (self->filter_count) {
					self->cflts = calloc(sizeof(struct command_filter*),self->filter_count);
					self->cflts_size = malloc(self->filter_count*sizeof(size_t));
					if (!parse_command_filters(self->nfsdb,filters,self->cflts,self->cflts_size)) {
						free(self->cflts);
						free(self->cflts_size);
						return 0;
					}
				}
			}
		}

		self->fast_filter = calloc(sizeof(struct command_filter),1);
		if (!parse_commands_fast_filter(self->nfsdb,iter_kwargs,self->fast_filter)) {
			free(self->fast_filter);
			self->fast_filter = 0;
			if (PyErr_Occurred()) {
				free(self->cflts);
				free(self->cflts_size);
				return 0;
			}
		}
		else {
			self->fflts[0] = self->fast_filter;
		}

		/* Set the iterator to the first filtered open(if possible) */
		for (self->command_index=0; self->command_index<self->nfsdb->nfsdb->nfsdb_count; ++self->command_index) {
			struct nfsdb_entry* entry = &self->nfsdb->nfsdb->nfsdb[self->command_index];
			if ((self->fast_filter ? libetrace_nfsdb_filtered_commands_filter_once(self->nfsdb,entry,self->fflts,self->fflts_size,1): true) &&
				(self->cflts_size > 0 ? libetrace_nfsdb_filtered_commands_filter_once(self->nfsdb,entry,self->cflts,self->cflts_size,self->filter_count): true)) {
				/* filter returned true */
				break;
			}
		}
	}

	return (PyObject *)self;
}

Py_ssize_t libetrace_nfsdb_filtered_commands_iter_sq_length(PyObject* self) {

	libetrace_nfsdb_filtered_commands_iter_object* __self = (libetrace_nfsdb_filtered_commands_iter_object*)self;

	unsigned long entry_count = 0;
	for (unsigned long i=0; i<__self->nfsdb->nfsdb->nfsdb_count; ++i) {
		struct nfsdb_entry* entry = &__self->nfsdb->nfsdb->nfsdb[i];
		if ((__self->fast_filter ? libetrace_nfsdb_filtered_commands_filter_once(__self->nfsdb,entry,__self->fflts,__self->fflts_size,1): true) &&
			(__self->cflts_size > 0 ? libetrace_nfsdb_filtered_commands_filter_once(__self->nfsdb,entry,__self->cflts,__self->cflts_size,__self->filter_count): true)) {
			/* filter returned true */
			entry_count++;
		}
	}

    return entry_count;
}

PyObject* libetrace_nfsdb_filtered_commands_iter_next(PyObject *self) {

	libetrace_nfsdb_filtered_commands_iter_object* __self = (libetrace_nfsdb_filtered_commands_iter_object*)self;

	if (__self->command_index>=__self->nfsdb->nfsdb->nfsdb_count) {
		/* Raising of standard StopIteration exception with empty value. */
		PyErr_SetNone(PyExc_StopIteration);
		return 0;
	}

	PyObject* execs = PyList_New(0);
	/* __self->command_index is pointing to the first filtered path */

	do {
		PyObject* args = PyTuple_New(2);
		PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb->nfsdb);
		PYTUPLE_SET_ULONG(args,1,__self->command_index);
		PyObject *entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryType, args);
		Py_DECREF(args);
		PyList_Append(execs,entry);
		Py_DecRef(entry);
		__self->command_index++;
		for (; __self->command_index<__self->nfsdb->nfsdb->nfsdb_count; ++__self->command_index) {
			struct nfsdb_entry* entry = &__self->nfsdb->nfsdb->nfsdb[__self->command_index];
			if ((__self->fast_filter ? libetrace_nfsdb_filtered_commands_filter_once(__self->nfsdb,entry,__self->fflts,__self->fflts_size,1): true) &&
				(__self->cflts_size > 0 ? libetrace_nfsdb_filtered_commands_filter_once(__self->nfsdb,entry,__self->cflts,__self->cflts_size,__self->filter_count): true)) {
				/* filter returned true */
				break;
			}
		}
		if (PyList_Size(execs)>=__self->patch_size) {
			break;
		}
	} while(__self->command_index<__self->nfsdb->nfsdb->nfsdb_count);

	if (__self->patch_size>1) {
		return execs;
	}
	else {
		PyObject* py_command = PyList_GetItem(execs,0);
		Py_XINCREF(py_command);
		Py_DecRef(execs);
		return py_command;
	}
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

	int written = snprintf(repr,128,"<nfsdbEntryEidType object at %lx : ",(uintptr_t)self);
	written += snprintf(repr+written,128,"<%lu",__self->pid);
	if (__self->exeidx<ULONG_MAX) {
		written+=snprintf(repr+written,128-written,":%lu>",__self->exeidx);
	}
	else {
		written+=snprintf(repr+written,128-written,">");
	}

	return PyUnicode_FromString(repr);
}

PyObject* libetrace_nfsdb_entry_eid_richcompare(PyObject *self, PyObject *other, int op) {

	libetrace_nfsdb_entry_eid_object* __self = (libetrace_nfsdb_entry_eid_object*)self;
	libetrace_nfsdb_entry_eid_object* __other = (libetrace_nfsdb_entry_eid_object*)other;

	int cmp = memcmp(&__self->pid,&__other->pid,2*sizeof(unsigned long));

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

PyObject* libetrace_nfsdb_entry_cid_repr(PyObject* self) {

	static char repr[128];

	libetrace_nfsdb_entry_cid_object* __self = (libetrace_nfsdb_entry_cid_object*)self;

	int written = snprintf(repr,128,"<nfsdbEntryCidType object at %lx : ",(uintptr_t)self);
	written += snprintf(repr+written,128,"<%lu:0x%08lx>",__self->pid,__self->flags);

	return PyUnicode_FromString(repr);
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
		self->opaque = PyLong_AsUnsignedLong(PyTuple_GetItem(args,7));
	}

	return (PyObject *)self;
}

PyObject* libetrace_nfsdb_entry_openfile_json(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	PyObject* openEntry = PyDict_New();
	FTDB_SET_ENTRY_STRING(openEntry,p,__self->nfsdb->string_table[__self->path]);
	if (__self->original_path!=ULONG_MAX) {
		FTDB_SET_ENTRY_STRING(openEntry,o,__self->nfsdb->string_table[__self->original_path]);
	}
	FTDB_SET_ENTRY_ULONG(openEntry,m,__self->mode);
	FTDB_SET_ENTRY_ULONG(openEntry,s,__self->size);
	return openEntry;
}

PyObject* libetrace_nfsdb_entry_openfile_repr(PyObject* self) {

	static char repr[1088];

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;
	int written = snprintf(repr,1024,"<nfsdbOpenfileEntry object at %lx : ",(uintptr_t)self);
	size_t path_size = __self->nfsdb->string_size_table[__self->path];
	if (66+path_size-3>=1024-written) {
		snprintf(repr+written,1024-written-64+1,"path:%s",__self->nfsdb->string_table[__self->path]);
		snprintf(repr+1024-64,128,"...|mode:%08lx|size:%010lu|nfsdb:%08lx|index:%08lx>",
			__self->mode,__self->size,__self->parent,__self->index);
	}
	else {
		written+=snprintf(repr+written,1024-written,"path:%s|mode:%08lx|size:%010lu|nfsdb:%08lx|index:%08lx>",
			__self->nfsdb->string_table[__self->path],__self->mode,__self->size,__self->parent,__self->index);
	}
	return PyUnicode_FromString(repr);
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

PyObject* libetrace_nfsdb_entry_openfile_get_opaque(PyObject* self, void* closure) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	if (__self->opaque==ULONG_MAX) {
		Py_RETURN_NONE;
	}

	PyObject* args = PyTuple_New(2);
	PYTUPLE_SET_ULONG(args,0,(uintptr_t)__self->nfsdb);
	PYTUPLE_SET_ULONG(args,1,__self->opaque);
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

PyObject* libetrace_nfsdb_entry_openfile_has_opaque_entry(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	libetrace_nfsdb_entry_openfile_object* __self = (libetrace_nfsdb_entry_openfile_object*)self;

	if (__self->opaque==ULONG_MAX) {
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

static int is_compiled_internal(struct openfile* openfile) {

	return ((openfile->opaque_entry) && (libetrace_nfsdb_entry_has_compilations_internal(openfile->opaque_entry)) &&
		(openfile->opaque_entry->compilation_info->compiled_list[0]==openfile->path));
}

PyObject* libetrace_nfsdb_entry_openfile_is_compiled(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	if (is_compiled_internal(openfile)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

static int is_linked_internal(struct openfile* openfile) {

	return ((openfile->opaque_entry) && (libetrace_nfsdb_entry_is_linking_internal(openfile->opaque_entry)) &&
		(*openfile->opaque_entry->linked_file == openfile->path));
}

PyObject* libetrace_nfsdb_entry_openfile_is_linked(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	if (is_linked_internal(openfile)) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_linked_static(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	if ((openfile->opaque_entry) && (libetrace_nfsdb_entry_is_linking_internal(openfile->opaque_entry)) &&
		(*openfile->opaque_entry->linked_file == openfile->path) && (openfile->opaque_entry->linked_type==0) ) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_linked_shared(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	if ((openfile->opaque_entry) && (libetrace_nfsdb_entry_is_linking_internal(openfile->opaque_entry)) &&
		(*openfile->opaque_entry->linked_file == openfile->path) && (openfile->opaque_entry->linked_type==1) &&
		(libetrace_nfsdb_entry_has_shared_argv(openfile->opaque_entry)) ) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_linked_exe(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	if ((openfile->opaque_entry) && (libetrace_nfsdb_entry_is_linking_internal(openfile->opaque_entry)) &&
		(*openfile->opaque_entry->linked_file == openfile->path) && (openfile->opaque_entry->linked_type==1) &&
		(!libetrace_nfsdb_entry_has_shared_argv(openfile->opaque_entry)) ) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_plain(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];

	/* Not compiled nor linked nor symlink nor binary */
	if ((!is_compiled_internal(openfile)) && (!is_linked_internal(openfile)) && (((openfile->mode&0x40)==0)||((openfile->mode&0x3c)!=0x28)) &&
		(!nfsdb_entryMap_search(&self->nfsdb->bmap,openfile->path))) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_binary(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->bmap,openfile->path);

	if (node) {
		Py_RETURN_TRUE;
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_compiler(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->bmap,openfile->path);

	if (node) {
		struct nfsdb_entry* first_entry = node->entry_list[0];
		if (first_entry->compilation_info) {
			Py_RETURN_TRUE;
		}
	}

	Py_RETURN_FALSE;
}

PyObject* libetrace_nfsdb_entry_openfile_is_linker(libetrace_nfsdb_entry_openfile_object *self, PyObject *args) {

	struct openfile* openfile = &self->nfsdb->nfsdb[self->parent].open_files[self->index];
	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->bmap,openfile->path);

	if (node) {
		struct nfsdb_entry* first_entry = node->entry_list[0];
		if (first_entry->linked_file) {
			Py_RETURN_TRUE;
		}
	}

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

/* Creates the nfsdbOpenFile entry
 * @arguments:
 * nfsdb - main nfsdb object of the database
 * entry - nfsdb_entry object where this file was opened
 * open_index - the index of the opened file within the nfsdb entry
 * nfsdb_index - index of the nfsdb_entry object where this file was opened
 *
 * Returns the nfsdbOpenFile entry object
 */
libetrace_nfsdb_entry_openfile_object* libetrace_nfsdb_create_openfile_entry(const struct nfsdb* nfsdb, const struct nfsdb_entry* entry,
		unsigned long open_index, unsigned long nfsdb_index) {

	PyObject* args = PyTuple_New(8);
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
	if (entry->open_files[open_index].opaque_entry) {
		PYTUPLE_SET_ULONG(args,7,entry->open_files[open_index].opaque_entry->nfsdb_index);
	}
	else {
		PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
	}
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
		if (cindex->__used) {
			libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
					&__self->nfsdb->nfsdb[cindex->nfsdb_index], cindex->open_index, cindex->nfsdb_index);
			PYLIST_ADD_PYOBJECT(compiled,(PyObject*)openfile);
		}
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
		if (cindex->__used) {
			libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
					&__self->nfsdb->nfsdb[cindex->nfsdb_index], cindex->open_index, cindex->nfsdb_index);
			PYLIST_ADD_PYOBJECT(objects,(PyObject*)openfile);
		}
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
		if (cindex->__used) {
			libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(__self->nfsdb,
					&__self->nfsdb->nfsdb[cindex->nfsdb_index], cindex->open_index, cindex->nfsdb_index);
			PYLIST_ADD_PYOBJECT(objects,(PyObject*)openfile);
		}
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

		//TODO: libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(self->nfsdb,entry,index,entry->nfsdb_index);

		PyObject* args = PyTuple_New(8);
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
		if (entry->open_files[index].opaque_entry) {
			PYTUPLE_SET_ULONG(args,7,entry->open_files[index].opaque_entry->nfsdb_index);
		}
		else {
			PYTUPLE_SET_ULONG(args,7,ULONG_MAX);
		}
		PyObject *openfile_entry = PyObject_CallObject((PyObject *) &libetrace_nfsdbEntryOpenfileType, args);
		Py_DECREF(args);
		PYLIST_ADD_PYOBJECT(fill_entries,openfile_entry);
	}
}

/* Retrieves the nfsdb openfile entries for the given 'path' and places them in the 'entryList' */
static int libetrace_nfsdb_filemap_entry_retrieve(libetrace_nfsdb_filemap* self, PyObject* path, PyObject* entryList) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	const char* fpath = PyString_get_c_str(path);
	struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
	if (!srefnode) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid path not present in the database [%s]",fpath);
		PyErr_SetString(libetrace_nfsdbError, errmsg);
		PYASSTR_DECREF(fpath);
		return 0;
	}
	unsigned long hfpath = srefnode->value;
	struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
	if (!node) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid path not present in the database [%s]",fpath);
		PyErr_SetString(libetrace_nfsdbError, errmsg);
		PYASSTR_DECREF(fpath);
		return 0;
	}
	PYASSTR_DECREF(fpath);
	libetrace_nfsdb_fill_entry_list(self,entryList,node->rd_entry_list,node->rd_entry_index,node->rd_entry_count);
	libetrace_nfsdb_fill_entry_list(self,entryList,node->rw_entry_list,node->rw_entry_index,node->rw_entry_count);
	libetrace_nfsdb_fill_entry_list(self,entryList,node->wr_entry_list,node->wr_entry_index,node->wr_entry_count);
	return 1;
}

/* Retrieves the nfsdb openfile entries for the given 'path' with specified 'filter_mode' and fills the 'entryList' */
static int libetrace_nfsdb_filemap_entry_retrieve_with_filter(libetrace_nfsdb_filemap* self, PyObject* path,
		unsigned long filter_mode, PyObject* entryList) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	const char* fpath = PyString_get_c_str(path);
	struct stringRefMap_node* srefnode = stringRefMap_search(&self->nfsdb->revstringmap, fpath);
	if (!srefnode) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid path not present in the database [%s]",fpath);
		PyErr_SetString(libetrace_nfsdbError, errmsg);
		PYASSTR_DECREF(fpath);
		return 0;
	}
	unsigned long hfpath = srefnode->value;
	struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,hfpath);
	if (!node) {
		snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid path not present in the database [%s]",fpath);
		PyErr_SetString(libetrace_nfsdbError, errmsg);
		PYASSTR_DECREF(fpath);
		return 0;
	}
	PYASSTR_DECREF(fpath);
	if (filter_mode==0) {
		libetrace_nfsdb_fill_entry_list(self,entryList,node->rd_entry_list,node->rd_entry_index,node->rd_entry_count);
	}
	else if (filter_mode==1) {
		libetrace_nfsdb_fill_entry_list(self,entryList,node->wr_entry_list,node->wr_entry_index,node->wr_entry_count);
	}
	else {
		libetrace_nfsdb_fill_entry_list(self,entryList,node->rw_entry_list,node->rw_entry_index,node->rw_entry_count);
	}
	return 1;
}

PyObject* libetrace_nfsdb_filemap_mp_subscript(PyObject* self, PyObject* slice) {

	static char errmsg[ERRMSG_BUFFER_SIZE];
	libetrace_nfsdb_filemap* __self = (libetrace_nfsdb_filemap*)self;
	PyObject* rL = PyList_New(0);

	if (PyUnicode_Check(slice)) {
		if (libetrace_nfsdb_filemap_entry_retrieve(__self,slice,rL)) {
			return rL;
		}
	}
	else if (PyTuple_Check(slice)) {
		PyObject* path = PyTuple_GetItem(slice,0);
		if (!PyUnicode_Check(path)) {
			PyErr_SetString(libetrace_nfsdbError,"Invalid filemap argument: path not (str)");
			goto filemap_err;
		}
		if (PyTuple_Size(slice)>1) {
			PyObject* py_filter_mode = PyTuple_GetItem(slice,1);
			if (!PyLong_Check(py_filter_mode)) {
				PyErr_SetString(libetrace_nfsdbError,"Invalid filemap argument: filter_mode not (long)");
				goto filemap_err;
			}
			unsigned long filter_mode = PyLong_AsLong(py_filter_mode);
			if (filter_mode>2) {
				snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid file access mode: [%lu]",filter_mode);
				PyErr_SetString(libetrace_nfsdbError,errmsg);
				goto filemap_err;
			}
			if (libetrace_nfsdb_filemap_entry_retrieve_with_filter(__self,path,filter_mode,rL)) {
				return rL;
			}
		}
		else {
			if (libetrace_nfsdb_filemap_entry_retrieve(__self,path,rL)) {
				return rL;
			}
		}
	}
	else if (PyList_Check(slice)) {
		Py_ssize_t i;
		for (i=0; i<PyList_Size(slice); ++i) {
			PyObject* item = PyList_GetItem(slice,i);
			if (PyUnicode_Check(item)) {
				if (!libetrace_nfsdb_filemap_entry_retrieve(__self,item,rL)) {
					break;
				}
			}
			else if (PyTuple_Check(item)) {
				PyObject* path = PyTuple_GetItem(item,0);
				ASSERT_BREAK_WITH_NFSDB_ERROR(PyUnicode_Check(path),"Invalid filemap argument: path not (str)");
				if (PyTuple_Size(item)>1) {
					PyObject* py_filter_mode = PyTuple_GetItem(item,1);
					ASSERT_BREAK_WITH_NFSDB_ERROR(PyLong_Check(py_filter_mode),"Invalid filemap argument: filter_mode not (long)");
					unsigned long filter_mode = PyLong_AsLong(py_filter_mode);
					ASSERT_BREAK_WITH_NFSDB_FORMAT_ERROR(filter_mode<=2,"Invalid file access mode: [%lu]",filter_mode);
					if (!libetrace_nfsdb_filemap_entry_retrieve_with_filter(__self,path,filter_mode,rL)) {
						break;
					}
				}
				else {
					if (!libetrace_nfsdb_filemap_entry_retrieve(__self,path,rL)) {
						break;
					}
				}
			}
			else {
				PyErr_SetString(libetrace_nfsdbError, "Invalid filemap subscript type (not a string or slice)");
				break;
			}
		}
		if (i>=PyList_Size(slice)) return rL; /* The loop finished normally */
	}
	else {
		PyErr_SetString(libetrace_nfsdbError, "Invalid filemap subscript type (not a string, slice or a list of [string/slice])");
	}

filemap_err:
	Py_DecRef(rL);
	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_contains_path(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (strstr(__self->nfsdb->string_table[data->key], arg) != NULL) {
	    return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_matches_wc(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (!fnmatch(arg,__self->nfsdb->string_table[data->key],0)) {
        return 1;
    }

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_matches_re(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	int match;

	PyObject* args = PyTuple_New(1);
	PyObject* py_path = PyUnicode_FromString(__self->nfsdb->string_table[data->key]);
	PyTuple_SetItem(args,0,py_path);
	PyObject* matchMethod = PyObject_GetAttrString((PyObject*)arg, "match");
	PyObject* matchResult = PyObject_CallObject(matchMethod, args);
	match = (matchResult != Py_None);
	Py_DecRef(matchResult);
	Py_DecRef(matchMethod);
	Py_DecRef(args);
	return match;
}

int libetrace_nfsdb_entry_has_shared_argv(const struct nfsdb_entry * entry) {

	return entry->has_shared_argv;
}

int libetrace_nfsdb_entry_openfile_filter_is_class(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {
	
	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	unsigned long arg_class = (unsigned long)arg;

	if (index==ULONG_MAX) {
		index = 0;
	}

	struct nfsdb_entry* entry = 0;
	unsigned long openfile_index;
	struct openfile* openfile = 0;
	int match = 0;

	if (data->access_type!=FILE_ACCESS_TYPE_EXEC) {
		entry = data->ga_entry_list[index];
		openfile_index = data->ga_entry_index[index];
		openfile = &entry->open_files[openfile_index];
	}

	if (arg_class&(FILE_CLASS_BINARY|FILE_CLASS_COMPILER|FILE_CLASS_LINKER)) {
		struct nfsdb_entryMap_node* node;
		if (data->access_type==FILE_ACCESS_TYPE_EXEC) {
			node = nfsdb_entryMap_search(&__self->nfsdb->bmap,data->key);
		}
		else {
			node = nfsdb_entryMap_search(&__self->nfsdb->bmap,openfile->path);
		}
		if (node) {
			/* This file is a binary */
			if ((arg_class&FILE_CLASS_BINARY)&&(!(arg_class&FILE_CLASS_COMPILER))&&(!(arg_class&FILE_CLASS_LINKER))) match=1;
			else {
				/* !BINARY || (COMPILER || LINKER) */
				struct nfsdb_entry* first_entry = node->entry_list[0];
				if (first_entry->compilation_info) {
					/* This file is a compiler */
					if (arg_class&FILE_CLASS_COMPILER) match=1;
				}
				else if (first_entry->linked_file) {
					/* This file is a linker */
					if (arg_class&FILE_CLASS_LINKER) match=1;
				}
			}
		}
	}

	if (arg_class&0x1f) { /* COMPILED or LINKED */
		if (openfile->opaque_entry) {
			const struct nfsdb_entry* opaque_entry = openfile->opaque_entry;
			if (arg_class&FILE_CLASS_COMPILED) {
				if ((libetrace_nfsdb_entry_has_compilations_internal(opaque_entry)) &&
				(opaque_entry->compilation_info->compiled_list[0]==openfile->path)) match=1;
			}
			else {
				/* We have linked family of filters */
				if (libetrace_nfsdb_entry_is_linking_internal(opaque_entry)) {
					int linked_match = *opaque_entry->linked_file == openfile->path;
					if (arg_class&FILE_CLASS_LINKED) {
						match = linked_match;
					}
					else if (arg_class&FILE_CLASS_LINKED_STATIC) {
						match = linked_match && (opaque_entry->linked_type==0);
					}
					else if (arg_class&FILE_CLASS_LINKED_SHARED) {
						match = linked_match && (opaque_entry->linked_type==1) && libetrace_nfsdb_entry_has_shared_argv(opaque_entry);
					}
					else if (arg_class&FILE_CLASS_LINKED_EXE) {
						match = linked_match && (opaque_entry->linked_type==1) &&(!libetrace_nfsdb_entry_has_shared_argv(opaque_entry));
					}
				}
			}
		}
	}

	if ((arg_class&(FILE_CLASS_BINARY|FILE_CLASS_COMPILER|FILE_CLASS_LINKER)) || (arg_class&0x1f)) {
		/* We're looking for (possibly) symlink and something that's not plain at the same time */
		if (arg_class&FILE_CLASS_SYMLINK) {
			return (match && (((openfile->mode&0x40)!=0)&&((openfile->mode&0x3c)==0x28)));
		}
		if (arg_class&FILE_CLASS_NOSYMLINK) {
			return (match && (((openfile->mode&0x40)!=0)&&((openfile->mode&0x3c)!=0x28)));
		}
		return match;
	}
	else {
		if ((arg_class&FILE_CLASS_SYMLINK)||(arg_class&FILE_CLASS_NOSYMLINK)) {
			/* We're looking for symlink directly */
			if (arg_class&FILE_CLASS_SYMLINK) {
				return (((openfile->mode&0x40)!=0)&&((openfile->mode&0x3c)==0x28));
			}
			if (arg_class&FILE_CLASS_NOSYMLINK) {
				return (((openfile->mode&0x40)!=0)&&((openfile->mode&0x3c)!=0x28));
			}
		}
		else if (arg_class&FILE_CLASS_PLAIN) {
			return ((!is_compiled_internal(openfile)) && (!is_linked_internal(openfile)) && (((openfile->mode&0x40)==0)||((openfile->mode&0x3c)!=0x28)) &&
					(!nfsdb_entryMap_search(&__self->nfsdb->bmap,openfile->path)));
		}
		else {
			/* Nothing specified
			   Pass-through all files */
			return 1;
		}
		return match;
	}
}

int libetrace_nfsdb_entry_openfile_filter_file_exists(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {
	
	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	if (index==ULONG_MAX) {
		index = 0;
	}

	if (data->access_type==FILE_ACCESS_TYPE_EXEC) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->bmap,data->key);
		if (node && node->custom_data) {
			return 1;
		}
	}
	else {
		struct nfsdb_entry* entry = data->ga_entry_list[index];
		unsigned long openfile_index = data->ga_entry_index[index];
		unsigned long mode = entry->open_files[openfile_index].mode;
		if (((mode&0x40)!=0)&&((mode&0x3c)!=0x10)) return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_file_not_exists(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {
	
	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	if (index==ULONG_MAX) {
		index = 0;
	}

	if (data->access_type==FILE_ACCESS_TYPE_EXEC) {
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&__self->nfsdb->bmap,data->key);
		if (node && (node->custom_data==0)) {
			return 1;
		}
	}
	else {
		struct nfsdb_entry* entry = data->ga_entry_list[index];
		unsigned long openfile_index = data->ga_entry_index[index];
		unsigned long mode = entry->open_files[openfile_index].mode;
		if ((mode&0x40)==0) return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_dir_exists(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {
	
	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	if (index==ULONG_MAX) {
		index = 0;
	}

	if (data->access_type!=FILE_ACCESS_TYPE_EXEC) {
		struct nfsdb_entry* entry = data->ga_entry_list[index];
		unsigned long openfile_index = data->ga_entry_index[index];
		unsigned long mode = entry->open_files[openfile_index].mode;
		if (((mode&0x40)!=0)&&((mode&0x3c)==0x10)) return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_has_access(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;
	unsigned long arg_access = (unsigned long)arg;

	if (data->access_type!=FILE_ACCESS_TYPE_EXEC) {

		if (arg_access>2) {
			/* We're looking for global access */
			return (arg_access-3)==data->global_access;
		}
		else {
			/* Per file access search */
			struct nfsdb_entry* entry = data->ga_entry_list[index];
			unsigned long openfile_index = data->ga_entry_index[index];
			unsigned long mode = entry->open_files[openfile_index].mode;
			return arg_access==(mode&0x3);
		}

	}

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_at_source_root(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	const char* path = __self->nfsdb->string_table[data->key];
	size_t path_size = __self->nfsdb->string_size_table[data->key];

	if (path_size<=__self->nfsdb->source_root_size) return 0;
	return !strncmp(path,__self->nfsdb->source_root,__self->nfsdb->source_root_size);
}

int libetrace_nfsdb_entry_openfile_filter_not_at_source_root(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	const char* path = __self->nfsdb->string_table[data->key];
	size_t path_size = __self->nfsdb->string_size_table[data->key];

	if (path_size<=__self->nfsdb->source_root_size) return 1;
	return strncmp(path,__self->nfsdb->source_root,__self->nfsdb->source_root_size);
}

int libetrace_nfsdb_entry_openfile_filter_source_type(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {
	
	unsigned long srctype = (unsigned long)arg;

	if (index==ULONG_MAX) {
		index = 0;
	}

	if (data->access_type==FILE_ACCESS_TYPE_EXEC) {
		return 0;
	}

	struct nfsdb_entry* entry = data->ga_entry_list[index];
	unsigned long openfile_index = data->ga_entry_index[index];
	struct openfile* openfile = &entry->open_files[openfile_index];

	if (openfile->opaque_entry) {
		const struct nfsdb_entry* opaque_entry = openfile->opaque_entry;
		if ((libetrace_nfsdb_entry_has_compilations_internal(opaque_entry)) &&
			(opaque_entry->compilation_info->compiled_list[0]==openfile->path)) {
			/* We have compiled file; let's check its source type */
			if (srctype==FILE_SOURCE_TYPE_C) {
				return opaque_entry->compilation_info->compilation_type==FILE_SOURCE_TYPE_C;
			}
			else if (srctype==FILE_SOURCE_TYPE_CPP) {
				return opaque_entry->compilation_info->compilation_type==FILE_SOURCE_TYPE_CPP;
			}
			else if (srctype==FILE_SOURCE_TYPE_OTHER) {
				return ((opaque_entry->compilation_info->compilation_type!=FILE_SOURCE_TYPE_C)&&
						(opaque_entry->compilation_info->compilation_type!=FILE_SOURCE_TYPE_CPP));
			}
		}
	}

	return 0;
}

int libetrace_nfsdb_entry_openfile_filter_in_path_list(void *self, const struct nfsdb_fileMap_node* data, const void* arg, unsigned long index) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	struct rb_root* path_list = (struct rb_root*)arg;

	struct stringRefMap_node* node = stringRefMap_search(path_list, __self->nfsdb->string_table[data->key]);
	return (node!=0);
}

int libetrace_nfsdb_entry_command_filter_cwd_contains_path(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (strstr(__self->nfsdb->string_table[data->cwd], arg) != NULL) {
	    return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_command_filter_cwd_matches_wc(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (!fnmatch(arg,__self->nfsdb->string_table[data->cwd],0)) {
        return 1;
    }

	return 0;
}

int libetrace_nfsdb_entry_command_filter_cwd_matches_re(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	int match;

	PyObject* args = PyTuple_New(1);
	PyObject* py_str = PyUnicode_FromString(__self->nfsdb->string_table[data->cwd]);
	PyTuple_SetItem(args,0,py_str);
	PyObject* matchMethod = PyObject_GetAttrString((PyObject*)arg, "match");
	PyObject* matchResult = PyObject_CallObject(matchMethod, args);
	match = (matchResult != Py_None);
	Py_DecRef(matchResult);
	Py_DecRef(matchMethod);
	Py_DecRef(args);
	return match;
}

int libetrace_nfsdb_entry_command_filter_cmd_has_string(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	const char* cmdstr = libetrace_nfsdb_string_handle_join(__self->nfsdb,data->argv,data->argv_count," ");
	int match = 0;

	if (strstr(cmdstr, arg) != NULL) {
	    match=1;
	}
	free((void*)cmdstr);

	return match;
}

int libetrace_nfsdb_entry_command_filter_cmd_matches_wc(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	const char* cmdstr = libetrace_nfsdb_string_handle_join(__self->nfsdb,data->argv,data->argv_count," ");
	int match = 0;

	if (!fnmatch(arg,cmdstr,0)) {
	    match=1;
	}
	free((void*)cmdstr);

	return match;
}

int libetrace_nfsdb_entry_command_filter_cmd_matches_re(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	const char* cmdstr = libetrace_nfsdb_string_handle_join(__self->nfsdb,data->argv,data->argv_count," ");
	int match;

	PyObject* args = PyTuple_New(1);
	PyObject* py_str = PyUnicode_FromString(cmdstr);
	PyTuple_SetItem(args,0,py_str);
	PyObject* matchMethod = PyObject_GetAttrString((PyObject*)arg, "match");
	PyObject* matchResult = PyObject_CallObject(matchMethod, args);
	match = (matchResult != Py_None);
	Py_DecRef(matchResult);
	Py_DecRef(matchMethod);
	Py_DecRef(args);
	free((void*)cmdstr);
	return match;
}

int libetrace_nfsdb_entry_command_filter_bin_contains_path(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (strstr(__self->nfsdb->string_table[data->bpath], arg) != NULL) {
	    return 1;
	}

	return 0;
}

int libetrace_nfsdb_entry_command_filter_bin_matches_wc(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;

	if (!fnmatch(arg,__self->nfsdb->string_table[data->bpath],0)) {
        return 1;
    }

	return 0;
}

int libetrace_nfsdb_entry_command_filter_bin_matches_re(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	int match;

	PyObject* args = PyTuple_New(1);
	PyObject* py_str = PyUnicode_FromString(__self->nfsdb->string_table[data->bpath]);
	PyTuple_SetItem(args,0,py_str);
	PyObject* matchMethod = PyObject_GetAttrString((PyObject*)arg, "match");
	PyObject* matchResult = PyObject_CallObject(matchMethod, args);
	match = (matchResult != Py_None);
	Py_DecRef(matchResult);
	Py_DecRef(matchMethod);
	Py_DecRef(args);
	return match;
}

int libetrace_nfsdb_entry_command_filter_has_ppid(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;
	unsigned long arg_ppid = (unsigned long)arg;

	return arg_ppid==data->parent_eid.pid;
}

int libetrace_nfsdb_entry_command_filter_is_class(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;
	unsigned long arg_class = (unsigned long)arg;
	int match = 0;

	if (arg_class&FILE_CLASS_COMMAND) {
		if ((data->bpath!=LIBETRACE_EMPTY_STRING_HANDLE)&&(data->argv_count>0)) {
			match=1;
		}
	}

	if (arg_class&FILE_CLASS_COMPILING) {
		if (libetrace_nfsdb_entry_has_compilations_internal(data)) {
			match=1;
		}
	}

	if (arg_class&FILE_CLASS_LINKING) {
		if (libetrace_nfsdb_entry_is_linking_internal(data)) {
			match=1;
		}
	}

	return match;
}

int libetrace_nfsdb_entry_command_filter_bin_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	const char* s = __self->nfsdb->string_table[data->bpath];
	size_t path_size = __self->nfsdb->string_size_table[data->bpath];

	if (path_size<=__self->nfsdb->source_root_size) return 0;
	return !strncmp(s,__self->nfsdb->source_root,__self->nfsdb->source_root_size);
}

int libetrace_nfsdb_entry_command_filter_bin_not_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	const char* s = __self->nfsdb->string_table[data->bpath];
	size_t path_size = __self->nfsdb->string_size_table[data->bpath];

	if (path_size<=__self->nfsdb->source_root_size) return 1;
	return strncmp(s,__self->nfsdb->source_root,__self->nfsdb->source_root_size);
}

int libetrace_nfsdb_entry_command_filter_cwd_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	const char* s = __self->nfsdb->string_table[data->cwd];
	size_t path_size = __self->nfsdb->string_size_table[data->cwd];

	if (path_size<=__self->nfsdb->source_root_size) return 0;
	return !strncmp(s,__self->nfsdb->source_root,__self->nfsdb->source_root_size);
}

int libetrace_nfsdb_entry_command_filter_cwd_not_at_source_root(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	(void)__self;

	const char* s = __self->nfsdb->string_table[data->cwd];
	size_t path_size = __self->nfsdb->string_size_table[data->cwd];

	if (path_size<=__self->nfsdb->source_root_size) return 1;
	return strncmp(s,__self->nfsdb->source_root,__self->nfsdb->source_root_size);
}

int libetrace_nfsdb_entry_command_filter_in_binaries_list(void *self, const struct nfsdb_entry* data, const void* arg) {

	libetrace_nfsdb_object* __self = (libetrace_nfsdb_object*)self;
	struct rb_root* bins_list = (struct rb_root*)arg;

	struct stringRefMap_node* node = stringRefMap_search(bins_list, __self->nfsdb->string_table[data->bpath]);
	return (node!=0);
}

int libetrace_nfsdb_entry_command_filter_in_pids_list(void *self, const struct nfsdb_entry* data, const void* arg) {

	struct rb_root* pids_list = (struct rb_root*)arg;

	struct ulong_entryMap_node* node = ulong_entryMap_search(pids_list, data->eid.pid);
	return (node!=0);
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

    if (PyType_Ready(&libetrace_nfsdbOpensIterType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbOpensIterType);

	if (PyType_Ready(&libetrace_nfsdbFilteredOpensPathsIterType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbFilteredOpensPathsIterType);

	if (PyType_Ready(&libetrace_nfsdbFilteredOpensIterType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbFilteredOpensIterType);

	if (PyType_Ready(&libetrace_nfsdbFilteredCommandsIterType) < 0)
		return 0;
	Py_XINCREF(&libetrace_nfsdbFilteredCommandsIterType);

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

	if (PyModule_AddObject(m, "nfsdbIter", (PyObject *)&libetrace_nfsdbIterType)<0) {
		Py_DECREF(m);
		return 0;
	}

	if (PyModule_AddObject(m, "nfsdbOpensIter", (PyObject *)&libetrace_nfsdbOpensIterType)<0) {
		Py_DECREF(m);
		return 0;
	}

	if (PyModule_AddObject(m, "nfsdbFilteredOpensPathsIter", (PyObject *)&libetrace_nfsdbFilteredOpensPathsIterType)<0) {
		Py_DECREF(m);
		return 0;
	}

	if (PyModule_AddObject(m, "nfsdbFilteredOpensIter", (PyObject *)&libetrace_nfsdbFilteredOpensIterType)<0) {
		Py_DECREF(m);
		return 0;
	}

	if (PyModule_AddObject(m, "nfsdbFilteredCommandsIter", (PyObject *)&libetrace_nfsdbFilteredCommandsIterType)<0) {
		Py_DECREF(m);
		return 0;
	}

	if (PyModule_AddObject(m, "nfsdbEntryCid", (PyObject *)&libetrace_nfsdbEntryCidType)<0) {
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

	if (PyModule_AddObject(m, "nfsdbEntryCompilationInfo", (PyObject *)&libetrace_nfsdbEntryCompilationInfoType)<0) {
		Py_DECREF(m);
		return 0;
	}

	if (PyModule_AddObject(m, "nfsdbFileMap", (PyObject *)&libetrace_nfsdbFileMapType)<0) {
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
