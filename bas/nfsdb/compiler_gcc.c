#include <signal.h>
#include <poll.h>
#include <wait.h>
#include <fnmatch.h>
#include "compiler.h"

DEFINE_COMPILER_OBJECT(gcc);

static int get_compiler_phases(struct buffer* buff) {

    const char* cc1_patterns[] = {
            "*cc1",
            "*cc1plus"
    };

    const char* ld_patterns[] = {
            "*collect2",
    };

    char* start = strnstr(buff->data,"\n ",buffer_size(buff));
    int r = 0;
    while(start) {
        start+=2;
        char* end = strnstr(start," ",buffer_size(buff)-(start-(char*)buff->data));
        *end = 0;

        if ( (!fnmatch(cc1_patterns[0],start,0)) || (!fnmatch(cc1_patterns[1],start,0)) ) {
            r|=PP_SEEN;
        }
        else if (!fnmatch(ld_patterns[0],start,0)) {
            r|=LD_SEEN;
        }
        *end = ' ';
        start = strnstr(end+1,"\n ",buffer_size(buff)-(end+1-(char*)buff->data));
    }

    return r;
}

DEFINE_COMPILER_REPR(gcc) {

	static char repr[128];
	snprintf(repr,128,"<gcc compiler object at %lx",(uintptr_t)self);
	return PyUnicode_FromString(repr);
}

/* TODO: this code would appreciate some refactoring */
DEFINE_COMPILER_GET_COMPILATIONS(gcc) {

/*
 *  get_compilations(self,args,kwargs) {...}
 *
 *  gcc.get_compilations([ (bin, CWD, cmdLst, pid), ... ], jobs, plugin_arg [,input_compiler_path])
 */

    PyObject* comps = PyTuple_GetItem(args,0);
    int jobs = (int)PyLong_AsLong(PyTuple_GetItem(args,1));
    PyObject* plugin_arg = PyTuple_GetItem(args,2);
    PyObject* input_compiler_path = 0;
    if (PyTuple_Size(args)>3) {
        input_compiler_path = PyTuple_GetItem(args,3);
    }
    PyObject* argv = PyDict_New();

	if (self->debug_compilations) {
		ssize_t ux;
		printf("--- libetrace_compiler_gcc_get_compilations() [%d]: (%ld)\n",getpid(),PyList_Size(comps));
		for (ux=0; ux<PyList_Size(comps); ++ux) {
			PyObject* comp = PyList_GetItem(comps,ux);
			PyObject* cmd = PyTuple_GetItem(comp,2);
			printf("--- libetrace_compiler_gcc_get_compilations() [%d] | INPUT: \n%s\n\n",getpid(),STRJOIN(cmd));
		}
	}

    DBG(self->debug,"--- libetrace_compiler_gcc_get_compilations(%ld,%d)\n",PyList_Size(comps),jobs);

    /*
     * No compilations provided - get the hell out of here!
     */
    if (PyList_Size(comps)<=0) {
        return argv;
    }

    DBG(self->debug_compilations,"Searching for gcc compilations ...\n");

    struct sigaction act;
    act.sa_handler = intHandler;
    sigaction(SIGINT, &act, 0);
    interrupt = 0;

    struct pollfd* pfds = calloc(sizeof(struct pollfd),jobs);
    int u;
    for (u=0; u<jobs; ++u) pfds[u].fd = -1;
    struct job* pjob = calloc(sizeof(struct job),jobs);
    Py_ssize_t proc_index=0, comp_index=0;
    int job_num = 0;
    Py_ssize_t comp_num = PyList_Size(comps);
    progress_max_g = comp_num;
    progress_g = 0;
    DBG(self->debug_compilations, " *** comp_num: %ld\n",comp_num);
    while((job_num<jobs) && (comp_index<comp_num)) {
        PyObject* comp = PyList_GetItem(comps,comp_index);
        if (self->debug) {
            PyObject* cmd = PyTuple_GetItem(comp,2);
            DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | CONSIDERING: \n%s\n\n",getpid(),STRJOIN(cmd));
        }
        const char* bin = PyString_get_c_str(PyTuple_GetItem(comp,0));
        const char* cwd = PyString_get_c_str(PyTuple_GetItem(comp,1));
        PyObject* original_args = PyTuple_GetItem(comp,2);
        PyObject* bargs = PyList_GetSlice(original_args,0,PyList_Size(original_args));
        PyList_Insert(bargs,1,Py_BuildValue("s","-###"));
        int pid = run_child(bin,cwd,bargs,&pfds[proc_index].fd,0);
        if (pid>0) {
            if (self->debug) {
                DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULED [#]: \n%s [%s] %s\n\n",
                		getpid(),bin,cwd,STRJOIN(bargs));
            }
            DBG(self->debug_compilations, " *** %d created [%s](hcmd)\n",pid,bin);
            pfds[proc_index].events = POLLIN|POLLHUP;
            DBG(self->debug_compilations,"{%s}:(%ld)\n",REPR(comp),comp_index);
            pjob[proc_index].index = comp_index;
            pjob[proc_index].pid = pid;
            buffer_init(&pjob[proc_index].buff,4096);
            pjob[proc_index].pfd = &pfds[proc_index];
            pjob[proc_index].comp = comp;
            pjob[proc_index].input = 0;
            pjob[proc_index].fns = 0;
            proc_index++;
            job_num++;
        }
        else {
            if (self->debug) {
                DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULE FAILED [#]: \n%s\n\n",getpid(),STRJOIN(bargs));
            }
            DBG(self->debug_compilations, " *** Failed to create child: [%s](hcmd): %d\n",bin,errno);
        }
        comp_index++;
        Py_DECREF(bargs);
    }

    int initial_job_num = job_num;
    while(job_num) {

        int status;
        if (interrupt) {
            for (u=0; u<jobs; ++u) {
                if (pfds[u].fd!=-1) {
                    int r = waitpid(pjob[u].pid,&status,0);
                    if (r<0) DBG(1, " *** waitpid(%d): %d (%d)\n",pjob[u].pid,r,(r<0)?errno:0);
                    if (r>0) {
                        if (status!=0) DBG(0,"wait status: %d (%d)\n",status,WIFEXITED(status));
                        if (WIFEXITED(status)) {
                            pjob[u].pid = 0;
                            close(pfds[u].fd);
                            pfds[u].fd = -1;
                        }
                    }
                }
            }
            printf("\n");
            Py_DecRef(argv);
            argv = PyDict_New();
            goto done;
        }

        poll(pfds,jobs,-1);
        for (u=0; u<jobs; ++u) {
            if (pfds[u].fd!=-1) {
                if( pfds[u].revents & (POLLIN|POLLHUP) ) {
                    struct job* job = pjob+u;
                    buffer_alloc(&job->buff,4096);
                    ssize_t count = read(pfds[u].fd, job->buff.data+job->buff.size, 4096);
                    if (count == -1) {
                        if (errno != EINTR) {
                            /* Mark the fd as invalid and try to wait the child */
                            DBG(self->debug_compilations, " *** Error on read at %d (pid: %d): %d\n",pfds[u].fd,pjob[u].pid,errno);
                            close(pfds[u].fd);
                            pfds[u].fd = -1;
                            waitpid(pjob[u].pid,&status,WNOHANG);
                        }
                    }
                    else if (count==0) {
                        /* hit EOF; reap the child and spawn new child (if more children is waiting in the queue) */
                        int r = waitpid(pjob[u].pid,&status,WNOHANG);
                        if (r<0) DBG(1, " *** waitpid(%d): %d (%d)\n",pjob[u].pid,r,(r<0)?errno:0);
                        if (r>0) {
                            if (status!=0) DBG(0,"wait status: %d (%d)\n",status,WIFEXITED(status));
                            if (WIFEXITED(status)) {
                                DBG(self->debug_compilations, " *** %d exited\n",pjob[u].pid);
                                pjob[u].pid = 0;
                                close(pfds[u].fd);
                                pfds[u].fd = -1;
                                if (!pjob[u].input) {
                                	int cp = get_compiler_phases(&pjob[u].buff);
                                	DBG(self->debug_compilations,"@[#] [%d] {%d}(%d) [%s]\n",
                                			cp,pjob[u].pid,pjob[u].index,
                                            REPR(PyUnicode_FromStringAndSize((const char*)pjob[u].buff.data,buffer_size(&pjob[u].buff))));
                                    if (cp & PP_SEEN) {
                                        /* Spawn the process for reading input files */
                                        const char* bin;
                                        if (!input_compiler_path) {
                                            bin = PyString_get_c_str(PyTuple_GetItem(pjob[u].comp,0));
                                        } else {
                                            bin = PyString_get_c_str(input_compiler_path);
                                        }
                                        const char* cwd = PyString_get_c_str(PyTuple_GetItem(pjob[u].comp,1));
                                        PyObject* original_args = PyTuple_GetItem(pjob[u].comp,2);
                                        PyObject* bargs = PyList_GetSlice(original_args,0,PyList_Size(original_args));
                                        if (cp & LD_SEEN) {
                                            if (PySequence_Contains(bargs,Py_BuildValue("s","-o"))) {
                                                Py_ssize_t i = PySequence_Index(bargs, Py_BuildValue("s","-o"));
                                                PySequence_DelItem(bargs, i);
                                                PySequence_DelItem(bargs, i);
                                            }
                                        }
                                        PyList_Append(bargs,plugin_arg);
                                        if (PySequence_Contains(bargs,Py_BuildValue("s","-pipe"))) {
                                            Py_ssize_t j = PySequence_Index(bargs,Py_BuildValue("s","-pipe"));
                                            PySequence_DelItem(bargs,j);
                                        }
                                        if (PySequence_Contains(bargs,Py_BuildValue("s","-E"))) {
											Py_ssize_t j = PySequence_Index(bargs,Py_BuildValue("s","-E"));
											PySequence_DelItem(bargs,j);
										}
                                        if (!PySequence_Contains(bargs,Py_BuildValue("s","-c"))) {
                                        	PyList_Append(bargs,Py_BuildValue("s","-c"));
                                        }
                                        int pid = run_child(bin,cwd,bargs,&pfds[u].fd,0);
                                        if (pid>0) {
                                        	if (self->debug) {
                                        		DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULED [I]: \n%s [%s] %s\n\n",
                                        				getpid(),bin,cwd,STRJOIN(bargs));
                                        	}
                                            DBG(self->debug_compilations, " *** %d created [%s](icmd)\n",pid,bin);
                                            pfds[u].events = POLLIN|POLLHUP;
                                            pjob[u].pid = pid;
                                            buffer_clear(&pjob[u].buff);
                                            pjob[u].pfd = &pfds[u];
                                            pjob[u].input = 1;
                                            pjob[u].fns = 0;
                                            continue;
                                        }
                                        else {
                                        	if (self->debug) {
                                        		DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULE FAILED [I]: \n%s\n\n",getpid(),STRJOIN(bargs));
                                        	}
                                            DBG(self->debug_compilations, " *** Failed to create child: [%s](icmd): %d\n",bin,errno);
                                        }
                                        Py_DecRef(bargs);
                                    }
                                    else {
                                        const int debug_err = 0;
                                        DBG(debug_err,"%s\n",REPR(pjob[u].comp));
                                        DBG(debug_err,"[");DBGC(debug_err,buffer_prints(&pjob[u].buff));DBG(debug_err,"]\n\n");
                                    }
                                }
                                else {
                                    if (pjob[u].fns) {
                                        DBG(self->debug_compilations,"@[M] [%s]:(%d)\n",
                                                REPR(PyUnicode_FromStringAndSize((const char*)pjob[u].buff.data,buffer_size(&pjob[u].buff))),pjob[u].index);
                                        /* Parse include paths and macro definitions */
                                        PyObject* im = PyUnicode_FromStringAndSize((const char*)pjob[u].buff.data,buffer_size(&pjob[u].buff));
                                        PyObject* vt = PyTuple_New(2);
                                        PyTuple_SetItem(vt,0,pjob[u].fns);
                                        PyTuple_SetItem(vt,1,im);
                                        PyDict_SetItem(argv,Py_BuildValue("i",pjob[u].index),vt);
                                        if (self->debug_compilations && self->debug) {
                                            printf("libetrace_compiler_gcc_get_compilations() [%d]: ADDING @(%d) [",getpid(),pjob[u].index);
                                            ssize_t ci;
                                            for (ci=0; ci<PyList_Size(pjob[u].fns); ++ci) {
                                                printf("%s",PyString_get_c_str(PyList_GetItem(pjob[u].fns,ci)));
                                                if (ci<PyList_Size(pjob[u].fns)-1) printf(",");
                                            }
                                            printf("]\n");
                                        }
                                    }
                                    else {
                                        /* Parse the input files */
                                        const char* cwd = PyString_get_c_str(PyTuple_GetItem(pjob[u].comp,1));
                                        size_t cwdLen = strlen(cwd);
                                        DBG(self->debug_compilations,"@[I] [%s]:(%d)\n",
                                                REPR(PyUnicode_FromStringAndSize((const char*)pjob[u].buff.data,buffer_size(&pjob[u].buff))),pjob[u].index);
                                        PyObject* fns = splitn((const char*)pjob[u].buff.data,buffer_size(&pjob[u].buff));
                                        Py_ssize_t i = 0;
                                        int access_ok = 1;
                                        for (i=0; i<PyList_Size(fns); ++i) {
                                            const char* fn = PyString_get_c_str(PyList_GetItem(fns,i));
                                            if (fn[0]=='/') {
                                                if (access(fn,F_OK)==-1) {
                                                    access_ok=0;
                                                    break;
                                                }
                                            }
                                            else {
                                                char *fullpath = malloc(cwdLen + strlen(fn) + 2);
                                                sprintf(fullpath, "%s/%s", cwd, fn);
                                                int r = access(fullpath,F_OK);
                                                free(fullpath);
                                                if (r==-1) {
                                                    access_ok=0;
                                                    break;
                                                }
                                            }
                                        }
                                        if (access_ok) {
                                            /* Spawn the child to read all include paths and preprocessor definitions */
                                            const char* bin = PyString_get_c_str(PyTuple_GetItem(pjob[u].comp,0));
                                            PyObject* original_args = PyTuple_GetItem(pjob[u].comp,2);
                                            PyObject* bargs = PyList_GetSlice(original_args,0,PyList_Size(original_args));

                                            PyObject* istr = Py_BuildValue("s","-include");
                                            while(PySequence_Contains(bargs,istr)) {
                                                i = PySequence_Index(bargs, istr);
                                                PySequence_DelItem(bargs, i);
                                                PySequence_DelItem(bargs, i);
                                            }
                                            if (PySequence_Contains(bargs,Py_BuildValue("s","-o"))) {
                                                i = PySequence_Index(bargs, Py_BuildValue("s","-o"));
                                                PyList_SetItem(bargs,i,Py_BuildValue("s","-E"));
                                                PyList_SetItem(bargs,i+1,Py_BuildValue("s","-P"));
                                                PyList_Insert(bargs,i+2,Py_BuildValue("s","-v"));
                                                PyList_Insert(bargs,i+3,Py_BuildValue("s","-dD"));
                                            }
                                            else {
                                                PyList_Append(bargs,Py_BuildValue("s","-E"));
                                                PyList_Append(bargs,Py_BuildValue("s","-P"));
                                                PyList_Append(bargs,Py_BuildValue("s","-v"));
                                                PyList_Append(bargs,Py_BuildValue("s","-dD"));
                                            }
                                            for (i=0; i<PyList_Size(fns); ++i) {
                                                if (PySequence_Contains(bargs,PyList_GetItem(fns,i))) {
                                                    Py_ssize_t j = PySequence_Index(bargs,PyList_GetItem(fns,i));
                                                    PySequence_DelItem(bargs,j);
                                                }
                                            }
                                            PyList_Append(bargs,Py_BuildValue("s","-"));
                                            int pid = run_child(bin,cwd,bargs,&pfds[u].fd,1);
                                            if (pid>0) {
                                                if (self->debug) {
                                                    DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULED [M]: \n%s [%s] %s\n\n",
                                                    		getpid(),bin,cwd,STRJOIN(bargs));
                                                }
                                                DBG(self->debug_compilations, " *** %d created [%s](mcmd)\n",pid,bin);
                                                pfds[u].events = POLLIN|POLLHUP;
                                                pjob[u].pid = pid;
                                                buffer_clear(&pjob[u].buff);
                                                pjob[u].pfd = &pfds[u];
                                                pjob[u].fns = fns;
                                                continue;
                                            }
                                            else {
                                                if (self->debug) {
                                                    DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULE FAILED [M]: \n%s\n\n",getpid(),STRJOIN(bargs));
                                                }
                                                DBG(self->debug_compilations, " *** Failed to create child: [%s](mcmd): %d\n",bin,errno);
                                            }
                                            Py_DecRef(bargs);
                                        }
                                    }
                                }
                                job_num--;
                                /* Spawn new child process from the queue */
                                int try_spawn = 1;
                                while ((comp_index<PyList_Size(comps))&&try_spawn) {
                                    PyObject* comp = PyList_GetItem(comps,comp_index);
                                    if (self->debug) {
                                        PyObject* cmd = PyTuple_GetItem(comp,2);
                                        DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | CONSIDERING: \n%s\n\n",getpid(),STRJOIN(cmd));
                                    }
                                    const char* bin = PyString_get_c_str(PyTuple_GetItem(comp,0));
                                    const char* cwd = PyString_get_c_str(PyTuple_GetItem(comp,1));
                                    PyObject* original_args = PyTuple_GetItem(comp,2);
                                    PyObject* bargs = PyList_GetSlice(original_args,0,PyList_Size(original_args));
                                    PyList_Insert(bargs,1,Py_BuildValue("s","-###"));
                                    int pid = run_child(bin,cwd,bargs,&pfds[u].fd,0);
                                    if (pid>0) {
                                        if (self->debug) {
                                            DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULED [#]: \n%s [%s] %s\n\n",
                                            		getpid(),bin,cwd,STRJOIN(bargs));
                                        }
									  DBG(self->debug_compilations, " *** %d created [%s]\n",pid,bin);
									  pfds[u].events = POLLIN|POLLHUP;
									  DBG(self->debug_compilations,"{%s}:(%ld)\n",REPR(comp),comp_index);
									  pjob[u].index = comp_index;
									  pjob[u].pid = pid;
									  buffer_clear(&pjob[u].buff);
									  pjob[u].pfd = &pfds[u];
									  pjob[u].comp = comp;
									  pjob[u].input = 0;
									  pjob[u].fns = 0;
									  job_num++;
									  try_spawn--;
                                    }
                                    else {
                                        if (self->debug) {
                                            DBG(self->debug_compilations,"--- libetrace_compiler_gcc_get_compilations() [%d] | SCHEDULE FAILED [#]: \n%s\n\n",getpid(),STRJOIN(bargs));
                                        }
                                      DBG(self->debug_compilations, " *** Failed to create child: [%s](hcmd): %d\n",bin,errno);
                                    }
                                    comp_index++;
                                    Py_DECREF(bargs);
                                }
                            }
                        }
                    }
                    else {
                        buffer_advance(&job->buff,count);
                    }
                }
            }
        }

    }

done:
    for (u=0; u<initial_job_num; ++u) {
        buffer_destroy(&pjob[u].buff);
    }

    free(pjob);
    free(pfds);

    DBG(self->debug,"--- libetrace_compiler_gcc_get_compilations(): done [%d] size: %ld\n",getpid(),PyDict_Size(argv));

    return argv;

}
