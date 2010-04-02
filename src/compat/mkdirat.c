#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include "dir_mutex.h"

int mkdirat(int dirfd, const char *pathname, mode_t mode)
{
	int rc;

	if(pthread_mutex_lock(&dir_mutex) < 0) {
		perror("pthread_mutex_lock");
		return -1;
	}

	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_unlock;
	}
	rc = mkdir(pathname, mode);
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
