#include <unistd.h>
#include <errno.h>
#include <dirent.h>
#include <fnmatch.h>
#include <stddef.h>
#define __USE_XOPEN_EXTENDED
#include <ftw.h>
#include "compiler.h"

#ifdef __USE_GNU
#define var_environ environ
#else
#define var_environ __environ
#endif

unsigned long progress_max_g;
unsigned long progress_g;

int run_child(const char* bin, const char* cwd, PyObject* args, int* pfd, int child_read_fd) {

    /*
     * Check cwd points to the valid directory
     */
    DIR* dir = opendir(cwd);
    if (dir == NULL)
        return -1;
    closedir(dir);

    /*
     * So basically, this whole block just checks whether bin exists?
     *  is it neccessary? I mean, we are only running clang here anyway...
     */
    // char* current_wd = getcwd(0, 0);
    // if (chdir(cwd)<0) {
    //     free(current_wd);
    //     return -1;
    // }

    // int ra = access(bin, F_OK | X_OK);
    // if (ra) {
    //     free(current_wd);
    //     return -1;
    // }
    
    // chdir(current_wd);
    // free(current_wd);


    /*
     * Do some magic pipes trickery
     */
    int fd[2];
    int fdr[2];
    int r = pipe(fd);
    if (r<0) return r;
    if (child_read_fd) {
        int r = pipe(fdr);
        if (r<0) return r;
    }
    pid_t pid = fork();
    if (pid > 0) {
        /* We're at the parent */
        close(fd[1]);
        if (child_read_fd) {
            close(fdr[0]);
            close(fdr[1]);
        }
    }
    else if (pid == 0) {
        /* Hello child! */
        char** argv = malloc((PyList_Size(args)+1)*sizeof(const char*));
        Py_ssize_t u;
        for (u=0; u<PyList_Size(args); ++u) {
            argv[u] = (char*)PyString_get_c_str(PyList_GetItem(args,u));
        }
        argv[0] = (char*)bin;
        argv[u] = 0;

        /*
         * What is with this dup2 in loops?
         */
        while ((dup2(fd[1], STDOUT_FILENO) == -1) && (errno == EINTR)) {}
        while ((dup2(fd[1], STDERR_FILENO) == -1) && (errno == EINTR)) {}
        
        close(fd[1]);
        close(fd[0]);
        if (child_read_fd) {
            while ((dup2(fdr[0], STDIN_FILENO) == -1) && (errno == EINTR)) {}
            close(fdr[0]);
            close(fdr[1]);
        }
        
        if (chdir(cwd) < 0)
            return -1;

        execve(bin,argv,var_environ);
        exit(errno);
    }
    else {
        /* Fork failed */
        return -1;
    }

    *pfd = fd[0];
    return pid;
}

PyObject* splitn(const char* str, size_t size) {

    while(size && isspace(*str)) {
        str++;
        size--;
    }
    while(size && isspace(str[size-1])) {
        --size;
    }
    PyObject* lst = PyList_New(0);
    char* newl = strnstr(str,"\n",size);
    while(newl) {
        ptrdiff_t part_size = newl-str;
        PyList_Append(lst,PyUnicode_FromStringAndSize(str,part_size));
        str+=part_size+1;
        size-=part_size+1;
        newl = strnstr(str,"\n",size);
    }
    if (size) {
        PyList_Append(lst,PyUnicode_FromStringAndSize(str,size));
    }
    return lst;
}

PyObject* check_file_access(PyObject* fns, const char* cwd) {

    size_t cwdLen = strlen(cwd);
    const char* fn = PyString_get_c_str(fns);
    PyObject* rfn;
    if (fn[0]=='/') {
        if (access(fn,F_OK)==-1) {
            return 0;
        }
        rfn = Py_BuildValue("s",fn);
    }
    else {
        char *fullpath = malloc(cwdLen + strlen(fn) + 2);
        sprintf(fullpath, "%s/%s", cwd, fn);
        int r = access(fullpath,F_OK);
        if (r==-1) {
            free(fullpath);
            return 0;
        }
        rfn = Py_BuildValue("s",fullpath);
        free(fullpath);
    }
    return rfn;
}

int strtab_contains(const char** tab,size_t num,char* s) {
    size_t i;
    for (i=0; i<num; ++i) {
        if (!strcmp(tab[i],s)) return 1;
    }
    return 0;
}

static inline int startsWith(const char *pre, const char *str) {
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

/*
 * Returns index of first item in the sequence that starts with the given prefix (or -1 if not found)
 * If the item matches the prefix perfectly, match is set to 1 (otherwise it is set to 0)
 */
Py_ssize_t PySequence_Contains_Prefix(PyObject* seq,PyObject* prefix,int* match) {

    Py_ssize_t n;
    PyObject *it;  /* iter(seq) */

    if (seq == NULL || prefix == NULL) {
        return -1;
    }

    it = PyObject_GetIter(seq);
    if (it == NULL) {
        return -1;
    }

    n = 0;
    for (;;) {
        int cmp;
        PyObject *item = PyIter_Next(it);
        if (item == NULL) {
            if (PyErr_Occurred()) {
                goto Fail;
            }
            break;
        }

        cmp = startsWith(PyString_get_c_str(prefix),PyString_get_c_str(item));
        if (cmp) {
            if (!strcmp(PyString_get_c_str(prefix),PyString_get_c_str(item))) {
                *match = 1;
            }
            else {
                *match = 0;
            }
            goto Done;
        }

        Py_DECREF(item);
        n++;
    }

Fail:
    n = -1;
Done:
    Py_DECREF(it);
    return n;
}

static int unlink_cb(const char *fpath, const struct stat *sb, int typeflag, struct FTW *ftwbuf) {

    int rv = remove(fpath);

    if (rv)
        perror(fpath);

    return rv;
}

int rmrf(char *path) {
    return nftw(path, unlink_cb, 64, FTW_DEPTH | FTW_PHYS);
}

