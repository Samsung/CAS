#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>

int main(int argc, char** argv) {

#define CHECK(x) do {\
	if ((x) < 0) {\
		printf("%s failed: %d (%s)\n", #x, errno, strerror(errno));\
	}\
} while (0)

	// regular file, absolute path
	CHECK(open("/bin/ls", O_RDONLY));

	// regular file, relative path
	CHECK(open("./../../../../../../../../../../../../../bin/ls", O_RDONLY));

	// symlink in the middle, absolute path
	CHECK(open("/proc/self/status", O_RDONLY));

	// symlink in the middle, relative path
	CHECK(open("../../../../../../../../../../../../../proc/self/status", O_RDONLY));


	// OPENAT(AT_FDCWD)

	// regular file, absolute path
	CHECK(openat(AT_FDCWD, "/bin/ls", O_RDONLY));

	// regular file, relative path
	CHECK(openat(AT_FDCWD, "./../../../../../../../../../../../../../bin/ls", O_RDONLY));

	// symlink in the middle, absolute path
	CHECK(openat(AT_FDCWD, "/proc/self/status", O_RDONLY));

	// symlink in the middle, relative path
	CHECK(openat(AT_FDCWD, "../../../../../../../../../../../../../proc/self/status", O_RDONLY));

	// empty name (?)
	// CHECK(openat(AT_FDCWD, "", O_RDONLY)); // ENOENT

	// null name (?)
	// CHECK(openat(AT_FDCWD, NULL, O_RDONLY));  // EBADADDR


	// OPENAT(custom_fd)

	int custom_fd = open("/bin", O_RDONLY | O_DIRECTORY);
	if (custom_fd < 0) {
		printf("open(custom_fd) failed, aborting: %d (%s)\n", errno, strerror(errno));
		return 1;
	}

	// regular file, absolute path
	CHECK(openat(custom_fd, "/bin/ls", O_RDONLY));

	// regular file, relative path
	CHECK(openat(custom_fd, "ls", O_RDONLY));

	// symlink in the middle, absolute path
	CHECK(openat(custom_fd, "/proc/self/status", O_RDONLY));

	// symlink in the middle, relative path
	CHECK(openat(custom_fd, "../../../../../../../../../../../../../proc/self/status", O_RDONLY));


	// OPEN(O_PATH)

	CHECK(openat(custom_fd, "../../../../../../../../../../../../../proc/self/status", O_RDONLY | O_PATH));


	// long path
	int segments = 400;
	if (argc == 2)
		segments = atoi(argv[1]);

//	char buf[3 * segments + sizeof("/bin/ls")] = {0};

	char *buf = calloc(3 * segments + sizeof("/bin/ls"), 1);

	for (int i = 0; i < segments; i++)
		strncpy(buf + i*3, "../", 3);

	strncpy(buf + segments*3, "/bin/ls", sizeof("/bin/ls"));

	CHECK(openat(AT_FDCWD, buf, O_RDONLY));

	return 0;
}
