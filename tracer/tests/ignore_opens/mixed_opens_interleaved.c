#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {

	int fds[4];
	for (int i = 0; i < 4; i++) {
		fds[i] = open("/dev/null", O_RDONLY);
		printf(" %d ", fds[i]);
	}
	for (int i = 0; i < 4; i++) {
		int fd = open("/dev/zero", O_RDONLY);
		printf(" %d ", fd);
		close(fd);
	}
	for (int i = 0; i < 4; i++)
		close(fds[i]);
	printf("\n");
}
