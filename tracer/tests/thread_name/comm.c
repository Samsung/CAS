#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <sys/prctl.h>

int main(int argc, char** argv) {

	char new_name[] = "new_prog_name";
	int ret = prctl(PR_SET_NAME, new_name);
	if (ret != 0) {
		perror("prctl() failed");
		return 1;
	}

	char new_pt_name[] = "new_pt_name";
	ret = pthread_setname_np(pthread_self(), new_pt_name);
	if (ret != 0) {
		printf("pthread_setname_np() failed, ret=%d", ret);
		return 1;
	}

	return 0;
}
