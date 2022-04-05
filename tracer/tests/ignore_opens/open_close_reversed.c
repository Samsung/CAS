#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {
	int fds[5];
	for (int i = 0; i < 5; i++) {
		fds[i] = open("/dev/null", O_RDONLY);
		printf(" %d ", fds[i]);
	}
	for (int i = 5; i != 0; i--) {
		close(fds[i-1]);
	}
	printf("\n");
}
