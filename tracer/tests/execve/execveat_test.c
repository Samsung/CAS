/*
 * This program calls execveat() syscall to check if absolute program path
 * resolution works as expected in "at" case (i.e. relative paths are
 * resolved against some other fd than AT_FDCWD). The result from running
 * this program under tracer should be similar to the following:
 *
 * !New_proc|argsize=16,prognameisize=50,prognamepsize=50,cwdsize=36
 * !PI|/full/path/to/execveat_test
 * !PP|/full/path/to/execveat_test
 * !CW|/some/full/cwd/path
 * !A[0]./execveat_test
 * !End_of_args|
 * ...
 * !New_proc|argsize=5,prognameisize=7,prognamepsize=7,cwdsize=36
 * !PI|/bin/ls  # full path here
 * !PP|/bin/ls  # full path here
 * !CW|/some/full/cwd/path
 * !A[0]./ls
 * !End_of_args|
 */

#define _GNU_SOURCE
#include <sys/syscall.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int execveat(int fd, char* fname, char** argv, char** envp, int flags) {
	return syscall(SYS_execveat, fd, fname, argv, envp, flags);
}

int main(int argc, char** argv, char** envp) {

	char* nargv[] = {"./ls", NULL};

	int binfd = open("/bin", O_DIRECTORY | O_PATH);

	int ret = execveat(binfd, "./ls", nargv, envp, 0);
	if (ret) {
		return 42;
	}
	return 0;
}
