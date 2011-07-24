#include "dir_mutex.h"
#include "tup/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t dir_mutex;

int compat_init(void)
{
	if(pthread_mutex_init(&dir_mutex, NULL) < 0)
		return -1;
	return 0;
}

void dir_mutex_lock(int dfd)
{
	pthread_mutex_lock(&dir_mutex);

	if(fchdir(dfd) < 0) {
		perror("fchdir");
		fprintf(stderr, "tup error: Failed to fchdir in a compat wrapper function.\n");
		exit(1);
	}
}

void dir_mutex_unlock(void)
{
	int olderrno = errno;
	pthread_mutex_unlock(&dir_mutex);
	errno = olderrno;
}
