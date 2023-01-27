#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {

	for (int i = 0; i < 4; i++) {
		int fd = open("/dev/null", O_RDONLY);
		printf(" %d ", fd);
		close(fd+100);
		close(fd);
	}
	for (int i = 0; i < 4; i++) {
		int fd = open("/dev/zero", O_RDONLY);
		printf(" %d ", fd);
		close(fd+100);
		close(fd);
	}
	printf("\n");
}
