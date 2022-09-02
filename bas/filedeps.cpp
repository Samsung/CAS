extern "C" {
#include "pyetrace.h"
}
#include <iostream>
#include <unordered_set>
#include <deque>
#include <set>
#include <map>
#include <vector>

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

static inline int SET_MSB_INT(int i) {
	return i|(1<<(CHAR_BIT*sizeof(int)-1));
}

static inline int CLEAR_MSB_INT(int i) {
	return i&(~(1<<(CHAR_BIT*sizeof(int)-1)));
}

static inline int MSB_SET_INT(int i) {
	return (i&(1<<(CHAR_BIT*sizeof(int)-1)))!=0;
}

static inline int SET_MSB_UPID(upid_t i) {
	return i|(((upid_t)1)<<(CHAR_BIT*sizeof(upid_t)-1));
}

static inline int CLEAR_MSB_UPID(upid_t i) {
	return i&(~(((upid_t)1)<<(CHAR_BIT*sizeof(upid_t)-1)));
}

static inline int MSB_IS_SET_UPID(upid_t i) {
	return (i&(((upid_t)1)<<(CHAR_BIT*sizeof(upid_t)-1)))!=0;
}

int g_timer = 0;
void timer_expired(int sig) {
	g_timer = 1;
}

static int pattern_match(const char* p, const char** excl_patterns, size_t excl_patterns_size) {

    size_t u;
    for (u=0; u<excl_patterns_size; ++u) {
        if (!fnmatch(excl_patterns[u],p,0)) {
            return 1;
        }
    }

    return 0;
}

struct libetrace_nfsdb_entry_openfile_object_less {
    bool operator()(const libetrace_nfsdb_entry_openfile_object * lhs, const libetrace_nfsdb_entry_openfile_object * rhs) const {
    	return memcmp(&lhs->parent,&rhs->parent,2*sizeof(unsigned long))<0;
    }
};

struct depproc_context {
	std::set<unsigned long> excl_set;					/* 'exclude_files' parameter */
	const char** excl_patterns;							/* 'exclude_patterns' parameter */	/* ALLOC */
	size_t excl_patterns_size;							/* (...) */
	const char** excl_commands;							/* 'exclude_commands' parameter */	/* ALLOC */
	size_t excl_commands_size;							/* (...) */
	long* excl_commands_index;							/* 'exclude_commands_index' parameter */	/* ALLOC */
	size_t excl_commands_index_size;					/* (...) */
	int direct_deps;									/* 'direct' parameter */
	int debug;											/* 'debug' parameter */
	int fd_debug;										/* 'debug_fd' parameter */
	int dry_run;										/* 'dry_run' parameter */
	int negate_pattern;									/* 'negate_pattern' parameter */
	int dep_graph;										/* 'dep_graph' parameter */
	int wrap_deps;										/* 'wrap_deps' parameter */
	int timeout;										/* 'timeout' paramater */
	int use_pipes;										/* 'use_pipes' parameter */	/* Default: 1 */
	std::set<unsigned long> all_modules_set;			/* 'all_modules' parameter */
	timer_t timer_id;
	std::deque<upid_t> qpid;
	std::set<upid_t> writing_process_list;
	std::set<upid_t> all_writing_process_list;
	std::set<libetrace_nfsdb_entry_openfile_object*,libetrace_nfsdb_entry_openfile_object_less> openfile_deps;
	std::deque<unsigned long> files;
	std::unordered_set<unsigned long> files_set;
	std::unordered_set<unsigned long> fdone;
	depproc_context():
		excl_patterns(0), excl_patterns_size(0), excl_commands(0), excl_commands_size(0),
		excl_commands_index(0), excl_commands_index_size(0), direct_deps(0), debug(0), fd_debug(0),
		dry_run(0), negate_pattern(0), dep_graph(0), wrap_deps(0), timeout(0), use_pipes(1), timer_id(0) {}
};

static volatile int interrupt = 0;
static void intHandler(int v) {
    interrupt = 1;
}

static void depproc_context_free(struct depproc_context* context) {

	for (unsigned long i=0; i<context->excl_patterns_size; ++i) {
		PYASSTR_DECREF(context->excl_patterns[i]);
	}
	free(context->excl_patterns);
	for (unsigned long i=0; i<context->excl_commands_size; ++i) {
		PYASSTR_DECREF(context->excl_commands[i]);
	}
	free(context->excl_commands);
	free(context->excl_commands_index);
	for (auto u = context->openfile_deps.begin(); u!=context->openfile_deps.end(); ++u) {
		Py_DecRef((PyObject*)*u);
	}
}

static int phs_find(std::vector<std::pair<unsigned long,const char*>>& phs, unsigned long phandle) {

	for (std::vector<std::pair<unsigned long,const char*>>::iterator i=phs.begin(); i!=phs.end(); ++i) {
		if ((*i).first==phandle) return 1;
	}

	return 0;
}

static void phs_free(std::vector<std::pair<unsigned long,const char*>>& phs) {

	for (std::vector<std::pair<unsigned long,const char*>>::iterator i=phs.begin(); i<phs.end(); ++i) {
		PYASSTR_DECREF((*i).second);
	}
}

static int depproc_parse_args(libetrace_nfsdb_object* self,std::vector<std::pair<unsigned long,const char*>>& phs,
		PyObject* kwargs, struct depproc_context* context) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (kwargs) {
		PyObject* debug = PyDict_GetItemString(kwargs, "debug");
		if (debug) {
			context->debug = PyObject_IsTrue(debug);
			if (!context->debug) {
				context->debug = self->debug;
			}
		}
		DBG(context->debug,"        debug=%s\n",context->debug?"true":"false");

		PyObject* exclude_files = PyDict_GetItemString(kwargs, "exclude_files");
		DBG(context->debug,"    exclude files count: %ld\n",exclude_files?PyList_Size(exclude_files):0);
		if (exclude_files) {
			int u;
			for (u=0; u<PyList_Size(exclude_files); ++u) {
				const char* exclude_file = PyString_get_c_str(PyList_GetItem(exclude_files,u));
				struct stringRefMap_node* efnode = stringRefMap_search(&self->nfsdb->revstringmap,exclude_file);
				if (!efnode) {
					snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid exclude file key [%s]",exclude_file);
					PyErr_SetString(libetrace_nfsdbError, errmsg);
					PYASSTR_DECREF(exclude_file);
					return 0;
				}
				context->excl_set.insert(efnode->value);
				DBG(context->debug,"        exclude file: %s\n",exclude_file);
				PYASSTR_DECREF(exclude_file);
			}
		}

		PyObject* exclude_patterns = PyDict_GetItemString(kwargs, "exclude_patterns");
		DBG(context->debug,"    exclude patterns count: %ld\n",exclude_patterns?PyList_Size(exclude_patterns):0);
		if (exclude_patterns && (PyList_Size(exclude_patterns)>0)) {
			context->excl_patterns = (const char**)malloc(PyList_Size(exclude_patterns)*sizeof(const char*));
			context->excl_patterns_size = PyList_Size(exclude_patterns);
			int u;
			for (u=0; u<PyList_Size(exclude_patterns); ++u) {
				context->excl_patterns[u] = PyString_get_c_str(PyList_GetItem(exclude_patterns,u));
				DBG(context->debug,"        exclude pattern: %s\n",context->excl_patterns[u]);
			}
		}

		PyObject* exclude_commands = PyDict_GetItemString(kwargs, "exclude_commands");
		DBG(context->debug,"    exclude commands count: %ld\n",exclude_commands?PyList_Size(exclude_commands):0);
		if (exclude_commands && (PyList_Size(exclude_commands)>0)) {
			context->excl_commands = (const char**)malloc(PyList_Size(exclude_commands)*sizeof(const char*));
			context->excl_commands_size = PyList_Size(exclude_commands);
			int u;
			for (u=0; u<PyList_Size(exclude_commands); ++u) {
				context->excl_commands[u] = PyString_get_c_str(PyList_GetItem(exclude_commands,u));
				DBG(context->debug,"        exclude command pattern: %s\n",context->excl_commands[u]);
			}
		}

		PyObject* exclude_commands_index = PyDict_GetItemString(kwargs, "exclude_commands_index");
		DBG(context->debug,"    exclude commands index count: %ld\n",exclude_commands_index?PyList_Size(exclude_commands_index):0);
		if (exclude_commands_index && (PyList_Size(exclude_commands_index)>0)) {
			context->excl_commands_index = (long*)malloc(PyList_Size(exclude_commands_index)*sizeof(long));
			context->excl_commands_index_size = PyList_Size(exclude_commands_index);
			int u;
			for (u=0; u<PyList_Size(exclude_commands_index); ++u) {
				context->excl_commands_index[u] = PyLong_AsLong(PyList_GetItem(exclude_commands_index,u));
				DBG(context->debug,"        exclude command pattern index: %ld\n",context->excl_commands_index[u]);
			}
		}

		PyObject* debug_fd = PyDict_GetItemString(kwargs, "debug_fd");
		if (debug_fd) {
			context->fd_debug = PyObject_IsTrue(debug_fd);
		}
		DBG(context->debug,"        debug_fd=%s\n",context->fd_debug?"true":"false");

		PyObject* dry_run = PyDict_GetItemString(kwargs, "dry_run");
		if (dry_run) {
			context->dry_run = PyObject_IsTrue(dry_run);
		}
		DBG(context->debug,"        dry_run=%s\n",context->dry_run?"true":"false");

		PyObject* neg_pattern = PyDict_GetItemString(kwargs, "negate_pattern");
		if (neg_pattern) {
			context->negate_pattern = PyObject_IsTrue(neg_pattern) ? 1 : 0;
		}
		DBG(context->debug,"        negate_pattern=%s\n",context->negate_pattern?"true":"false");

		PyObject* dep_graph_kwarg = PyDict_GetItemString(kwargs, "dep_graph");
		if (dep_graph_kwarg) {
			/* When dep_graph_kwarg option is set to True then apart from pids and dependency list
			 * also additional dictionary that contains all detailed dependencies and commands is returned
			 */
			context->dep_graph = PyObject_IsTrue(dep_graph_kwarg) ? 1 : 0;
		}
		DBG(context->debug,"        dep_graph=%s\n",context->dep_graph?"true":"false");

		PyObject* wrapping_deps = PyDict_GetItemString(kwargs, "wrap_deps");
		if (wrapping_deps) {
			context->wrap_deps = PyObject_IsTrue(wrapping_deps) ? 1 : 0;
		}
		DBG(context->debug,"        wrap_deps=%s\n",context->wrap_deps?"true":"false");

		PyObject* use_pipes = PyDict_GetItemString(kwargs, "use_pipes");
		if (use_pipes) {
			context->use_pipes = PyObject_IsTrue(use_pipes) ? 1 : 0;
		}
		else {
			context->use_pipes = 1;
		}
		DBG(context->debug,"        use_pipes=%s\n",context->use_pipes?"true":"false");

		PyObject* func_timeout = PyDict_GetItemString(kwargs, "timeout");
		if (func_timeout) {
			context->timeout = PyLong_AsLong(func_timeout);
		}
		DBG(context->debug,"        timeout=%d\n",context->timeout);

		PyObject* all_modules = PyDict_GetItemString(kwargs, "all_modules");
		DBG(context->debug,"    all_modules count: %ld\n",all_modules?PyList_Size(all_modules):0);
		if (all_modules) {
			int u;
			for (u=0; u<PyList_Size(all_modules); ++u) {
				const char* module_name = PyString_get_c_str(PyList_GetItem(all_modules,u));
				struct stringRefMap_node* mfnode = stringRefMap_search(&self->nfsdb->revstringmap,module_name);
				if (!mfnode) {
					snprintf(errmsg,ERRMSG_BUFFER_SIZE,"Invalid module name key [%s]",module_name);
					PyErr_SetString(libetrace_nfsdbError, errmsg);
					PYASSTR_DECREF(module_name);
					return 0;
				}
				context->all_modules_set.insert(mfnode->value);
				DBG(context->debug,"        module: %s\n",module_name);
				PYASSTR_DECREF(module_name);
			}
			for (std::vector<std::pair<unsigned long,const char*>>::iterator pi = phs.begin(); pi!=phs.end(); ++pi) {
				if (context->all_modules_set.find((*pi).first)!=context->all_modules_set.end()) {
					context->all_modules_set.erase((*pi).first);
				}
			}
		}

		PyObject* direct = PyDict_GetItemString(kwargs, "direct");
		if (direct) {
			context->direct_deps = PyObject_IsTrue(direct);
		}
		DBG(context->debug,"        direct=%s\n",context->direct_deps?"true":"false");
		if (!all_modules) {
			/* Fill the all modules set with all linked modules paths */
			struct rb_node * p = rb_first(&self->nfsdb->linkedmap);
			while(p) {
				struct nfsdb_entryMap_node* data = (struct nfsdb_entryMap_node*)p;
				if (!phs_find(phs, data->key)) {
					context->all_modules_set.insert(data->key);
				}
				p = rb_next(p);
			}
		}
	}

	return 1;
}

static int depproc_init_timer(struct depproc_context* context) {

	struct sigaction act;
	clockid_t clock_id;
	struct sigevent clock_sig_event;
	struct itimerspec timer_value;
	int ret;

	if (context->timeout>0) {

		/* Register new action for SIGUSR1 */
		memset(&act, 0, sizeof(struct sigaction));
		act.sa_handler =  timer_expired;
		ret = sigaction(SIGUSR1, &act, NULL);
		assert(ret == 0);

		clock_id = CLOCK_MONOTONIC;
		memset(&clock_sig_event, 0, sizeof( struct sigevent));
		clock_sig_event.sigev_notify = SIGEV_SIGNAL;
		clock_sig_event.sigev_signo = SIGUSR1;
		clock_sig_event.sigev_notify_attributes = NULL;
		/* Creating process interval timer */
		ret = timer_create(clock_id, &clock_sig_event, &context->timer_id);
		assert(ret == 0);

		/* setitng timer interval values */
		memset(&timer_value, 0, sizeof(struct itimerspec));
		timer_value.it_interval.tv_sec = 0;
		timer_value.it_interval.tv_nsec = 0;

		/* setting timer initial expiration values*/
		timer_value.it_value.tv_sec = context->timeout;
		timer_value.it_value.tv_nsec = 0;

		/* Create timer */
		ret = timer_settime(context->timer_id, 0, &timer_value, NULL);
		return ret;

		/* Now we have a timer with following features:
		 * It will expire after 'timeout' seconds and execute function timer_expired
		 * upon expiration */
	}

	return 0;
}

static const char* nfsdb_entry_command_string(libetrace_nfsdb_object* self, const struct nfsdb_entry* entry) {

	if (entry->argv_count<=0) {
		return (const char*)calloc(1,1);
	}

	size_t newsize = entry->argv_count;
	for (unsigned long u=0; u<entry->argv_count; ++u) {
		newsize+=self->nfsdb->string_size_table[entry->argv[u]];
	}

	char* s = (char*)malloc(newsize), *p = s;
	for (unsigned long u=0; u<entry->argv_count; ++u) {
		const char* v = self->nfsdb->string_table[entry->argv[u]];
		size_t sz = self->nfsdb->string_size_table[entry->argv[u]];
		memcpy((void*)p,v,sz);
		p[sz] = ' ';
		p+=sz+1;
	}
	*(p-1) = 0;

	return s;
}

/* Verifies whether any execution in the process 'pid' matches any precomputed predefined commands index
 * When it matches it returns the matching index, otherwise return -1 */
static long depproc_commands_index_match(libetrace_nfsdb_object* self, struct depproc_context* context, upid_t pid) {

	if (context->excl_commands_index_size<=0) {
		return -1;
	}

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			if (entry->argv_count>0) {
				if (entry->pcp) {
					size_t w;
					for (w=0; w<context->excl_commands_index_size; ++w) {
						unsigned pattern_index = context->excl_commands_index[w];
						unsigned byte_index = pattern_index/8;
						unsigned bit_index = pattern_index%8;
						if (entry->pcp[byte_index]&(0x01<<(7-bit_index))) {
							return w;
						}
					}
				}
				else {
					if (context->fd_debug) {
						DBG(context->fd_debug,"*** Could not find pcp entry in pid (" GENERIC_ARG_PID_FMT ") cmd (%zu)\n",pid,u);
						const char* cmd = nfsdb_entry_command_string(self,entry);
						DBG(context->fd_debug,"$ %s\n",cmd);
						free((void*)cmd);
					}
				}
			}
		}
	}

	return -1;
}

/* Verifies whether any execution in the process 'pid' matches any predefined commands
 * When it matches it returns the execution index which matches, otherwise return -1 */
static long depproc_commands_match(libetrace_nfsdb_object* self, struct depproc_context* context, upid_t pid) {

	if (context->excl_commands_size<=0) {
		return -1;
	}

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			if (entry->argv_count>0) {
				const char* cmd = nfsdb_entry_command_string(self,entry);
				if (pattern_match(cmd,context->excl_commands,context->excl_commands_size)!=context->negate_pattern) {
					free((void*)cmd);
					return u;
				}
				free((void*)cmd);
			}
		}
	}

	return -1;
}

static void log_write_process_commands(libetrace_nfsdb_object* self, struct depproc_context* context, upid_t pid, const char* indent) {

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		int cmd_print_count = 0;
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			if (entry->argv_count>0) {
				const char* cmd = nfsdb_entry_command_string(self,entry);
				DBG(context->fd_debug,"%s$ %s\n",indent,cmd);
				cmd_print_count++;
				free((void*)cmd);
			}
		}
		if (cmd_print_count==0) {
			DBG(context->fd_debug,"%s$ <NONE>\n",indent);
		}
	}
	else {
		DBG(context->fd_debug,"%s$ <NONE>\n",indent);
	}
}

/* Returns the number of new processes marked as writing processes for further processing
  * Returns -1 when interrupted */
static long depproc_process_qpid(libetrace_nfsdb_object* self, struct depproc_context* context,
		std::map<upid_t,std::string>& writing_pid_map, std::string& f) {

	long processed = 0;

	while(!context->qpid.empty()) {
		upid_t wpid = context->qpid.front();
		context->qpid.pop_front();
		if (!context->use_pipes) continue;
		DBG(context->fd_debug,"     checking pipe_map for " GENERIC_ARG_PID_FMT,wpid);
		int pipe_entry_count = 0;
		struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,wpid);
		if (node) {
			for (unsigned long u=0; u<node->entry_count; ++u) {
				struct nfsdb_entry* entry = node->entry_list[u];
				for (unsigned long i=0; i<entry->pipe_eids_count; ++i) {
					upid_t npid = entry->pipe_eids[i].pid;
					upid_t wnpid = npid;
					DBG(context->fd_debug,"       pipe write to " GENERIC_ARG_PID_FMT,npid);
					pipe_entry_count++;
					if (context->wrap_deps) {
						unsigned long wrapper_pid = ULONG_MAX;
						struct nfsdb_entryMap_node* nnode = nfsdb_entryMap_search(&self->nfsdb->procmap,npid);
						if (nnode) {
							for (unsigned long v=0; v<nnode->entry_count; ++v) {
								struct nfsdb_entry* nentry = nnode->entry_list[v];
								if (nentry->wrapper_pid!=ULONG_MAX) {
									wrapper_pid = nentry->wrapper_pid;
									break;
								}
							}
						}
						if (wrapper_pid!=ULONG_MAX) {
							DBG(context->fd_debug," (wrapped: " GENERIC_ARG_PID_FMT ")",(upid_t)wrapper_pid);
							wnpid = SET_MSB_UPID((upid_t)wrapper_pid);
						}
					}
					if (context->writing_process_list.find(wnpid)==context->writing_process_list.end()) {
						DBG(context->fd_debug," : new writing process\n");
						context->qpid.push_back(wnpid);
						unsigned match = 0;
						if (context->excl_commands_index) {
							match = depproc_commands_index_match(self,context,wnpid);
						}
						else {
							match = depproc_commands_match(self,context,wnpid);
						}
						if (match) {
							continue;
						}
						if (context->writing_process_list.insert(wnpid).second) {
							DBG(context->fd_debug,"     added process to consider: " GENERIC_ARG_PID_FMT "\n",wnpid);
							processed++;
							if (context->fd_debug) {
								log_write_process_commands(self,context,wnpid,"       ");
							}
						}
						if (context->dep_graph) {
							writing_pid_map.insert(std::pair<upid_t,std::string>(wnpid,f));
						}
						context->all_writing_process_list.insert(npid);
					}
					else {
						DBG(context->fd_debug," : already considered\n");
					}
				}
			}
			if (pipe_entry_count<=0) {
				DBG(context->fd_debug," : no pipe entries\n");
			}
		}
		if (g_timer||interrupt) {
			return -1;
		}
	} /* while(qpid) */

	return processed;
}

/* Returns the number of new processes marked as writing processes for further consideration
 * Returns -1 when interrupted */
static long depproc_process_written_file(libetrace_nfsdb_object* self, struct depproc_context* context,
		std::map<upid_t,std::string>& writing_pid_map, unsigned long fh) {

	long processed = 0;
	static char errmsg[ERRMSG_BUFFER_SIZE];

	struct nfsdb_fileMap_node* fnode = fileMap_search(&self->nfsdb->filemap,fh);
	ASSERT_RETURN_WITH_NFSDB_FORMAT_ERROR(fnode,"Internal nfsdb error at binary path handle [%lu]",-1,fh);

	if ((fnode->wr_entry_count<=0) && (fnode->rw_entry_count<=0)) {
		DBG(context->fd_debug,"   missing entry in rev_wr_map\n");
		return processed;
	}

	/* Here we get a list of processes that have written to the 'fh' file */
	DBG(context->fd_debug,"   writing process count: %ld\n",fnode->wr_entry_count+fnode->rw_entry_count);
	/* Process the pids in sorted order (to simplify potential debugging) */
	unsigned long wri=0, rwi=0;
	upid_t lastpid = 0;
	while(1) {
		if ((wri>=fnode->wr_entry_count) && (rwi>=fnode->rw_entry_count)) break;
		upid_t writing_pid;
		struct nfsdb_entry* writing_entry;
		unsigned long writing_open_index;
		if ((wri<fnode->wr_entry_count) && (rwi>=fnode->rw_entry_count)) {
			writing_entry = fnode->wr_entry_list[wri];
			writing_pid = fnode->wr_entry_list[wri]->eid.pid;
			writing_open_index = fnode->wr_entry_index[wri];
			wri++;
		}
		else if ((wri>=fnode->wr_entry_count) && (rwi<fnode->rw_entry_count)) {
			writing_entry = fnode->rw_entry_list[rwi];
			writing_pid = fnode->rw_entry_list[rwi]->eid.pid;
			writing_open_index = fnode->rw_entry_index[rwi];
			rwi++;
		}
		else {
			if (fnode->wr_entry_list[wri]->eid.pid<fnode->rw_entry_list[rwi]->eid.pid) {
				writing_entry = fnode->wr_entry_list[wri];
				writing_pid = fnode->wr_entry_list[wri]->eid.pid;
				writing_open_index = fnode->wr_entry_index[wri];
				wri++;
			}
			else {
				writing_entry = fnode->rw_entry_list[rwi];
				writing_pid = fnode->rw_entry_list[rwi]->eid.pid;
				writing_open_index = fnode->rw_entry_index[rwi];
				rwi++;
			}
		}
		if (writing_pid==lastpid) continue;
		lastpid=writing_pid;
		/* Handle executions by 'pid' */
		upid_t wrapping_pid = writing_pid;
		DBG(context->fd_debug,"     writing pid: " GENERIC_ARG_PID_FMT,writing_pid);
		if (context->wrap_deps) {
			/* Sometimes executed processes are intertwined together, for example:
			 * /bin/bash -c "cat <...> | sort > out.f"
			 * Here we cannot do much with the "sort" command alone which writes to the out.f file,
			 * therefore instead of considering the sort process we want to consider the wrapping /bin/bash call
			 * We do that by first creating reverse binary mapping for /bin/bash command which maps all descendant
			 * processes back to their wrapping /bin/bash execution and then act as the real writing process was the
			 * wrapping /bin/bash
			 */
			struct nfsdb_entryMap_node* pnode = nfsdb_entryMap_search(&self->nfsdb->procmap,writing_pid);
			if (pnode) {
				for (unsigned long u=0; u<pnode->entry_count; ++u) {
					struct nfsdb_entry* entry = pnode->entry_list[u];
					if (entry->wrapper_pid!=ULONG_MAX) {
						/* Tell the read dependency processor which runs later that this was the wrapping pid,
						 * not the original pid by setting the MSB bit */
						DBG(context->fd_debug,", wrapping pid: " GENERIC_ARG_PID_FMT, entry->eid.pid);
						wrapping_pid = SET_MSB_UPID(entry->eid.pid);
						break;
					}
				}
			}
		}
		DBG(context->fd_debug,"\n");
		long match=0;
		if (context->excl_commands_index) {
			/* Matching large string (which represents command) with a pattern is quite expensive and doing so
			 *  in the main loop of dependency processing blows up the performance entirely. To solve this
			 *  all commands are matched with all predefined patterns in advanced and stored as a bit of information
			 *  (matches or not). Therefore the pattern matching for predefined pattern is just a reading a bit from memory.
			 */
			if ((match = depproc_commands_index_match(self,context,wrapping_pid))>=0) {
				DBG(context->fd_debug,"     command pattern index match at index: %ld (skipping...)\n",match);
			}
		}
		else {
			if ((match = depproc_commands_match(self,context,wrapping_pid))>=0) {
				if (match>=0) DBG(context->fd_debug,"     command pattern match at index: %ld (skipping...)\n",match);
			}
		}
		if (match>=0) {
			continue;
		}
		if (context->writing_process_list.insert(wrapping_pid).second) {
			DBG(context->fd_debug,"     added process to consider: " GENERIC_ARG_PID_FMT "\n",wrapping_pid);
			processed++;
			if (context->fd_debug) {
				log_write_process_commands(self,context,wrapping_pid,"     ");
			}
		}
		if (context->dep_graph) {
			writing_pid_map[wrapping_pid] = self->nfsdb->string_table[fh];
		}
		context->all_writing_process_list.insert(writing_pid);
		libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(
				self->nfsdb,writing_entry,writing_open_index,writing_entry->nfsdb_index);
		if (!context->openfile_deps.insert(openfile).second) {
			Py_DecRef((PyObject*)openfile);
		}
		context->qpid.push_back(wrapping_pid);
		if (g_timer||interrupt) return -1;
		std::string f = self->nfsdb->string_table[fh];
		long qprocs = depproc_process_qpid(self,context,writing_pid_map,f);
		if (qprocs<0) return -1;
		processed+=qprocs;
	}

	return processed;
}

static void process_read_open_files_unique_with_children(libetrace_nfsdb_object* self, upid_t pid,
		std::set<unsigned long> hfiles) {

	DBG(self->debug,"--- process_read_open_files_unique_with_children_nocpy(" GENERIC_ARG_PID_FMT ")\n",pid);

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			for (unsigned long i=0; i<entry->open_files_count; ++i) {
				if ((entry->open_files[i].mode&0x01)==0) {
					hfiles.insert(entry->open_files[i].path);
				}
			}
			for (unsigned long i=0; i<entry->child_ids_count; ++i) {
				process_read_open_files_unique_with_children(self,entry->child_ids[i].pid,hfiles);
			}
		}
	}
}

static void process_write_open_files_unique_with_children(libetrace_nfsdb_object* self, upid_t pid,
		std::set<unsigned long> hfiles) {

	DBG(self->debug,"--- process_write_open_files_unique_with_children_nocpy(" GENERIC_ARG_PID_FMT ")\n",pid);

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			for (unsigned long i=0; i<entry->open_files_count; ++i) {
				if (entry->open_files[i].mode>0) {
					hfiles.insert(entry->open_files[i].path);
				}
			}
			for (unsigned long i=0; i<entry->child_ids_count; ++i) {
				process_write_open_files_unique_with_children(self,entry->child_ids[i].pid,hfiles);
			}
		}
	}
}

static long get_wrapping_process_descendants_read_files(libetrace_nfsdb_object* self, struct depproc_context* context,
		upid_t pid, std::vector<unsigned long>& dep_flist) {

	long rcount = 0;

	std::set<unsigned long> rdfiles;
	process_read_open_files_unique_with_children(self,pid,rdfiles);
	if (context->fd_debug) {
		// TODO: sort rdfiles by the real path for debug purposes
	}
	std::set<unsigned long> wrfiles;
	process_write_open_files_unique_with_children(self,pid,wrfiles);
	for (decltype(rdfiles)::iterator i=rdfiles.begin(); i!=rdfiles.end(); ++i) {
		unsigned long rdh = (*i);
		if (wrfiles.find(rdh)!=wrfiles.end()) {
			continue;
		}
		if (context->dep_graph) {
			dep_flist.push_back(rdh);
		}
		if ((context->files_set.find(rdh)==context->files_set.end()) && (context->fdone.find(rdh)==context->fdone.end())) {
			if ((context->excl_set.find(rdh)==context->excl_set.end()) &&
					(pattern_match(self->nfsdb->string_table[rdh],
							context->excl_patterns,context->excl_patterns_size)==context->negate_pattern)) {
				context->files.push_back(rdh);
				context->files_set.insert(rdh);
				DBG(context->fd_debug,"     added read file: %s\n",self->nfsdb->string_table[rdh]);
				rcount++;
			}
			else {
				context->fdone.insert(rdh);
			}
		}
	}

	return rcount;
}

static long get_process_read_files(libetrace_nfsdb_object* self, struct depproc_context* context,
		upid_t pid, std::vector<unsigned long>& dep_flist) {

	long rcount = 0;

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			for (unsigned long i=0; i<entry->open_files_count; ++i) {
				if ((entry->open_files[i].mode&0x01)==0) {
					unsigned long rdh = entry->open_files[i].path;
					if (context->dep_graph) {
						dep_flist.push_back(rdh);
					}
					if ((context->files_set.find(rdh)==context->files_set.end()) && (context->fdone.find(rdh)==context->fdone.end())) {
						libetrace_nfsdb_entry_openfile_object* openfile = libetrace_nfsdb_create_openfile_entry(
								self->nfsdb,entry,i,entry->nfsdb_index);
						if (!context->openfile_deps.insert(openfile).second) {
							Py_DecRef((PyObject*)openfile);
						}
						if ((context->excl_set.find(rdh)==context->excl_set.end()) &&
								(pattern_match(self->nfsdb->string_table[rdh],
										context->excl_patterns,context->excl_patterns_size)==context->negate_pattern)) {
							context->files.push_back(rdh);
							context->files_set.insert(rdh);
							DBG(context->fd_debug,"     added read file: %s\n",self->nfsdb->string_table[rdh]);
							rcount++;
						}
						else {
							context->fdone.insert(rdh);
						}
					}
				}
			}
		}
	}

	return rcount;
}

static int workaround_gcc_pipe_compilation_mode(libetrace_nfsdb_object* self, struct depproc_context* context, upid_t pid) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	struct nfsdb_entryMap_node* node = nfsdb_entryMap_search(&self->nfsdb->procmap,pid);
	if (node) {
		for (unsigned long u=0; u<node->entry_count; ++u) {
			struct nfsdb_entry* entry = node->entry_list[u];
			const char* bpath = self->nfsdb->string_table[entry->bpath];
			size_t len = strlen(bpath);
			if (!strncmp(bpath+len-4,"/cc1",4)) {
				struct ulongMap_node* node = ulongMap_search(&self->nfsdb->revforkmap, pid);
				ASSERT_RETURN_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at parent search [%lu]",-1,pid);
				context->all_writing_process_list.insert(node->value_list[0]);
				break;
			}
		}
	}

	return 0;
}

/* TODO: memory leaks */
static int build_dep_graph_entry(libetrace_nfsdb_object* self, struct depproc_context* context, upid_t pid,
		std::map<upid_t,std::string>& writing_pid_map, PyObject* dgraph, std::vector<unsigned long>& dep_flist) {

	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (writing_pid_map.find(pid)!=writing_pid_map.end()) {
		if ((context->wrap_deps)&&(MSB_IS_SET_UPID(pid))) {
			/* We have a pid of the wrapping process, get the real pid */
			pid = CLEAR_MSB_UPID(pid);
		}
		const std::string& f = writing_pid_map[pid];
		PyObject* dgraph_item = PyDict_GetItem(dgraph,PyUnicode_FromString(f.c_str()));
		PyObject* cmdTuple = PyTuple_New(2);
		PyTuple_SetItem(cmdTuple,0,Py_BuildValue(PY_ARG_PID_FMT,pid));
		PyObject* deplist = PyList_New(0);
		for (std::vector<unsigned long>::iterator i=dep_flist.begin(); i<dep_flist.end(); ++i) {
			PyList_Append(deplist,PyUnicode_FromString(self->nfsdb->string_table[(*i)]));
		}
		PyTuple_SetItem(cmdTuple,1,deplist);
		/*
		 *	dgraph:
		 *	  { root : ([ (pid,[file list]),(pid,[...]),... ], // CMDs need to be sorted by pid and lists of files merged
		 * 			  set([written files])),
		 * 	  ... }
		 * 	Here pid is a writing pid to the 'root' file dependency and file list is a list of dependencies
		 * 	(i.e. read files by pid) of the 'root' file dependency
		 */
		if (dgraph_item) {
			PyObject* nitem = PyTuple_New(2);
			PyObject* oldCmdsList = PyTuple_GetItem(dgraph_item,0);
			PyObject* cmdsList = PyList_GetSlice(oldCmdsList,0,PyList_Size(oldCmdsList));
			PyList_Append(cmdsList,cmdTuple);
			Py_DECREF(cmdTuple);
			PyObject* oldWrittenSet = PyTuple_GetItem(dgraph_item,1);
			PyObject* writtenSet = PySet_New(oldWrittenSet);
			for (std::vector<unsigned long>::iterator i=dep_flist.begin(); i<dep_flist.end(); ++i) {
				struct nfsdb_fileMap_node* fnode = fileMap_search(&self->nfsdb->filemap,(*i));
				ASSERT_RETURN_WITH_NFSDB_FORMAT_ERROR(fnode,"Internal nfsdb error at binary path handle [%lu]",-1,(*i));
				if ((fnode->wr_entry_count>0) || (fnode->rw_entry_count>0)) {
					PySet_Add(writtenSet,PyUnicode_FromString(self->nfsdb->string_table[(*i)]));
				}
			}
			PyTuple_SetItem(nitem,0,cmdsList);
			PyTuple_SetItem(nitem,1,writtenSet);
			PyDict_SetItem(dgraph,PyUnicode_FromString(f.c_str()),nitem);
			Py_DECREF(nitem);
		}
		else {
			PyObject* item = PyTuple_New(2);
			PyObject* cmdsList = PyList_New(0);
			PyList_Append(cmdsList,cmdTuple);
			Py_DECREF(cmdTuple);
			PyObject* writtenSet = PySet_New(0);
			for (std::vector<unsigned long>::iterator i=dep_flist.begin(); i<dep_flist.end(); ++i) {
				struct nfsdb_fileMap_node* fnode = fileMap_search(&self->nfsdb->filemap,(*i));
				ASSERT_RETURN_WITH_NFSDB_FORMAT_ERROR(fnode,"Internal nfsdb error at binary path handle [%lu]",-1,(*i));
				if ((fnode->wr_entry_count>0) || (fnode->rw_entry_count>0)) {
					PySet_Add(writtenSet,PyUnicode_FromString(self->nfsdb->string_table[(*i)]));
				}
			}
			PyTuple_SetItem(item,0,cmdsList);
			PyTuple_SetItem(item,1,writtenSet);
			PyDict_SetItem(dgraph,PyUnicode_FromString(f.c_str()),item);
			Py_DECREF(item);
		}
	}

	return 0;
}

/*
 *	file_dependencies(<PATH>,
 *	  exclude_files = [<PATH>,...],
 *	  exclude_patterns = [<PATTERN>,...],
 *	  exclude_commands = [<CMD_PATTERN>,...],
 *	  exclude_commands_index = [<CMD_PATTERN_ID>,...],
 *	  debug = True/False
 *	  dry_run = True/False
 *	  direct = True/False,
 *	  debug_fd = True/False,
 *	  negate_pattern = True/False,
 *	  dep_graph = True/False,
 *	  wrap_deps = True/False,
 *	  use_pipes = True/False,
 *	  timeout = N[s],
 *	  all_modules = [<MODULE_PATH>,...]
 */
PyObject* libetrace_nfsdb_file_dependencies(libetrace_nfsdb_object *self, PyObject *args, PyObject* kwargs) {

	struct depproc_context context;
	static char errmsg[ERRMSG_BUFFER_SIZE];

	if (PyTuple_Size(args)<=0) {
		ASSERT_WITH_NFSDB_ERROR(0,"File dependency processing requires at least one argument");
	}

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

	std::vector<std::pair<unsigned long,const char*>> phs;
	for (Py_ssize_t i=0; i<PyList_Size(path_args); ++i) {
		PyObject* arg = PyList_GetItem(path_args,i);
		const char* arg_cstr = PyString_get_c_str(arg);
		struct stringRefMap_node* pnode = stringRefMap_search(&self->nfsdb->revstringmap, arg_cstr);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(pnode,"Invalid pathname key [%s]",arg_cstr);
		unsigned long phandle = pnode->value;
		struct nfsdb_fileMap_node* node = fileMap_search(&self->nfsdb->filemap,phandle);
		ASSERT_WITH_NFSDB_FORMAT_ERROR(node,"Internal nfsdb error at binary path handle [%lu]",phandle);
		if ((node->wr_entry_count<=0) && (node->rw_entry_count<=0)) {
			/* Do not process files that weren't open for write.
			 * They don't depend on other files in dependency processing context */
			PYASSTR_DECREF(arg_cstr);
			continue;
		}
		phs.push_back(std::pair<unsigned long,const char*>(phandle,arg_cstr));
	}
	Py_DecRef(path_args);

	if (!depproc_parse_args(self,phs,kwargs,&context)) {
		phs_free(phs);
		return 0;
	}

	depproc_init_timer(&context);

	clock_t start, end;
	struct sigaction act;
	act.sa_handler = intHandler;
	sigaction(SIGINT, &act, 0);
	interrupt = 0;

	DBG(context.debug,"--- file_dependencies(\n");
	for (std::vector<std::pair<unsigned long,const char*>>::iterator i=phs.begin(); i<phs.end(); ++i) {
		DBG(context.debug,"                       %s\n",(*i).second);
	}
	DBG(context.debug,"                     )\n");

	int expired = 0;
	PyObject* dgraph = 0;

	PyObject* argv = 0;
	if (context.dep_graph) {
		argv = PyTuple_New(4);
	}
	else {
		argv = PyTuple_New(3);
	}
	PyObject* deps = PyList_New(0);
	PyObject* pids = PyList_New(0);
	PyObject* openfile_deps = PyList_New(0);
	PyTuple_SetItem(argv,0,pids);
	PyTuple_SetItem(argv,1,deps);
	PyTuple_SetItem(argv,2,openfile_deps);
	if (context.dep_graph) {
		dgraph = PyDict_New();
		PyTuple_SetItem(argv,3,dgraph);
	}

	std::set<unsigned long> _phs;
	for (std::vector<std::pair<unsigned long,const char*>>::iterator i=phs.begin(); i<phs.end(); ++i) {
		unsigned long phandle = (*i).first;
		const char* pathname = (*i).second;
		if ((context.excl_set.find(phandle)!=context.excl_set.end()) ||
			(pattern_match(pathname,context.excl_patterns,context.excl_patterns_size)!=context.negate_pattern)) {
			// We're matching the root path for exclusion; no dependencies and no processes that's written to any of its dependencies
			continue;
		}
		_phs.insert(phandle);
	}

	if (_phs.size()<=0) {
		/* Nothing left to process */
		if (context.timeout>0) {
			timer_delete(context.timer_id);
			g_timer = 0;
		}
		phs_free(phs);
		depproc_context_free(&context);
		return argv;
	}

	if (context.dry_run) {
		if (context.timeout>0) {
			timer_delete(context.timer_id);
			g_timer = 0;
		}
		phs_free(phs);
		depproc_context_free(&context);
		return argv;
	}

	for (std::set<unsigned long>::iterator i=_phs.begin(); i!=_phs.end(); ++i) {
		context.files.push_back(*i);
		context.files_set.insert(*i);
	}
	start = clock();

	/*
	 * Given specific linked file (say vmlinux.o) we get a list of all processes (most probably the linker process) that ever written
	 *  to this file ("writing_pid" variable). Now for each such process we get all the files read by this process (i.e. files read
	 *  by the linker) and we repeat this procedure for each read file (i.e. subsequent object files) until there's no new read files.
	 */

	while(!context.files.empty()) {
		static unsigned long pass_count = 0;
		context.writing_process_list.clear();
		auto file_iter = context.files.begin();
		std::map<upid_t,std::string> writing_pid_map;
		DBG(context.debug,"@ main pass[%lu] files to proc: %zu\n",pass_count,context.files.size());
		while(file_iter!=context.files.end()) {
			/* fh - handle of the file that's been written to
			 * Find all processes that have written to it */
			unsigned long fh = *file_iter;
			DBG(context.fd_debug,"   considering file: %s\n",self->nfsdb->string_table[fh]);
			if ((!context.direct_deps) ||
					(context.all_modules_set.find(fh)==context.all_modules_set.end())) {
				/* Gets a list of processes that have written to the 'fh' file
				 * For each such process:
				 *  - check if wrapping process should be considered instead of the original ('wrapping_pid' vs 'writing_pid')
				 *  - check if any command executed by this process matches the exclusion command pattern (skip this process in such a case)
				 *  - mark this process as a writing process for further processing
				 *      (fills 'writing_process_list' and 'all_writing_process_list')
				 *  - check if this process could write to any other processes (parent or sibling) through pipe. In such case repeat
				 *     the above steps for each such process (which can also be wrapped-up).
				 */
				long procs = depproc_process_written_file(self,&context,writing_pid_map,fh);
				if (procs<0) {
					expired = 2;
					goto maybe_expired;
				}
				DBG(context.fd_debug,"   number of new writing processes to consider: %ld\n",procs);
			}
			else {
				DBG(context.fd_debug,"   skipped file: %s\n",self->nfsdb->string_table[fh]);
				/* If the file on the dependency list is a linked module and we want only direct dependencies
				 * skip further processing for this file */
			}
			unsigned long pending = context.files.front();
			context.files.pop_front();
			context.files_set.erase(pending);
			context.fdone.insert(pending);
			file_iter++;
			if (g_timer||interrupt) {
				expired = 1;
				goto maybe_expired;
			}
		} /* while(files.iter) */

		/* Process all writing processes */
		DBG(context.debug,"@ read pass[%lu] writing processes to consider: %zu\n",pass_count++,context.writing_process_list.size());
		auto pid_iter = context.writing_process_list.begin();

		while (pid_iter!=context.writing_process_list.end()) {
			upid_t pid = *pid_iter;
			upid_t wrapping_pid = pid;
			if ((context.wrap_deps)&&(MSB_IS_SET_UPID(pid))) {
				/* We have a pid of the wrapping process, get the real pid */
				pid = CLEAR_MSB_UPID(pid);
			}
			std::vector<unsigned long> dep_flist;
			DBG(context.fd_debug,"   considering process: " GENERIC_ARG_PID_FMT "\n",pid);
			ulongMap_node* node = ulongMap_search(&self->nfsdb->rdmap, pid);
			if (node) {
				if ((context.wrap_deps)&&(MSB_IS_SET_UPID(wrapping_pid))) {
					/* Get files opened for read for all descendants of the wrapping process
					 *  Fills 'files', 'files_set' and 'fdone' upon completion
					 */
					get_wrapping_process_descendants_read_files(self,&context,pid,dep_flist);
				}
				else {
					/* Get files opened for read for this process
					 *  Fills 'files', 'files_set' and 'fdone' upon completion
					 */
					get_process_read_files(self,&context,pid,dep_flist);
				}
			}
			/* When gcc uses -pipe then driver process doesn't open the file being compiled and the driver pid
			 * is not listed in the writing process list. On the other hand the driver process is detected as a compiler
			 * but is missing in the list of processes that wrote to dependencies of some higher level module.
			 * All that lead to the Eclipse project generation errors.
			 * Let's have a workaround for this: detect *cc1 compilers and add it's parent (i.e. the driver process) to the
			 * writing process list */
			if (workaround_gcc_pipe_compilation_mode(self,&context,pid)) {
				expired = 2;
				goto maybe_expired;
			}
			pid_iter++;
			if (context.dep_graph) {
				if (build_dep_graph_entry(self,&context,wrapping_pid,writing_pid_map,dgraph,dep_flist)) {
					expired = 2;
					goto maybe_expired;
				}
			}
			if (g_timer||interrupt) {
				expired = 1;
				goto maybe_expired;
			}
		} /* while(pid.iter) */
		DBG( context.debug, "   done files: %zu\n",context.fdone.size());
	}
	end = clock();

maybe_expired:
	if (expired) {
		timer_delete(context.timer_id);
		g_timer = 0;
		depproc_context_free(&context);
		phs_free(phs);
		Py_DECREF(argv);

		if (expired==1) {
			DBG(context.debug,"--- file_dependencies(...): TIMEOUT\n");
			PyErr_SetString(PyExc_ValueError, "Timeout for command");
		}

		return NULL;
	}

	for (std::set<unsigned long>::iterator i=_phs.begin(); i!=_phs.end(); ++i) {
		context.fdone.erase(*i);
	}
	auto file_iter = context.fdone.begin();
	auto pid_iter = context.all_writing_process_list.begin();
	auto openfile_iter = context.openfile_deps.begin();
	pids = PyTuple_GetItem(argv,0);
	deps = PyTuple_GetItem(argv,1);
	openfile_deps = PyTuple_GetItem(argv,2);
	while (file_iter!=context.fdone.end()) {
		unsigned long v = *file_iter;
		PyObject* s = Py_BuildValue("s",self->nfsdb->string_table[v]);
		PyList_Append(deps,s);
		Py_DECREF(s);
		file_iter++;
	}
	while(pid_iter!=context.all_writing_process_list.end()) {
		PyObject* pi = Py_BuildValue(PY_ARG_PID_FMT, *pid_iter);
		PyList_Append(pids,pi);
		Py_DECREF(pi);
		pid_iter++;
	}
	while(openfile_iter!=context.openfile_deps.end()) {
		PyObject* openfile = (PyObject*)*openfile_iter;
		PyList_Append(openfile_deps,openfile);
		openfile_iter++;
	}

	if (context.dep_graph) {
		PyObject* dgraph_keys = PyDict_Keys(dgraph);
		ssize_t di;
		for (di=0; di<PyList_Size(dgraph_keys); ++di) {
			PyObject* fn = PyList_GetItem(dgraph_keys,di);
			PyObject* dgraph_item = PyDict_GetItem(dgraph,fn);
			PyObject* nitem = PyTuple_New(2);
			PyObject* oldCmdsList = PyTuple_GetItem(dgraph_item,0);
			PyObject* oldWrittenSet = PyTuple_GetItem(dgraph_item,1);
			PyObject* writtenList = PyList_New(0);
			ssize_t si;
			ssize_t setsz = PySet_Size(oldWrittenSet);
			for (si=0; si<setsz; ++si) {
				PyObject* wrf = PySet_Pop(oldWrittenSet);
				PyList_Append(writtenList,wrf);
				Py_DECREF(wrf);
			}
			PyObject* cmdsList = PyList_GetSlice(oldCmdsList,0,PyList_Size(oldCmdsList));
			PyTuple_SetItem(nitem,0,cmdsList);
			PyTuple_SetItem(nitem,1,writtenList);
			PyDict_SetItem(dgraph,fn,nitem);
			Py_DECREF(nitem);
		}
		Py_DECREF(dgraph_keys);
	}

	phs_free(phs);

	if (context.timeout>0) {
		timer_delete(context.timer_id);
		g_timer = 0;
	}

	depproc_context_free(&context);

	DBG(context.debug,"--- file_dependencies(...) - pids(%ld) deps(%ld) elapsed(%.2f[ms]): OK\n",PyList_Size(pids),PyList_Size(deps),
			1000*(((double) (end - start)) / CLOCKS_PER_SEC));

	return argv;
}
