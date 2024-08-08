#define _GNU_SOURCE
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/prctl.h>

void* thread_fn(void* arg) {
	char new_pt_name[] = "123456789012345";

	sleep(1);

	int ret = pthread_setname_np(pthread_self(), new_pt_name);
	if (ret) {
		printf("pthread_setname_np() from thread_fn failed, ret=%d\n", ret);
	}
	return NULL;
}

int main(int argc, char** argv) {

	pthread_t new_thread;

	int ret = pthread_create(&new_thread, NULL, thread_fn, NULL);
	if (ret) {
		printf("pthread_create() failed, ret=%d\n", ret);
		return 1;
	}

	char new_pt_name[] = "new_name";
	ret = pthread_setname_np(new_thread, new_pt_name);
	if (ret) {
		printf("pthread_setname_np() failed, ret=%d\n", ret);
	}

	ret = pthread_join(new_thread, NULL);
	if (ret) {
		printf("pthread_join() failed, ret=%d\n", ret);
	}

	return ret;
}
