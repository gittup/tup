#include "dir_mutex.h"
#include "tup/compat.h"
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

pthread_mutex_t dir_mutex;
int dir_mutex_enabled = 1;

int compat_init(void)
{
	if(pthread_mutex_init(&dir_mutex, NULL) < 0)
		return -1;
	return 0;
}

void compat_lock_enable(void)
{
	dir_mutex_enabled = 1;
}

void compat_lock_disable(void)
{
	dir_mutex_enabled = 0;
}

void dir_mutex_lock(int dfd)
{
	if(dir_mutex_enabled)
		pthread_mutex_lock(&dir_mutex);

	if(fchdir(dfd) < 0) {
		perror("fchdir");
		fprintf(stderr, "tup error: Failed to fchdir in a compat wrapper function.\n");
		exit(1);
	}
}

void dir_mutex_unlock(void)
{
	if(dir_mutex_enabled)
		pthread_mutex_unlock(&dir_mutex);
}
