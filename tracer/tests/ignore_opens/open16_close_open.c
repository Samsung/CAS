#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
	int fds[16];
	for (int i = 0; i < 16; i++) {
		fds[i] = open("/dev/null", O_RDONLY);
		printf(" %d ", fds[i]);
	}
	printf("\n");
	close(fds[0]);
	open("/dev/null", O_RDONLY); // fd intentionally not closed explicitly
	for (int i = 1; i < 16; i++) {
		close(fds[i]);
	}
}
