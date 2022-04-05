#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>

int main(int argc, char** argv) {

	for (int i = 0; i < 50; i++) {
		int fd = open("/dev/null", O_RDONLY);
		printf(" %d ", fd);
		close(fd);
	}
	printf("\n");
}
