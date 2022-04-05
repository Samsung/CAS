#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
	int fds[50];
	for (int i = 0; i < 50; i++) {
		fds[i] = open("/dev/null", O_RDONLY);
		printf(" %d ", fds[i]);
	}
	for (int i = 0; i < 50; i++) {
		close(fds[i]);
	}
	printf("\n");
}
