#include <signal.h>
#include <poll.h>
#include <wait.h>
#include "compiler.h"

DEFINE_COMPILER_OBJECT(clang);

DEFINE_COMPILER_REPR(clang) {

	static char repr[128];
	snprintf(repr,128,"<clang compiler object at %lx",(uintptr_t)self);
	return PyUnicode_FromString(repr);
}

DEFINE_COMPILER_GET_COMPILATIONS(clang) {

    /*
     *  get_compilations(self,args,kwargs) {...}
     *
     *  clang.get_compilations([ (bin, CWD, cmdLst, pid, comp_type ), ... ],jobs)
     *  ex. 
     *      (u'/usr/lib/llvm-10/bin/clang', 
     *       u'/mnt/HugeDisk/Linux_VMs/nbas/linux', 
     *      [u'/usr/lib/llvm-10/bin/clang', u'-cc1', u'-triple', u'i386-pc-linux-code16', u'-S', ...],
     *      196568146591, 
     *      1 // 1 for c or 2 for c++)
     */

        PyObject* comps = PyTuple_GetItem(args,0);
        int jobs = (int)PyLong_AsLong(PyTuple_GetItem(args,1));
        PyObject* argv = PyDict_New();

		if (self->debug_compilations) {
			ssize_t ux;
			printf("--- libetrace_compiler_clang_input_files() [%d]: (%ld)\n",getpid(),PyList_Size(comps));
			for (ux=0; ux<PyList_Size(comps); ++ux) {
				PyObject* comp = PyList_GetItem(comps,ux);
				PyObject* cmd = PyTuple_GetItem(comp,2);
				printf("--- libetrace_compiler_clang_get_compilations() [%d] {%ld} | INPUT: \n%s\n\n",getpid(),ux,STRJOIN(cmd));
			}
		}

        DBG(self->debug,"--- libetrace_compiler_clang_get_compilations(%ld,%d) [%d]\n",PyList_Size(comps),jobs,getpid());

        /*
         * No compilations provided - get the hell out of here!
         */
        if (PyList_Size(comps)<=0) {
            return argv;
        }

        /*
         * Read `tailopts` optional argument
         *  - tailopts is an array of arguments to be appended at the end of provided compilation command
         */
        const char** tailopts = 0;
        size_t tailopts_size = 0;
        if (kwargs) {
            PyObject* Py_tailopts = PyDict_GetItemString(kwargs, "tailopts");
            if (Py_tailopts && (PyList_Size(Py_tailopts)>0)) {
            	tailopts = malloc(PyList_Size(Py_tailopts)*sizeof(const char*));
            	tailopts_size = PyList_Size(Py_tailopts);
                int u;
                for (u=0; u<PyList_Size(Py_tailopts); ++u) {
                	tailopts[u] = PyString_get_c_str(PyList_GetItem(Py_tailopts,u));
                    DBG(self->debug,"        tailopt: %s\n",tailopts[u]);
                }
            }
        }

        /*
         * Allocate job related structures
         */
        struct pollfd* pfds = calloc(sizeof(struct pollfd),jobs);
        for (int u = 0; u < jobs; u++) 
            pfds[u].fd = -1;
        struct job job = {0};

        Py_ssize_t proc_index=0, comp_index=0;
        Py_ssize_t comp_num = PyList_Size(comps);

        progress_max_g = comp_num;
        progress_g = 0;
        DBG(self->debug_compilations, " *** comp_num: %ld\n",comp_num);

        /*
         * Start initial set of jobs
         */
        while((comp_index < comp_num)) {
            PyObject* comp = PyList_GetItem(comps,comp_index);
            if (self->debug) {
                PyObject* cmd = PyTuple_GetItem(comp,2);
                DBG(self->debug_compilations,"--- libetrace_compiler_clang_get_compilations() [%d] | CONSIDERING I{%ld}: \n%s\n\n",getpid(),
                        comp_index,STRJOIN(cmd));
            }

            /*
             * Extract arguments for this particular compilation unit
             */
            const char* bin = PyString_get_c_str(PyTuple_GetItem(comp,0));
            const char* cwd = PyString_get_c_str(PyTuple_GetItem(comp,1));
            PyObject* original_args = PyTuple_GetItem(comp,2);
            int compilation_type = (int)PyLong_AsLong(PyTuple_GetItem(comp,4));
            PyObject* bargs = PyList_GetSlice(original_args,0,PyList_Size(original_args));  // Create a copy?
            PyObject* fns = check_file_access(PyList_GetItem(bargs,PyList_Size(bargs)-1),cwd);
            int extra_arg_num = 0;
            ssize_t eidx = PyList_Size(bargs)-2;
            if (!fns) {
            	extra_arg_num++;
                // Check if there aren't any command line switches at the end of command
                while((eidx>=0) && (strtab_contains(tailopts,tailopts_size,
                        (char*)PyString_get_c_str(PyList_GetItem(bargs,eidx))))) {
                    eidx--;
                    extra_arg_num++;
                }
                fns = check_file_access(PyList_GetItem(bargs,eidx),cwd);
            }
            if (!fns) {
                DBG(self->debug_compilations,"WARNING: File access failed for (%s):(%s) at command:\n%s\n",cwd,
                        PyString_get_c_str(PyList_GetItem(bargs,eidx)),STRJOIN(original_args));
            }
            if (fns) {
                Py_ssize_t i;
                i = PySequence_Index(bargs, Py_BuildValue("s","-o"));
                PyList_Insert(bargs,i,Py_BuildValue("s","-dD"));
                PyList_Insert(bargs,i,Py_BuildValue("s","-E"));
                PyList_Insert(bargs,i,Py_BuildValue("s","-P"));
                PyList_Insert(bargs,i,Py_BuildValue("s","-v"));
                if (compilation_type==1) {
                	PyList_Insert(bargs,i,Py_BuildValue("s","c"));
                }
                else {
                	PyList_Insert(bargs,i,Py_BuildValue("s","c++"));
                }
                PyList_Insert(bargs,i,Py_BuildValue("s","-x"));
                PyList_SetItem(bargs,i+7,Py_BuildValue("s","-"));
                if (extra_arg_num>0) {
				   PyObject* nbargs = PyList_GetSlice(bargs,0,PyList_Size(bargs)-extra_arg_num-1);
				   int eai;
				   for (eai=0; eai<extra_arg_num; ++eai) {
					   PyList_Append(nbargs,PyList_GetItem(bargs,PyList_Size(bargs)-1-extra_arg_num+1+eai));
				   }
				   PyList_Append(nbargs,Py_BuildValue("s","-"));
				   Py_DecRef(bargs);
				   bargs = nbargs;
                }
                else {
                    PyList_SetItem(bargs,PyList_Size(bargs)-1,Py_BuildValue("s","-"));
                }

                /*
                 * Start single clang subprocess
                 */
                int pid = run_child(bin, cwd, bargs, &pfds[proc_index].fd, 1);
                if (pid < 0) {
                    DBG(self->debug_compilations, " *** Failed to create child: [%s](hcmd): %d\n",bin,errno);
                    continue;
                } else if(pid > 0) {
                	DBG(self->debug_compilations,"--- libetrace_compiler_clang_get_compilations() [%d](%ld) | SCHEDULED:"
                			"\n%s\n *** %d created [%s](hcmd)\n{%s}\n",
                			getpid(),comp_index,STRJOIN(bargs),pid,bin,REPR(comp));
                    DBG(self->debug_compilations, " *** %d created [%s](hcmd)\n",pid,bin);
                    job.index = comp_index;
                    job.pid = pid;
                    buffer_init(&job.buff,8192);
                    job.pfd = &pfds[proc_index];
                    job.comp = comp;
                    job.input = 0;
                    job.fns = PyList_New(0);
                    PyList_Append(job.fns,fns);

                    while(1) {
                        buffer_alloc(&job.buff,8192);
                        ssize_t count = read(pfds[proc_index].fd, job.buff.data + job.buff.size, 8192);
                        if(count > 0)
                            buffer_advance(&job.buff,count);
                        else if(count == 0)
                            break;
                        else 
                            // Handle an error?
                            ;
                    }

                    waitpid(job.pid, NULL, 0);


                    /*
                     * Parse collected output
                     */
                    DBG(self->debug_compilations, " *** %d exited\n",job.pid);
                    DBG(self->debug_compilations,"@@@[%s]:(%d)\n",
                            REPR(PyUnicode_FromStringAndSize((const char*)job.buff.data,buffer_size(&job.buff))),job.index);
                    /* Parse include paths and macro definitions */
                    PyObject* original_args = PyTuple_GetItem(job.comp,2);
                    if (!PySequence_Contains(original_args,Py_BuildValue("s","-E"))) {
                        PyObject* im = PyUnicode_FromStringAndSize((const char*)job.buff.data,buffer_size(&job.buff));
                        PyObject* vt = PyTuple_New(2);
                        PyTuple_SetItem(vt,0,job.fns);
                        PyTuple_SetItem(vt,1,im);
                        PyDict_SetItem(argv,Py_BuildValue("i",job.index),vt);
                        if (self->debug_compilations && self->debug) {
                            printf("libetrace_compiler_clang_get_compilations() [%d]: ADDING @(%d) [",getpid(),job.index);
                            ssize_t ci;
                            for (ci=0; ci<PyList_Size(job.fns); ++ci) {
                                printf("%s",PyString_get_c_str(PyList_GetItem(job.fns,ci)));
                                if (ci<PyList_Size(job.fns)-1) printf(",");
                            }
                            printf("]\n");
                        }
                    }
                }
                Py_DECREF(bargs);
            }
            comp_index++;
        }

        buffer_destroy(&job.buff);
        free(pfds);

        DBG(self->debug,"--- libetrace_compiler_clang_get_compilations(): done (%ld) [%d]\n",PyDict_Size(argv),getpid());

        return argv;
}
