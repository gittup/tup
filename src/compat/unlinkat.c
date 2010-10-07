#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include "dir_mutex.h"

int unlinkat(int dirfd, const char *pathname, int flags)
{
	int rc;

	if(flags != 0) {
		fprintf(stderr, "tup compat unlinkat error: flags=%i not supported\n", flags);
		return -1;
	}

	pthread_mutex_lock(&dir_mutex);

	if(fchdir(dirfd) < 0) {
		perror("fchdir");
		goto err_unlock;
	}
	rc = unlink(pathname);
	pthread_mutex_unlock(&dir_mutex);
	return rc;

err_unlock:
	pthread_mutex_unlock(&dir_mutex);
	return -1;
}
